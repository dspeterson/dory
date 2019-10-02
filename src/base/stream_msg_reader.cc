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
#include <utility>

#include <unistd.h>

#include <base/error_util.h>
#include <base/no_default_case.h>
#include <base/wr/fd_util.h>

using namespace Base;

TStreamMsgReader::TState TStreamMsgReader::Read() {
  assert(this);

  if (Impl.State != TState::ReadNeeded) {
    assert(false);
    return Impl.State;
  }

  assert(Impl.Fd >= 0);
  assert(Impl.ReadyMsgOffset == 0);
  assert(Impl.ReadyMsgSize == 0);
  assert(!Impl.EndOfInput);
  size_t read_size = GetNextReadSize();

  if (read_size == 0) {
    /* In case we get a read size of 0, return here so we don't get a return
       value of 0 from read() and interpret it as end of input.  A typical
       subclass implementation will probably never return 0 from
       GetNextReadSize() but we can't predict how subclasses will be
       implemented. */
    return Impl.State;
  }

  Impl.Buf.EnsureSpace(read_size);

  /* Note that read() works with TCP and UNIX domain stream sockets, as well as
     standard UNIX pipes. */
  ssize_t ret = Wr::read(Impl.Fd, Impl.Buf.Space(), read_size);

  if (ret < 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlogical-op"
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      return Impl.State;
    }
#pragma GCC diagnostic pop

    IfLt0(ret);  // throws std::system_error
  }

  if (ret == 0) {
    /* There is no more data to read, but depending on how a subclass
       implements GetNextMsg(), the buffer may still contain unprocessed
       messages. */
    Impl.EndOfInput = true;
  }

  Impl.Buf.MarkSpaceConsumed(static_cast<size_t>(ret));
  return TryAdvanceToNextMsg();
}

TStreamMsgReader::TState TStreamMsgReader::ConsumeReadyMsg() {
  assert(this);

  if (Impl.State != TState::MsgReady) {
    assert(false);
    return Impl.State;
  }

  assert(Impl.Fd >= 0);
  assert(Impl.ReadyMsgOffset <= GetDataSize());
  assert(Impl.ReadyMsgSize <= (GetDataSize() - Impl.ReadyMsgOffset));

  /* hook for subclass to update its internal state */
  BeforeConsumeReadyMsg();

  assert(Impl.TrailingDataSize <=
      ((GetDataSize() - Impl.ReadyMsgOffset) - Impl.ReadyMsgSize));
  Impl.Buf.MarkDataConsumed(Impl.ReadyMsgOffset + Impl.ReadyMsgSize +
      Impl.TrailingDataSize);
  Impl.ReadyMsgOffset = 0;
  Impl.ReadyMsgSize = 0;
  Impl.TrailingDataSize = 0;
  return TryAdvanceToNextMsg();
}

const uint8_t *TStreamMsgReader::GetReadyMsg() const noexcept {
  assert(this);
  static const uint8_t empty_data = 0;

  /* We use a lot of defensive programming below to prevent buggy code from
     reading past the end of the buffer. */

  if ((Impl.State != TState::MsgReady) || Impl.RestrictReadyMsgCalls) {
    assert(false);
    return &empty_data;
  }

  if (Impl.Fd < 0) {
    assert(false);
    return &empty_data;
  }

  size_t data_size = GetDataSize();

  if (Impl.ReadyMsgOffset > data_size) {
    assert(false);
    return &empty_data;
  }

  if (Impl.ReadyMsgSize > (data_size - Impl.ReadyMsgOffset)) {
    assert(false);
    return &empty_data;
  }

  if (Impl.TrailingDataSize >
      ((data_size - Impl.ReadyMsgOffset) - Impl.ReadyMsgSize)) {
    assert(false);
    return &empty_data;
  }

  if ((GetDataSize() == 0) || (Impl.ReadyMsgSize == 0)) {
    return &empty_data;
  }

  return GetData() + Impl.ReadyMsgOffset;
}

const uint8_t *TStreamMsgReader::GetData() const noexcept {
  assert(this);
  static const uint8_t empty_data = 0;
  return Impl.Buf.DataIsEmpty() ? &empty_data : Impl.Buf.Data();
}

void TStreamMsgReader::Reset(int fd) noexcept {
  assert(this);

  /* Give subclass code a chance to reset its state before we reset ours. */
  Impl.RestrictReadyMsgCalls = true;
  HandleReset();
  Impl.RestrictReadyMsgCalls = false;

  Impl.Buf.Clear();
  Impl = TImpl(fd, std::move(Impl.Buf));
}

TStreamMsgReader::TStreamMsgReader(int fd)
    : Impl(fd) {
}

TStreamMsgReader::TState TStreamMsgReader::TryAdvanceToNextMsg() {
  assert(this);
  Impl.RestrictReadyMsgCalls = true;
  TGetMsgResult result = GetNextMsg();
  Impl.RestrictReadyMsgCalls = false;

  switch (result.DataState) {
    case TDataState::MsgReady: {
      if ((result.MsgOffset > GetDataSize()) ||
          (result.MsgSize > (GetDataSize() - result.MsgOffset)) ||
          (result.TrailingDataSize >
              ((GetDataSize() - Impl.ReadyMsgOffset) - Impl.ReadyMsgSize))) {
        /* Make sure a buggy subclass will not cause us to read beyond the end
           of the buffer. */
        assert(false);
        Impl.State = TState::DataInvalid;
        return Impl.State;
      }

      Impl.ReadyMsgOffset = result.MsgOffset;
      Impl.ReadyMsgSize = result.MsgSize;
      Impl.TrailingDataSize = result.TrailingDataSize;
      Impl.State = TState::MsgReady;
      break;
    }
    case TDataState::NoMsgReady: {
      Impl.State = Impl.EndOfInput ? TState::AtEnd : TState::ReadNeeded;
      break;
    }
    case TDataState::Invalid: {
      Impl.State = TState::DataInvalid;
      break;
    }
    NO_DEFAULT_CASE;
  }

  return Impl.State;
}

TStreamMsgReader::TImpl::TImpl(int fd, TBuf<uint8_t> &&buf)
    : State((fd < 0) ? TState::AtEnd : TState::ReadNeeded),
      Fd((fd < 0) ? -1 : fd),
      Buf(std::move(buf)),
      EndOfInput(fd < 0)
{
}
