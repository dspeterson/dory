/* <dory/metadata_fetcher.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   Implements <dory/metadata_fetcher.h>
 */

#include <dory/metadata_fetcher.h>

#include <algorithm>
#include <cstddef>
#include <system_error>

#include <poll.h>

#include <base/counter.h>
#include <base/error_util.h>
#include <base/io_util.h>
#include <base/no_default_case.h>
#include <base/system_error_codes.h>
#include <base/time_util.h>
#include <base/wr/fd_util.h>
#include <dory/kafka_proto/request_response.h>
#include <dory/util/connect_to_host.h>
#include <dory/util/poll_array.h>
#include <log/log.h>
#include <socket/db/error.h>

using namespace Base;
using namespace Dory;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Metadata;
using namespace Dory::Util;
using namespace Log;

DEFINE_COUNTER(BadMetadataResponse);
DEFINE_COUNTER(BadMetadataResponseSize);
DEFINE_COUNTER(MetadataHasEmptyBrokerList);
DEFINE_COUNTER(MetadataHasEmptyTopicList);
DEFINE_COUNTER(MetadataResponseReadLostTcpConnection);
DEFINE_COUNTER(MetadataResponseReadSuccess);
DEFINE_COUNTER(MetadataResponseReadTimeout);
DEFINE_COUNTER(SendMetadataRequestFail);
DEFINE_COUNTER(SendMetadataRequestLostTcpConnection);
DEFINE_COUNTER(SendMetadataRequestSuccess);
DEFINE_COUNTER(SendMetadataRequestUnexpectedEnd);
DEFINE_COUNTER(ShortMetadataResponse);
DEFINE_COUNTER(StartSendMetadataRequest);

static std::vector<uint8_t>
CreateMetadataRequest(const TMetadataProtocol &metadata_protocol) {
  std::vector<uint8_t> result;
  metadata_protocol.WriteAllTopicsMetadataRequest(result, 0);
  return result;
}

TMetadataFetcher::TMetadataFetcher(const TMetadataProtocol *metadata_protocol)
    : MetadataProtocol(metadata_protocol),
      MetadataRequest(CreateMetadataRequest(*metadata_protocol)),
      /* Note: The max message body size value is a loose upper bound to guard
         against a response with a ridiculously large size field. */
      StreamReader(false, true, 4 * 1024 * 1024, 64 * 1024) {
  static_assert(sizeof(TStreamReaderType::TSizeFieldType) ==
      REQUEST_OR_RESPONSE_SIZE_SIZE, "Wrong size field size for StreamReader");
}

bool TMetadataFetcher::Connect(const char *host_name, in_port_t port) {
  assert(this);
  Disconnect();

  try {
    ConnectToHost(host_name, port, Sock);
  } catch (const std::system_error &x) {
    LOG(TPri::ERR) << "Failed to connect to host " << host_name << " port "
        << port << " for metadata: " << x.what();
    assert(!Sock.IsOpen());
    return false;
  } catch (const Socket::Db::TError &x) {
    LOG(TPri::ERR) << "Failed to connect to host " << host_name << " port "
        << port << " for metadata: " << x.what();
    assert(!Sock.IsOpen());
    return false;
  }

  if (!Sock.IsOpen()) {
    return false;
  }

  StreamReader.Reset(Sock);
  return true;
}

std::unique_ptr<TMetadata> TMetadataFetcher::Fetch(int timeout_ms) {
  assert(this);

  if (!Sock.IsOpen()) {
    Die("Must connect to host before getting metadata");
  }

  std::unique_ptr<TMetadata> result;

  if (!SendRequest(MetadataRequest, timeout_ms) || !ReadResponse(timeout_ms)) {
    return result;
  }

  assert(StreamReader.GetState() == TStreamMsgReader::TState::MsgReady);
  std::vector<uint8_t> response;
  size_t response_size = StreamReader.GetReadyMsgSize();

  if (response_size == 0) {
    BadMetadataResponse.Increment();
    LOG(TPri::ERR) << "Got empty metadata response while getting metadata";
    return result;
  }

  const uint8_t *response_begin = StreamReader.GetReadyMsg();
  response.assign(response_begin, response_begin + response_size);
  StreamReader.ConsumeReadyMsg();

  try {
    result = MetadataProtocol->BuildMetadataFromResponse(&response[0],
        response_size);
  } catch (const TMetadataProtocol::TBadMetadataResponse &x) {
    BadMetadataResponse.Increment();
    LOG(TPri::ERR) << "Failed to parse metadata response: " << x.what();
    return result;
  }

  bool bad_metadata = false;

  if (result->GetBrokers().empty()) {
    MetadataHasEmptyBrokerList.Increment();
    bad_metadata = true;
  }

  if (result->GetTopics().empty()) {
    /* Note: It's ok if no topics exist, since that's the initial state of a
       newly provisioned broker cluster.  If automatic topic creation is
       enabled, receipt of a message will cause us to create its topic before
       we route the message to a broker.  Otherwise we will discard all
       messages until a topic is created (i.e. by a sysadmin). */
    MetadataHasEmptyTopicList.Increment();
  }

  if (bad_metadata) {
    LOG(TPri::ERR) << "Bad metadata response: broker count "
        << result->GetBrokers().size() << " topic count "
        << result->GetTopics().size();
    result.reset();
  }

  return result;
}

TMetadataFetcher::TTopicAutocreateResult
TMetadataFetcher::TopicAutocreate(const char *topic, int timeout_ms) {
  assert(this);

  if (!Sock.IsOpen()) {
    Die("Must connect to host before getting metadata");
  }

  std::vector<uint8_t> request;
  MetadataProtocol->WriteSingleTopicMetadataRequest(request, topic, 0);

  if (!SendRequest(request, timeout_ms) || !ReadResponse(timeout_ms)) {
    return TTopicAutocreateResult::TryOtherBroker;
  }

  assert(StreamReader.GetState() == TStreamMsgReader::TState::MsgReady);
  std::vector<uint8_t> response;
  size_t response_size = StreamReader.GetReadyMsgSize();

  if (response_size == 0) {
    BadMetadataResponse.Increment();
    LOG(TPri::ERR) << "Got empty metadata response during topic autocreate";
    return TTopicAutocreateResult::Fail;
  }

  const uint8_t *response_begin = StreamReader.GetReadyMsg();
  response.assign(response_begin, response_begin + response_size);
  StreamReader.ConsumeReadyMsg();
  bool success = false;

  try {
    success = MetadataProtocol->TopicAutocreateWasSuccessful(topic,
        &response[0], response_size);
  } catch (const TMetadataProtocol::TBadMetadataResponse &x) {
    BadMetadataResponse.Increment();
    LOG(TPri::ERR) << "Failed to parse metadata response: " << x.what();
    return TTopicAutocreateResult::TryOtherBroker;
  }

  return success ? TTopicAutocreateResult::Success :
                   TTopicAutocreateResult::Fail;
}

bool TMetadataFetcher::SendRequest(const std::vector<uint8_t> &request,
    int timeout_ms) {
  assert(this);
  StartSendMetadataRequest.Increment();

  try {
    if (!TryWriteExactly(Sock, &request[0], request.size(), timeout_ms)) {
      SendMetadataRequestFail.Increment();
      LOG(TPri::ERR) << "Failed to send metadata request";
      return false;
    }
  } catch (const std::system_error &x) {
    if (LostTcpConnection(x)) {
      SendMetadataRequestLostTcpConnection.Increment();
      LOG(TPri::ERR)
          << "Lost TCP connection to broker while trying to send metadata "
          << "request: " << x.what();
      return false;
    }

    throw;  // anything else is fatal
  } catch (const TUnexpectedEnd &) {
    SendMetadataRequestUnexpectedEnd.Increment();
    LOG(TPri::ERR)
        << "Lost TCP connection to broker while trying to send metadata "
        << "request";
    return false;
  }

  SendMetadataRequestSuccess.Increment();
  return true;
}

enum class TReadResponsePollItem {
  SockIo = 0
};  // TReadResponsePollItem

bool TMetadataFetcher::ReadResponse(int timeout_ms) {
  assert(this);
  TPollArray<TReadResponsePollItem, 1> poll_array;
  struct pollfd &sock_item = poll_array[TReadResponsePollItem::SockIo];
  sock_item.events = POLLIN;
  sock_item.fd = StreamReader.GetFd();
  uint64_t start_time = GetMonotonicRawMilliseconds();
  uint64_t current_time = start_time;
  uint64_t elapsed = 0;

  for (; ; ) {
    int time_left = (timeout_ms < 0) ?
        -1 : (timeout_ms - static_cast<int>(elapsed));

    /* Treat EINTR as fatal, since this thread should have signals masked. */
    const int ret = Wr::poll(Wr::TDisp::AddFatal, {EINTR}, poll_array,
        poll_array.Size(), time_left);
    assert(ret >= 0);

    if (ret == 0) {
      MetadataResponseReadTimeout.Increment();
      return false;
    }

    try {
      if (StreamReader.Read() != TStreamMsgReader::TState::ReadNeeded) {
        break;
      }
    } catch (const std::system_error &x) {
      if (LostTcpConnection(x)) {
        MetadataResponseReadLostTcpConnection.Increment();
        LOG(TPri::ERR)
            << "Lost TCP connection to broker while trying to read metadata "
            << "response: " << x.what();
        return false;
      }

      std::string msg("Failed to read metadata response: ");
      AppendStrerror(x.code().value(), msg);
      Die(msg.c_str());
    }

    current_time = GetMonotonicRawMilliseconds();
    elapsed = current_time - start_time;

    if ((timeout_ms >= 0) && (elapsed >= static_cast<uint64_t>(timeout_ms))) {
      return false;
    }

    sock_item.revents = 0;
  }

  switch (StreamReader.GetState()) {
    case TStreamMsgReader::TState::ReadNeeded: {
      Die("TMetadataFetcher internal error in ReadResponse()");
    }
    case TStreamMsgReader::TState::MsgReady: {
      break;
    }
    case TStreamMsgReader::TState::DataInvalid: {
      BadMetadataResponseSize.Increment();
      LOG(TPri::ERR) << "Router thread got bad metadata response size";
      return false;
    }
    case TStreamMsgReader::TState::AtEnd: {
      ShortMetadataResponse.Increment();
      LOG(TPri::ERR) << "Router thread got short metadata response";
      return false;
    }
    NO_DEFAULT_CASE;
  }

  MetadataResponseReadSuccess.Increment();
  return true;
}
