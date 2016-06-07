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
#include <utility>

#include <poll.h>
#include <syslog.h>

#include <base/error_utils.h>
#include <base/no_default_case.h>
#include <dory/input_dg/input_dg_util.h>
#include <dory/msg.h>
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
SERVER_COUNTER(TcpInputLongSizeField);
SERVER_COUNTER(TcpInputShortSizeField);
SERVER_COUNTER(TcpInputSocketError);
SERVER_COUNTER(TcpInputSocketGotData);
SERVER_COUNTER(TcpInputSocketRead);
SERVER_COUNTER(TcpInputUncleanDisconnect);
SERVER_COUNTER(UnixStreamInputCleanDisconnect);
SERVER_COUNTER(UnixStreamInputForwardMsg);
SERVER_COUNTER(UnixStreamInputLongSizeField);
SERVER_COUNTER(UnixStreamInputShortSizeField);
SERVER_COUNTER(UnixStreamInputSocketError);
SERVER_COUNTER(UnixStreamInputSocketGotData);
SERVER_COUNTER(UnixStreamInputSocketRead);
SERVER_COUNTER(UnixStreamInputUncleanDisconnect);

TStreamClientWorkFn::TStreamClientWorkFn(nullptr_t) noexcept
    : IsTcp(false),
      Config(nullptr),
      Pool(nullptr),
      MsgStateTracker(nullptr),
      AnomalyTracker(nullptr),
      OutputQueue(nullptr),
      ShutdownRequestFd(nullptr) {
}

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
  ReceiveBuf.Clear();
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

  if (IsTcp) {
    NewTcpClient.Increment();
  } else {
    NewUnixClient.Increment();
  }

  struct pollfd &sock_item = MainLoopPollArray[TMainLoopPollItem::Sock];
  struct pollfd &shutdown_item =
      MainLoopPollArray[TMainLoopPollItem::ShutdownRequest];
  sock_item.fd = ClientSocket;
  sock_item.events = POLLIN;
  shutdown_item.fd = *ShutdownRequestFd;
  shutdown_item.events = POLLIN;

  do {
    MainLoopPollArray.ClearRevents();

    /* Don't check for EINTR, since this thread has signals masked. */
    int ret = IfLt0(poll(MainLoopPollArray, MainLoopPollArray.Size(), -1));
    assert(ret > 0);
    assert(sock_item.revents || shutdown_item.revents);
  } while (sock_item.revents && HandleSockReadReady());
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
}

TStreamClientWorkFn::TSockReadStatus
TStreamClientWorkFn::DoSockRead(size_t min_size) {
  assert(this);
  assert(min_size);

  if (IsTcp) {
    TcpInputSocketRead.Increment();
  } else {
    UnixStreamInputSocketRead.Increment();
  }

  ReceiveBuf.EnsureSpace(min_size);
  ssize_t result = 0;

  try {
    result = IfLt0(recv(ClientSocket, ReceiveBuf.Space(),
        ReceiveBuf.SpaceSize(), 0));
  } catch (const std::system_error &x) {
    if (LostTcpConnection(x)) {
      if (IsTcp) {
        TcpInputSocketError.Increment();
      } else {
        UnixStreamInputSocketError.Increment();
      }

      syslog(LOG_ERR, "%s input thread lost client connection: %s",
          IsTcp ? "TCP" : "UNIX stream", x.what());
      return TSockReadStatus::Error;
    }

    throw;  // anything else is fatal
  }

  assert(result >= 0);

  if (result == 0) {
    return TSockReadStatus::ClientClosed;
  }

  /* Read was successful, although the amount of data obtained may be less than
     what the caller hoped for. */
  ReceiveBuf.MarkSpaceConsumed(result);

  if (IsTcp) {
    TcpInputSocketGotData.Increment();
  } else {
    UnixStreamInputSocketGotData.Increment();
  }

  return TSockReadStatus::Open;
}

void TStreamClientWorkFn::HandleShortMsgSize(size_t msg_size) {
  assert(this);

  if (IsTcp) {
    TcpInputShortSizeField.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(30));

    if (lim.Test()) {
      syslog(LOG_ERR, "Got TCP input message with short size: %lu",
          static_cast<unsigned long>(msg_size));
    }
  } else {
    UnixStreamInputShortSizeField.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(30));

    if (lim.Test()) {
      syslog(LOG_ERR, "Got UNIX stream input message with short size: %lu",
          static_cast<unsigned long>(msg_size));
    }
  }

  const uint8_t *data_begin = reinterpret_cast<const uint8_t *>(
      ReceiveBuf.Data());
  const uint8_t *data_end = data_begin + ReceiveBuf.DataSize();
  AnomalyTracker->TrackMalformedMsgDiscard(data_begin, data_end);
}

void TStreamClientWorkFn::HandleLongMsgSize(size_t msg_size) {
  assert(this);

  if (IsTcp) {
    TcpInputLongSizeField.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(30));

    if (lim.Test()) {
      syslog(LOG_ERR, "Disconnecting on large TCP input message: %lu",
          static_cast<unsigned long>(msg_size));
    }
  } else {
    UnixStreamInputLongSizeField.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(30));

    if (lim.Test()) {
      syslog(LOG_ERR,
          "Disconnecting on large UNIX stream input message: %lu",
          static_cast<unsigned long>(msg_size));
    }
  }

  const uint8_t *data_begin = reinterpret_cast<const uint8_t *>(
      ReceiveBuf.Data());
  const uint8_t *data_end = data_begin + ReceiveBuf.DataSize();
  AnomalyTracker->TrackMalformedMsgDiscard(data_begin, data_end);
}

bool TStreamClientWorkFn::CheckMsgSize(int32_t msg_size) {
  assert(this);

  if (msg_size < static_cast<int32_t>(SIZE_FIELD_SIZE)) {
    HandleShortMsgSize(msg_size);
    return false;
  }

  if (static_cast<size_t>(msg_size) > Config->MaxStreamInputMsgSize) {
    HandleLongMsgSize(msg_size);
    return false;
  }

  return true;
}

TStreamClientWorkFn::TMsgReadResult
TStreamClientWorkFn::TryReadMsgData() {
  assert(this);
  bool did_read = false;

  /* Move any message data to front of buffer.  This improves efficiency by
     maximizing read size. */
  ReceiveBuf.MoveDataToFront();

  if (ReceiveBuf.DataSize() < SIZE_FIELD_SIZE) {
    switch (DoSockRead(SIZE_FIELD_SIZE - ReceiveBuf.DataSize())) {
      case TSockReadStatus::Open: {
        break;
      }
      case TSockReadStatus::ClientClosed: {
        return TMsgReadResult::ClientClosed;
      }
      case TSockReadStatus::Error: {
        return TMsgReadResult::Error;
      }
      NO_DEFAULT_CASE;
    }

    did_read = true;

    if (ReceiveBuf.DataSize() < SIZE_FIELD_SIZE) {
      return TMsgReadResult::NeedMoreData;  // try again later
    }
  }

  /* The size field is always first, and gives the size of the entire message,
     including the size field. */
  int32_t msg_size = ReadSizeField();

  if (!CheckMsgSize(msg_size)) {
    return TMsgReadResult::Error;
  }

  if (ReceiveBuf.DataSize() >= static_cast<size_t>(msg_size)) {
    return TMsgReadResult::MsgReady;
  }

  if (did_read) {
    return TMsgReadResult::NeedMoreData;  // try again later
  }

  switch (DoSockRead(msg_size - ReceiveBuf.DataSize())) {
    case TSockReadStatus::Open: {
      break;
    }
    case TSockReadStatus::ClientClosed: {
      return TMsgReadResult::ClientClosed;
    }
    case TSockReadStatus::Error: {
      return TMsgReadResult::Error;
    }
    NO_DEFAULT_CASE;
  }

  return (ReceiveBuf.DataSize() >= static_cast<size_t>(msg_size)) ?
      TMsgReadResult::MsgReady : TMsgReadResult::NeedMoreData;
}

bool TStreamClientWorkFn::TryProcessMsgData() {
  assert(this);

  /* On entry, we know we have at least one complete message.  Process that
     message and any additional complete messages now available. */

  int32_t msg_size = ReadSizeField();
  assert((msg_size >= static_cast<int32_t>(SIZE_FIELD_SIZE)) &&
      (static_cast<size_t>(msg_size) <= Config->MaxStreamInputMsgSize));

  do {
    char * const msg_begin = reinterpret_cast<char *>(&ReceiveBuf.Data()[0]);
    TMsg::TPtr msg = InputDg::BuildMsgFromDg(msg_begin, msg_size, *Config,
        *Pool, *AnomalyTracker, *MsgStateTracker);

    if (msg) {
      OutputQueue->Put(std::move(msg));

      if (IsTcp) {
        TcpInputForwardMsg.Increment();
      } else {
        UnixStreamInputForwardMsg.Increment();
      }
    }

    ReceiveBuf.MarkDataConsumed(msg_size);

    if (ReceiveBuf.DataSize() < SIZE_FIELD_SIZE) {
      break;
    }

    msg_size = ReadSizeField();

    if (!CheckMsgSize(msg_size)) {
      return false;
    }
  } while (ReceiveBuf.DataSize() >= static_cast<size_t>(msg_size));

  return true;
}

void TStreamClientWorkFn::HandleClientClosed() const {
  if (ReceiveBuf.DataIsEmpty()) {
    if (IsTcp) {
      TcpInputCleanDisconnect.Increment();
    } else {
      UnixStreamInputCleanDisconnect.Increment();
    }
  } else {
    const uint8_t *data_begin = ReceiveBuf.Data();
    const uint8_t *data_end = data_begin + ReceiveBuf.DataSize();
    AnomalyTracker->TrackStreamClientUncleanDisconnect(IsTcp, data_begin,
        data_end);

    if (IsTcp) {
      TcpInputUncleanDisconnect.Increment();
      static TLogRateLimiter lim(std::chrono::seconds(30));

      if (lim.Test()) {
        syslog(LOG_WARNING, "TCP client disconnected after writing "
            "incomplete message");
      }
    } else {
      UnixStreamInputUncleanDisconnect.Increment();
      static TLogRateLimiter lim(std::chrono::seconds(30));

      if (lim.Test()) {
        syslog(LOG_WARNING, "UNIX stream client disconnected after "
            "writing incomplete message");
      }
    }
  }
}

bool TStreamClientWorkFn::HandleSockReadReady() {
  assert(this);

  switch (TryReadMsgData()) {
    case TMsgReadResult::MsgReady: {
      if (!TryProcessMsgData()) {
        return false;
      }

      break;
    }
    case TMsgReadResult::NeedMoreData: {
      break;
    }
    case TMsgReadResult::ClientClosed: {
      HandleClientClosed();
      return false;
    }
    case TMsgReadResult::Error: {
      return false;
    }
    NO_DEFAULT_CASE;
  }

  return true;
}
