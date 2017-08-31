/* <base/stream_msg_reader.cc>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

   Implements <base/stream_msg_reader.h>.
 */

#include <base/stream_msg_reader.h>

#include <cerrno>

#include <unistd.h>

#include <base/error_utils.h>
#include <base/no_default_case.h>

using namespace Base;

TStreamMsgReader::TState TStreamMsgReader::Read() {
  assert(this);

  if (State != TState::ReadNeeded) {
    assert(false);
    return State;
  }

  assert(Fd >= 0);
  assert(ReadyMsgOffset == 0);
  assert(ReadyMsgSize == 0);
  assert(!EndOfInput);
  size_t read_size = GetNextReadSize();

  if (read_size == 0) {
    /* In case we get a read size of 0, return here so we don't get a return
       value of 0 from read() and interpret it as end of input.  A typical
       subclass implementation will probably never return 0 from
       GetNextReadSize() but we can't predict how subclasses will be
       implemented. */
    return State;
  }

  Buf.EnsureSpace(read_size);

  /* Note that read() works with TCP and UNIX domain stream sockets, as well as
     standard UNIX pipes. */
  ssize_t ret = read(Fd, Buf.Space(), read_size);

  if (ret < 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlogical-op"
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      return State;
    }
#pragma GCC diagnostic pop

    IfLt0(ret);  // throws std::system_error
  }

  if (ret == 0) {
    /* There is no more data to read, but depending on how a subclass
       implements GetNextMsg(), the buffer may still contain unprocessed
       messages. */
    EndOfInput = true;
  }

  Buf.MarkSpaceConsumed(ret);
  return TryAdvanceToNextMsg();
}

TStreamMsgReader::TState TStreamMsgReader::ConsumeReadyMsg() noexcept {
  assert(this);

  if (State != TState::MsgReady) {
    assert(false);
    return State;
  }

  assert(Fd >= 0);
  assert(ReadyMsgOffset <= GetDataSize());
  assert(ReadyMsgSize <= (GetDataSize() - ReadyMsgOffset));

  /* hook for subclass to update its internal state */
  BeforeConsumeReadyMsg();

  assert(TrailingDataSize <=
      ((GetDataSize() - ReadyMsgOffset) - ReadyMsgSize));
  Buf.MarkDataConsumed(ReadyMsgOffset + ReadyMsgSize + TrailingDataSize);
  ReadyMsgOffset = 0;
  ReadyMsgSize = 0;
  TrailingDataSize = 0;
  return TryAdvanceToNextMsg();
}

const uint8_t *TStreamMsgReader::GetReadyMsg() const noexcept {
  assert(this);
  static const uint8_t empty_data = 0;

  /* We use a lot of defensive programming below to prevent buggy code from
     reading past the end of the buffer. */

  if ((State != TState::MsgReady) || RestrictReadyMsgCalls) {
    assert(false);
    return &empty_data;
  }

  if (Fd < 0) {
    assert(false);
    return &empty_data;
  }

  size_t data_size = GetDataSize();

  if (ReadyMsgOffset > data_size) {
    assert(false);
    return &empty_data;
  }

  if (ReadyMsgSize > (data_size - ReadyMsgOffset)) {
    assert(false);
    return &empty_data;
  }

  if (TrailingDataSize > ((data_size - ReadyMsgOffset) - ReadyMsgSize)) {
    assert(false);
    return &empty_data;
  }

  if ((GetDataSize() == 0) || (ReadyMsgSize == 0)) {
    return &empty_data;
  }

  return GetData() + ReadyMsgOffset;
}

const uint8_t *TStreamMsgReader::GetData() const noexcept {
  assert(this);
  static const uint8_t empty_data = 0;
  return Buf.DataIsEmpty() ? &empty_data : Buf.Data();
}

void TStreamMsgReader::Reset(int fd) noexcept {
  assert(this);

  /* Give subclass code a chance to reset its state before we reset ours. */
  RestrictReadyMsgCalls = true;
  HandleReset();
  RestrictReadyMsgCalls = false;

  bool no_fd = (fd < 0);
  State = no_fd ? TState::AtEnd : TState::ReadNeeded;
  Fd = no_fd ? -1 : fd;
  Buf.Clear();
  RestrictReadyMsgCalls = false;
  ReadyMsgOffset = 0;
  ReadyMsgSize = 0;
  TrailingDataSize = 0;
  EndOfInput = no_fd;
}

TStreamMsgReader::TStreamMsgReader(int fd)
    : State((fd < 0) ? TState::AtEnd : TState::ReadNeeded),
      Fd((fd < 0) ? -1 : fd),
      RestrictReadyMsgCalls(false),
      ReadyMsgOffset(0),
      ReadyMsgSize(0),
      TrailingDataSize(0),
      EndOfInput(fd < 0) {
}

TStreamMsgReader::TState TStreamMsgReader::TryAdvanceToNextMsg() noexcept {
  assert(this);
  RestrictReadyMsgCalls = true;
  TGetMsgResult result = GetNextMsg();
  RestrictReadyMsgCalls = false;

  switch (result.DataState) {
    case TDataState::MsgReady: {
      if ((result.MsgOffset > GetDataSize()) ||
          (result.MsgSize > (GetDataSize() - result.MsgOffset)) ||
          (result.TrailingDataSize >
              ((GetDataSize() - ReadyMsgOffset) - ReadyMsgSize))) {
        /* Make sure a buggy subclass will not cause us to read beyond the end
           of the buffer. */
        assert(false);
        State = TState::DataInvalid;
        return State;
      }

      ReadyMsgOffset = result.MsgOffset;
      ReadyMsgSize = result.MsgSize;
      TrailingDataSize = result.TrailingDataSize;
      State = TState::MsgReady;
      break;
    }
    case TDataState::NoMsgReady: {
      State = EndOfInput ? TState::AtEnd : TState::ReadNeeded;
      break;
    }
    case TDataState::Invalid: {
      State = TState::DataInvalid;
      break;
    }
    NO_DEFAULT_CASE;
  }

  return State;
}
