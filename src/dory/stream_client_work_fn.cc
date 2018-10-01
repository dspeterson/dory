/* <dory/stream_client_work_fn.cc>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/stream_client_work_fn.h>.
 */

#include <dory/stream_client_work_fn.h>

#include <cassert>
#include <system_error>
#include <utility>

#include <poll.h>
#include <syslog.h>

#include <base/error_utils.h>
#include <base/no_default_case.h>
#include <dory/input_dg/input_dg_util.h>
#include <dory/msg.h>
#include <dory/util/poll_array.h>
#include <dory/util/system_error_codes.h>
#include <dory/util/time_util.h>
#include <server/counter.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Util;
using namespace Thread;

SERVER_COUNTER(NewTcpClient);
SERVER_COUNTER(NewUnixClient);
SERVER_COUNTER(TcpInputCleanDisconnect);
SERVER_COUNTER(TcpInputForwardMsg);
SERVER_COUNTER(TcpInputInvalidSizeField);
SERVER_COUNTER(TcpInputMsgBodyTooLarge);
SERVER_COUNTER(TcpInputSocketError);
SERVER_COUNTER(TcpInputSocketGotData);
SERVER_COUNTER(TcpInputSocketRead);
SERVER_COUNTER(TcpInputUncleanDisconnect);
SERVER_COUNTER(UnixStreamInputCleanDisconnect);
SERVER_COUNTER(UnixStreamInputForwardMsg);
SERVER_COUNTER(UnixStreamInputInvalidSizeField);
SERVER_COUNTER(UnixStreamInputMsgBodyTooLarge);
SERVER_COUNTER(UnixStreamInputSocketError);
SERVER_COUNTER(UnixStreamInputSocketGotData);
SERVER_COUNTER(UnixStreamInputSocketRead);
SERVER_COUNTER(UnixStreamInputUncleanDisconnect);

TStreamClientWorkFn &TStreamClientWorkFn::TStreamClientWorkFn::operator=(
    nullptr_t) noexcept {
  assert(this);
  IsTcp = false;
  Config = nullptr;
  Pool = nullptr;
  MsgStateTracker = nullptr;
  AnomalyTracker = nullptr;
  OutputQueue = nullptr;
  ShutdownRequestFd = nullptr;
  ClientSocket.Reset();
  StreamReader.Reset();
  return *this;
}

void TStreamClientWorkFn::operator()() {
  assert(this);
  assert(Config);
  assert(Pool);
  assert(MsgStateTracker);
  assert(AnomalyTracker);
  assert(OutputQueue);
  assert(ShutdownRequestFd);
  assert(ClientSocket >= 0);
  assert(StreamReader.GetFd() == ClientSocket);

  enum class t_poll_item {
    Sock = 0,
    ShutdownRequest = 1
  };  // t_poll_item

  if (IsTcp) {
    NewTcpClient.Increment();
  } else {
    NewUnixClient.Increment();
  }

  TPollArray<t_poll_item, 2> poll_array;
  struct pollfd &sock_item = poll_array[t_poll_item::Sock];
  struct pollfd &shutdown_item = poll_array[t_poll_item::ShutdownRequest];
  sock_item.fd = ClientSocket;
  sock_item.events = POLLIN;
  shutdown_item.fd = *ShutdownRequestFd;
  shutdown_item.events = POLLIN;

  do {
    poll_array.ClearRevents();

    /* Don't check for EINTR, since this thread has signals masked. */
    int ret = IfLt0(poll(poll_array, poll_array.Size(), -1));
    assert(ret > 0);
    assert(sock_item.revents || shutdown_item.revents);
  } while (!shutdown_item.revents && HandleSockReadReady());
}

void TStreamClientWorkFn::SetState(bool is_tcp, const TConfig &config,
    TPool &pool, TMsgStateTracker &msg_state_tracker,
    TAnomalyTracker &anomaly_tracker, TGatePutApi<TMsg::TPtr> &output_queue,
    const TFd &shutdown_request_fd, TFd &&client_socket) noexcept {
  assert(this);
  IsTcp = is_tcp;
  Config = &config;
  Pool = &pool;
  MsgStateTracker = &msg_state_tracker;
  AnomalyTracker = &anomaly_tracker;
  OutputQueue = &output_queue;
  ShutdownRequestFd = &shutdown_request_fd;
  ClientSocket = std::move(client_socket);
  StreamReader.Reset(ClientSocket);
  StreamReader.SetMaxMsgBodySize(config.MaxStreamInputMsgSize);
}

void TStreamClientWorkFn::HandleClientClosed() const {
  if (StreamReader.GetDataSize() == 0) {
    if (IsTcp) {
      TcpInputCleanDisconnect.Increment();
    } else {
      UnixStreamInputCleanDisconnect.Increment();
    }
  } else {
    const uint8_t *data_begin = StreamReader.GetData();
    const uint8_t *data_end = data_begin + StreamReader.GetDataSize();
    AnomalyTracker->TrackStreamClientUncleanDisconnect(IsTcp, data_begin,
        data_end);

    if (IsTcp) {
      TcpInputUncleanDisconnect.Increment();
      static TLogRateLimiter lim(std::chrono::seconds(30));

      if (lim.Test()) {
        syslog(LOG_WARNING,
            "TCP client disconnected after writing incomplete message");
      }
    } else {
      UnixStreamInputUncleanDisconnect.Increment();
      static TLogRateLimiter lim(std::chrono::seconds(30));

      if (lim.Test()) {
        syslog(LOG_WARNING,
            "UNIX stream client disconnected after writing incomplete message"
        );
      }
    }
  }
}

void TStreamClientWorkFn::HandleDataInvalid() {
  assert(this);
  assert(StreamReader.GetDataInvalidReason().IsKnown());

  switch (*StreamReader.GetDataInvalidReason()) {
    case TStreamMsgWithSizeReaderBase::TDataInvalidReason::InvalidSizeField: {
      if (IsTcp) {
        TcpInputInvalidSizeField.Increment();
        static TLogRateLimiter lim(std::chrono::seconds(30));

        if (lim.Test()) {
          syslog(LOG_ERR, "Got TCP input message with invalid size");
        }
      } else {
        UnixStreamInputInvalidSizeField.Increment();
        static TLogRateLimiter lim(std::chrono::seconds(30));

        if (lim.Test()) {
          syslog(LOG_ERR, "Got UNIX stream input message with invalid size");
        }
      }

      break;
    }
    case TStreamMsgWithSizeReaderBase::TDataInvalidReason::MsgBodyTooLarge: {
      if (IsTcp) {
        TcpInputMsgBodyTooLarge.Increment();
        static TLogRateLimiter lim(std::chrono::seconds(30));

        if (lim.Test()) {
          syslog(LOG_ERR, "Got too large TCP input message");
        }
      } else {
        UnixStreamInputMsgBodyTooLarge.Increment();
        static TLogRateLimiter lim(std::chrono::seconds(30));

        if (lim.Test()) {
          syslog(LOG_ERR, "Got too large UNIX stream input message");
        }
      }

      break;
    }
    NO_DEFAULT_CASE;
  }

  const uint8_t *data_begin = StreamReader.GetData();
  const uint8_t *data_end = data_begin + StreamReader.GetDataSize();
  AnomalyTracker->TrackMalformedMsgDiscard(data_begin, data_end);
}

bool TStreamClientWorkFn::HandleSockReadReady() {
  assert(this);

  if (IsTcp) {
    TcpInputSocketRead.Increment();
  } else {
    UnixStreamInputSocketRead.Increment();
  }

  TStreamMsgReader::TState reader_state = TStreamMsgReader::TState::AtEnd;

  try {
    reader_state = StreamReader.Read();
  } catch (const std::system_error &x) {
    if (LostTcpConnection(x)) {
      if (IsTcp) {
        TcpInputSocketError.Increment();
      } else {
        UnixStreamInputSocketError.Increment();
      }

      syslog(LOG_ERR, "%s input thread lost client connection: %s",
          IsTcp ? "TCP" : "UNIX stream", x.what());
      return false;
    }

    throw;  // anything else is fatal
  }

  do {
    switch (reader_state) {
      case TStreamMsgReader::TState::ReadNeeded: {
        break;
      }
      case TStreamMsgReader::TState::MsgReady: {
        TMsg::TPtr msg = InputDg::BuildMsgFromDg(StreamReader.GetReadyMsg(),
            StreamReader.GetReadyMsgSize(), *Config, *Pool, *AnomalyTracker,
            *MsgStateTracker);

        if (msg) {
          OutputQueue->Put(std::move(msg));

          if (IsTcp) {
            TcpInputForwardMsg.Increment();
          } else {
            UnixStreamInputForwardMsg.Increment();
          }
        }

        reader_state = StreamReader.ConsumeReadyMsg();
        break;
      }
      case TStreamMsgReader::TState::DataInvalid: {
        HandleDataInvalid();
        return false;
      }
      case TStreamMsgReader::TState::AtEnd: {
        HandleClientClosed();
        return false;
      }
      NO_DEFAULT_CASE;
    }

    if (IsTcp) {
      TcpInputSocketGotData.Increment();
    } else {
      UnixStreamInputSocketGotData.Increment();
    }
  } while (reader_state == TStreamMsgReader::TState::MsgReady);

  return true;
}
