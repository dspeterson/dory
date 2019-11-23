/* <dory/kafka_proto/metadata/v0/mdrequest.cc>

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

   Utility program for sending a metadata request to a Kafka broker and writing
   the response in JSON to standard output.
 */

#include <dory/kafka_proto/metadata/v0/metadata_request_writer.h>
#include <dory/kafka_proto/metadata/v0/metadata_response_reader.h>
#include <dory/kafka_proto/request_response.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <sys/uio.h>

#include <base/basename.h>
#include <base/fd.h>
#include <base/indent.h>
#include <base/io_util.h>
#include <base/no_copy_semantics.h>
#include <dory/build_id.h>
#include <dory/util/connect_to_host.h>
#include <dory/util/invalid_arg_error.h>
#include <rpc/transceiver.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Metadata::V0;
using namespace Dory::Util;
using namespace Rpc;

struct TCmdLineArgs {
  /* Throws TInvalidArgError on error parsing args. */
  TCmdLineArgs(int argc, const char *const argv[]);

  std::string BrokerHost;

  in_port_t BrokerPort = 9092;

  std::string Topic;

  size_t RequestCount = 1;
};  // TCmdLineArgs

static void ParseArgs(int argc, const char *const argv[], TCmdLineArgs &args) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd(
        "Utility for sending a metadata request to a Kafka broker and writing "
        "the response to standard output",
        ' ', dory_build_id);
    ValueArg<decltype(args.BrokerHost)> arg_broker_host("", "broker_host",
        "Kafka broker to connect to.", true, args.BrokerHost, "HOST");
    cmd.add(arg_broker_host);
    ValueArg<decltype(args.BrokerPort)> arg_broker_port("", "broker_port",
        "Port to connect to.", false, args.BrokerPort, "PORT");
    cmd.add(arg_broker_port);
    ValueArg<decltype(args.Topic)> arg_topic("", "topic",
        "Topic to request metadata for.  If omitted, metadata will be "
        "requested for all topics.", false, args.Topic, "TOPIC");
    cmd.add(arg_topic);
    ValueArg<decltype(args.RequestCount)> arg_request_count("",
        "request_count", "Number of requests to send (for testing).", false,
        args.RequestCount, "COUNT");
    cmd.add(arg_request_count);
    cmd.parse(argc, &arg_vec[0]);
    args.BrokerHost = arg_broker_host.getValue();
    args.BrokerPort = arg_broker_port.getValue();
    args.Topic = arg_topic.getValue();
    args.RequestCount = arg_request_count.getValue();
  } catch (const ArgException &x) {
    throw TInvalidArgError(x.error(), x.argId());
  }
}

TCmdLineArgs::TCmdLineArgs(int argc, const char *const argv[]) {
  ParseArgs(argc, argv, *this);
}

class TServerClosedConnection final : public std::runtime_error {
  public:
  TServerClosedConnection()
      : std::runtime_error("Server unexpectedly closed connection") {
  }
};  // TServerClosedConnection

class TCorrelationIdMismatch final : public std::runtime_error {
  public:
  TCorrelationIdMismatch()
      : std::runtime_error("Kafka correlation ID mismatch") {
  }
};  // TCorrelationIdMismatch

static const int32_t CORRELATION_ID = 0;

static void SendRequest(int socket_fd, const std::string &topic) {
  TTransceiver xver;
  struct iovec *vecs = nullptr;
  std::vector<uint8_t> header_buf;

  if (topic.empty()) {  // all topics request
    vecs = xver.GetIoVecs(1);
    header_buf.resize(TMetadataRequestWriter::NumAllTopicsHeaderBytes());
    TMetadataRequestWriter().WriteAllTopicsRequest(*vecs, &header_buf[0],
        CORRELATION_ID);
  } else {  // single topic request
    vecs = xver.GetIoVecs(2);
    header_buf.resize(TMetadataRequestWriter::NumSingleTopicHeaderBytes());
    const char *topic_begin = topic.data();
    const char *topic_end = topic_begin + topic.size();
    TMetadataRequestWriter().WriteSingleTopicRequest(vecs[0], vecs[1],
        &header_buf[0], topic_begin, topic_end, CORRELATION_ID);
  }

  assert(vecs);
  assert(!header_buf.empty());

  for (size_t part = 0; xver; xver += part) {
    part = xver.Send(socket_fd);
  }
}

static void ReadResponse(int socket_fd, std::vector<uint8_t> &response_buf) {
  size_t nbytes = REQUEST_OR_RESPONSE_SIZE_SIZE;
  response_buf.resize(nbytes);

  if (!TryReadExactly(socket_fd, &response_buf[0], nbytes)) {
    throw TServerClosedConnection();
  }

  size_t response_size = GetRequestOrResponseSize(&response_buf[0]);
  response_buf.resize(response_size);
  assert(response_size >= nbytes);

  if (!TryReadExactly(socket_fd, &response_buf[nbytes],
      response_size - nbytes)) {
    throw TServerClosedConnection();
  }

  TMetadataResponseReader reader(&response_buf[0], response_buf.size());

  if (reader.GetCorrelationId() != CORRELATION_ID) {
    throw TCorrelationIdMismatch();
  }
}

class TResponsePrinter final {
  NO_COPY_SEMANTICS(TResponsePrinter);

  public:
  TResponsePrinter(std::ostream &out, const std::vector<uint8_t> &response)
      : Out(out),
        Resp(&response[0], response.size()) {
  }

  void Print();

  private:
  void WriteOneBroker(TIndent &ind0);

  void WriteBrokers(TIndent &ind0);

  void WriteOneReplica(TIndent & ind0);

  void WriteOneCaughtUpReplica(TIndent & ind0);

  void WriteOnePartition(TIndent &ind0);

  void WriteOneTopic(TIndent &ind0);

  void WriteTopics(TIndent &ind0);

  std::ostream &Out;

  TMetadataResponseReader Resp;
};  // TResponsePrinter

void TResponsePrinter::Print() {
  std::string indent_str;
  TIndent ind0(indent_str, TIndent::StartAt::Zero, 4);
  Out << ind0 << "{" << std::endl;
  WriteBrokers(ind0);
  WriteTopics(ind0);
  Out << ind0 << "}" << std::endl;
}

void TResponsePrinter::WriteOneBroker(TIndent &ind0) {
  TIndent ind1(ind0);
  std::string host(Resp.GetCurrentBrokerHostBegin(),
                   Resp.GetCurrentBrokerHostEnd());
  Out << ind1 << "\"node\": " << Resp.GetCurrentBrokerNodeId() << ","
      << std::endl
      << ind1 << "\"host\": \"" << host << "\"," << std::endl
      << ind1 << "\"port\": " << Resp.GetCurrentBrokerPort() << std::endl;
}

void TResponsePrinter::WriteBrokers(TIndent &ind0) {
  TIndent ind1(ind0);
  Out << ind1 << "\"brokers\": [" << std::endl;

  {
    TIndent ind2(ind1);

    if (Resp.NextBroker()) {
      Out << ind2 << "{" << std::endl;
      WriteOneBroker(ind2);
      Out << ind2 << "}";

      while (Resp.NextBroker()) {
        Out << "," << std::endl << ind2 << "{" << std::endl;
        WriteOneBroker(ind2);
        Out << ind2 << "}";
      }

      Out << std::endl;
    }
  }

  Out << ind1 << "]," << std::endl;
}

void TResponsePrinter::WriteOneReplica(TIndent &ind0) {
  TIndent ind1(ind0);
  Out << ind1 << "\"id\": " << Resp.GetCurrentReplicaNodeId() << std::endl;
}

void TResponsePrinter::WriteOneCaughtUpReplica(TIndent & ind0) {
  TIndent ind1(ind0);
  Out << ind1 << "\"id\": " << Resp.GetCurrentCaughtUpReplicaNodeId()
      << std::endl;
}

void TResponsePrinter::WriteOnePartition(TIndent &ind0) {
  TIndent ind1(ind0);
  Out << ind1 << "\"id\": " << Resp.GetCurrentPartitionId() << "," << std::endl
      << ind1 << "\"leader_id\": " << Resp.GetCurrentPartitionLeaderId() << ","
      << std::endl
      << ind1 << "\"error_code: " << Resp.GetCurrentPartitionErrorCode() << ","
      << std::endl
      << ind1 << "\"replicas\": [" << std::endl;

  {
    TIndent ind2(ind1);

    if (Resp.NextReplicaInPartition()) {
      Out << ind2 << "{" << std::endl;
      WriteOneReplica(ind2);
      Out << ind2 << "}";

      while (Resp.NextReplicaInPartition()) {
        Out << "," << std::endl << ind2 << "{" << std::endl;
        WriteOneReplica(ind2);
        Out << ind2 << "}";
      }

      Out << std::endl;
    }
  }

  Out << ind1 << "]," << std::endl
      << ind1 << "\"caught_up_replicas\": [" << std::endl;

  {
    TIndent ind2(ind1);

    if (Resp.NextCaughtUpReplicaInPartition()) {
      Out << ind2 << "{" << std::endl;
      WriteOneCaughtUpReplica(ind2);
      Out << ind2 << "}";

      while (Resp.NextCaughtUpReplicaInPartition()) {
        Out << "," << std::endl << ind2 << "{" << std::endl;
        WriteOneCaughtUpReplica(ind2);
        Out << ind2 << "}";
      }

      Out << std::endl;
    }
  }

  Out << ind1 << "]" << std::endl;
}

void TResponsePrinter::WriteOneTopic(TIndent &ind0) {
  TIndent ind1(ind0);
  std::string name(Resp.GetCurrentTopicNameBegin(),
      Resp.GetCurrentTopicNameEnd());
  Out << ind1 << "\"name\": \"" << name << "\"," << std::endl
      << ind1 << "\"error_code: " << Resp.GetCurrentTopicErrorCode() << ","
      << std::endl
      << ind1 << "\"partitions\": [" << std::endl;

  {
    TIndent ind2(ind1);

    if (Resp.NextPartitionInTopic()) {
      Out << ind2 << "{" << std::endl;
      WriteOnePartition(ind2);
      Out << ind2 << "}";

      while (Resp.NextPartitionInTopic()) {
        Out << "," << std::endl << ind2 << "{" << std::endl;
        WriteOnePartition(ind2);
        Out << ind2 << "}";
      }

      Out << std::endl;
    }
  }

  Out << ind1 << "]" << std::endl;
}

void TResponsePrinter::WriteTopics(TIndent &ind0) {
  TIndent ind1(ind0);
  Out << ind1 << "\"topics\": [" << std::endl;

  {
    TIndent ind2(ind1);

    if (Resp.NextTopic()) {
      Out << ind2 << "{" << std::endl;
      WriteOneTopic(ind2);
      Out << ind2 << "}";

      while (Resp.NextTopic()) {
        Out << "," << std::endl << ind2 << "{" << std::endl;
        WriteOneTopic(ind2);
        Out << ind2 << "}";
      }

      Out << std::endl;
    }
  }

  Out << ind1 << "]" << std::endl;
}

static int mdrequest_main(int argc, const char *const *argv) {
  std::unique_ptr<TCmdLineArgs> args;

  try {
    args.reset(new TCmdLineArgs(argc, argv));
  } catch (const TInvalidArgError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  }

  TFd broker_socket;
  ConnectToHost(args->BrokerHost, args->BrokerPort, broker_socket);

  if (!broker_socket.IsOpen()) {
    std::cerr << "Failed to connect to host " << args->BrokerHost << " port "
        << args->BrokerPort << std::endl;
    return EXIT_FAILURE;
  }

  for (size_t i = 0; i < args->RequestCount; ++i) {
    SendRequest(broker_socket, args->Topic);
    std::vector<uint8_t> response_buf;
    ReadResponse(broker_socket, response_buf);
    std::ostringstream out;
    TResponsePrinter(out, response_buf).Print();
    std::cout << out.str();
  }

  broker_socket.Reset();
  return EXIT_SUCCESS;
}

int main(int argc, const char *const *argv) {
  int ret = EXIT_SUCCESS;

  try {
    ret = mdrequest_main(argc, argv);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    ret = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "error: uncaught unknown exception" << std::endl;
    ret = EXIT_FAILURE;
  }

  return ret;
}
