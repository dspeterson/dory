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
  const size_t read_size = PrepareForRead();

  /* In case we get a read size of 0, return here so we don't get a return
     value of 0 from read() and interpret it as end of input.  A typical
     subclass implementation will probably never return 0 from
     GetNextReadSize() but we can't predict how subclasses will behave.

     Note that read() works with TCP and UNIX domain stream sockets, as well as
     standard UNIX pipes. */
  return (read_size == 0) ?
      Impl.State :
      ProcessReadResult(Wr::read(Impl.Fd, Impl.Buf.Space(), read_size));
}

TStreamMsgReader::TState TStreamMsgReader::ConsumeReadyMsg() noexcept {
  assert(this);

  if (Impl.State != TState::MsgReady) {
    Die("Invalid call to TStreamMsgReader::ConsumeReadyMsg()");
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
    Die("Invalid call to TStreamMsgReader::GetReadyMsg()");
  }

  if (Impl.Fd < 0) {
    Die("Invalid file descriptor in TStreamMsgReader::GetReadyMsg()");
  }

  size_t data_size = GetDataSize();

  if (Impl.ReadyMsgOffset > data_size) {
    Die("ReadyMsgOffset invalid in TStreamMsgReader::GetReadyMsg()");
  }

  if (Impl.ReadyMsgSize > (data_size - Impl.ReadyMsgOffset)) {
    Die("ReadyMsgSize invalid in TStreamMsgReader::GetReadyMsg()");
  }

  if (Impl.TrailingDataSize >
      ((data_size - Impl.ReadyMsgOffset) - Impl.ReadyMsgSize)) {
    Die("TrailingDataSize invalid in TStreamMsgReader::GetReadyMsg()");
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

TStreamMsgReader::TState TStreamMsgReader::TryAdvanceToNextMsg() noexcept {
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
        Die("Attempt to read past end of buffer in "
            "TStreamMsgReader::TryAdvanceToNextMsg()");
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

size_t TStreamMsgReader::PrepareForRead() {
  assert(this);

  if (Impl.State != TState::ReadNeeded) {
    Die("Invalid call to TStreamMsgReader::Read()");
  }

  assert(Impl.Fd >= 0);
  assert(Impl.ReadyMsgOffset == 0);
  assert(Impl.ReadyMsgSize == 0);
  assert(!Impl.EndOfInput);
  const size_t read_size = GetNextReadSize();
  Impl.Buf.EnsureSpace(read_size);
  return read_size;
}

TStreamMsgReader::TState TStreamMsgReader::ProcessReadResult(
    ssize_t read_result) {
  assert(this);

  if (read_result < 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlogical-op"
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      return Impl.State;
    }
#pragma GCC diagnostic pop

    IfLt0(read_result);  // throws std::system_error
  }

  if (read_result == 0) {
    /* There is no more data to read, but depending on how a subclass
       implements GetNextMsg(), the buffer may still contain unprocessed
       messages. */
    Impl.EndOfInput = true;
  }

  Impl.Buf.MarkSpaceConsumed(static_cast<size_t>(read_result));
  return TryAdvanceToNextMsg();
}

TStreamMsgReader::TImpl::TImpl(int fd, TBuf<uint8_t> &&buf)
    : State((fd < 0) ? TState::AtEnd : TState::ReadNeeded),
      Fd((fd < 0) ? -1 : fd),
      Buf(std::move(buf)),
      EndOfInput(fd < 0)
{
}
