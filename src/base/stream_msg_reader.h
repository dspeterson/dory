/* <base/stream_msg_reader.h>

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

   Abstract base class for reading a sequence of messages from a stream-
   oriented file descriptor (a TCP or UNIX domain stream socket, or a pipe).
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <sys/types.h>

#include <base/buf.h>
#include <base/no_copy_semantics.h>

namespace Base {

  /* Abstract base class for reading a sequence of messages from a stream-
     oriented file descriptor (a TCP or UNIX domain stream socket, or a pipe).
     Subclasses will handle the details of different wire formats for messages.
     For instance, a subclass might implement support for messages consisting
     of a size field followed by a message body. */
  class TStreamMsgReader {
    NO_COPY_SEMANTICS(TStreamMsgReader);

    public:
    /* State of message reader. */
    enum class TState {
      /* More data needs to be read. */
      ReadNeeded,

      /* A message is ready for consumption.  Client code accesses the ready
         message by calling GetReadyMsg() and GetReadyMsgSize().  Once client
         is done processing message, client calls ConsumeReadyMsg(). */
      MsgReady,

      /* No more messages can be read because the message data is invalid.  If
         desired, client code can inspect the buffer contents by calling
         GetData() and GetDataSize(). */
      DataInvalid,

      /* The other end of the connection has been closed and all messages have
         been consumed.  If any partial message data remains, it can be
         obtained by calling GetData() and GetDataSize(). */
      AtEnd
    };  // TState

    virtual ~TStreamMsgReader() = default;

    /* When in state TState::ReadNeeded, client code calls this to read more
       data.  This is guaranteed not to block if prior to the call, the client
       has determined that the file descriptor is readable (for instance, by
       calling poll() or select()).  Returns new reader state after finishing
       read.  On read() error, errno values {EBADF, EFAULT, EINVAL} are treated
       as fatal, causing immediate program termination.  Other errors cause
       std::system_error to be thrown. */
    TState Read();

    /* Similar to Read() above, but allows caller to gain more control over the
       read operation by passing read_fn, which may define its own error
       handling strategy. */
    template <typename TReadFn>
    TState Read(TReadFn read_fn) noexcept(noexcept(read_fn(0, nullptr, 0))) {
      const size_t read_size = PrepareForRead();

      /* In case we get a read size of 0, return here so we don't get a return
         value of 0 from read() and interpret it as end of input.  A typical
         subclass implementation will probably never return 0 from
         GetNextReadSize() but we can't predict how subclasses will behave. */
      return (read_size == 0) ?
          Impl.State :
          ProcessReadResult(read_fn(Impl.Fd, Impl.Buf.Space(), read_size));
    }

    /* When in state TState::MsgReady, client calls this to indicate that the
       ready message has been processed.  Returns next state of reader. */
    TState ConsumeReadyMsg() noexcept;

    /* Returns current state of reader. */
    TState GetState() const noexcept {
      return Impl.State;
    }

    /* When in state TState::MsgReady, returns a pointer to the ready message.
       Note that empty messages are allowed, in which case GetReadyMsgSize()
       returns 0.  Must not be called from within any subclass-implemented
       method except BeforeConsumeReadyMsg(). */
    const uint8_t *GetReadyMsg() const noexcept;

    /* When in state TState::MsgReady, returns the size in bytes of the ready
       message.  Must not be called from within any subclass-implemented method
       except BeforeConsumeReadyMsg(). */
    size_t GetReadyMsgSize() const noexcept {
      assert(Impl.State == TState::MsgReady);
      assert(!Impl.RestrictReadyMsgCalls);
      assert(Impl.Fd >= 0);
      assert(Impl.ReadyMsgOffset <= GetDataSize());
      assert(Impl.ReadyMsgSize <= (GetDataSize() - Impl.ReadyMsgOffset));
      assert(Impl.TrailingDataSize <=
          ((GetDataSize() - Impl.ReadyMsgOffset) - Impl.ReadyMsgSize));
      return Impl.ReadyMsgSize;
    }

    /* Client code can use this to obtain any partial message data remaining
       after state TState::AtEnd has been reached, or to peek into the buffer
       if desired.  Subclasses that implement GetNextReadSize() and
       GetNextMsg() call this to examine the current buffer contents.  Note
       that when in state TState::MsgReady, the return value of this method is
       not necessarily the same as the return value of GetReadyMsg().  For
       instance, if the wire format for a message consists of a size field
       followed by a message body, this will return a pointer to the location
       of the size field while GetReadyMsg() returns a pointer to the message
       body. */
    const uint8_t *GetData() const noexcept;

    /* Client code can use this to obtain any partial message data remaining
       after state TState::AtEnd has been reached, or to peek into the buffer
       if desired.  Subclasses that implement GetNextReadSize() and
       GetNextMsg() call this to examine the current buffer contents.  Note
       that when in state TState::MsgReady, the return value of this method is
       not necessarily the same as the return value of GetReadyMsgSize().  This
       returns the total size in bytes of buffered data, which may include
       multiple messages along with extra data such as size fields or message
       terminators. */
    size_t GetDataSize() const noexcept {
      return Impl.Buf.DataSize();
    }

    /* Returns the file descriptor that the reader is reading from, or -1 if it
       currently has no associated file descriptor. */
    int GetFd() const noexcept {
      return Impl.Fd;
    }

    /* Resets the state of the reader to the same state created by the
       constructor that takes a file descriptor as a parameter.  The reader
       does not own the file descriptor.  It is the client's responsibility to
       close the file descriptor when finished using it.*/
    void Reset(int fd) noexcept;

    /* Resets the state of the reader to the same state created by the default
       constructor.  Specifically, no file descriptor is associated with the
       reader. */
    void Reset() noexcept {
      Reset(-1);
    }

    protected:
    /* State determined by GetNextMsg(). */
    enum class TDataState {
      MsgReady,  // a message is ready for consumption
      NoMsgReady,  // no message is ready yet
      Invalid  // invalid message data (recovery is impossible)
    };  // TDataState

    /* Result returned by GetNextMsg(). */
    struct TGetMsgResult final {
      /* This is meant to be called by the implementation of GetNextMsg().
         Return a result indicating that a message is ready for consumption.
         Parameter 'offset' is the offset of the first byte of the message
         within the buffer pointed to by the return value of GetData().
         Parameter 'size' is the size of the message in bytes.  Parameter
         'trailing_data_size' is the size in bytes of any trailing data (such
         as a message terminator). */
      static TGetMsgResult MsgReady(size_t offset, size_t size,
          size_t trailing_data_size) noexcept {
        return TGetMsgResult(TDataState::MsgReady, offset, size,
            trailing_data_size);
      }

      /* This is meant to be called by the implementation of GetNextMsg().
         Return a result indicating that no message is ready yet. */
      static TGetMsgResult NoMsgReady() noexcept {
        return TGetMsgResult(TDataState::NoMsgReady, 0, 0, 0);
      }

      /* This is meant to be called by the implementation of GetNextMsg().
         Return a result indicating that the message data is invalid. */
      static TGetMsgResult Invalid() noexcept {
        return TGetMsgResult(TDataState::Invalid, 0, 0, 0);
      }

      const TDataState DataState;

      /* Offest of first byte of ready message from location returned by
         GetData(). */
      const size_t MsgOffset;

      /* Size in bytes of ready message. */
      const size_t MsgSize;

      const size_t TrailingDataSize;

      private:
      /* Can only be constructed by one of the static methods above. */
      TGetMsgResult(TDataState data_state, size_t msg_offset,
          size_t msg_size, size_t trailing_data_size) noexcept
          : DataState(data_state),
            MsgOffset(msg_offset),
            MsgSize(msg_size),
            TrailingDataSize(trailing_data_size) {
      }
    };  // TGetMsgResult

    /* Creates a reader initialized with a file descriptor to read from.  The
       reader does not own the file descriptor.  It is the client's
       responsibility to close the file descriptor when finished using it. */
    explicit TStreamMsgReader(int fd);

    /* Creates a reader without any associated file descriptor.  Client must
       then call Reset() specifying a file descriptor before using the reader.
     */
    TStreamMsgReader()
        : TStreamMsgReader(-1) {
    }

    TStreamMsgReader(TStreamMsgReader &&) noexcept = default;

    TStreamMsgReader &operator=(TStreamMsgReader &&) noexcept = default;

    /* GetNextMsg() may call this to determine whether the end of input has
       been reached. */
    bool AtEndOfInput() const noexcept {
      return Impl.EndOfInput;
    }

    /* Convenience method for subclasses to use.  Note that when a message is
       ready for the client to consume, the return value is equal to
       (GetReadyMsg() - GetData()).  Must not be called from within any
       subclass-implemented method except BeforeConsumeReadyMsg(). */
    size_t GetReadyMsgOffset() const noexcept {
      assert(Impl.State == TState::MsgReady);
      assert(!Impl.RestrictReadyMsgCalls);
      assert(Impl.Fd >= 0);
      assert(Impl.ReadyMsgOffset <= GetDataSize());
      assert(Impl.ReadyMsgSize <= (GetDataSize() - Impl.ReadyMsgOffset));
      assert(Impl.TrailingDataSize <=
          ((GetDataSize() - Impl.ReadyMsgOffset) - Impl.ReadyMsgSize));
      return Impl.ReadyMsgOffset;
    }

    /* Base class calls this when it needs to determine how many bytes to read
       next. */
    virtual size_t GetNextReadSize() noexcept = 0;

    /* Base class calls this to see if a message is ready for consumption yet.
     */
    virtual TGetMsgResult GetNextMsg() noexcept = 0;

    /* At the start of a Reset() call, the base class calls this method to
       allow the derived class to reset any internal state it maintains. */
    virtual void HandleReset() noexcept = 0;

    /* The base class calls this method immediately before a ready message is
       about to be consumed.  Subclass code may use this for updating its
       internal state. */
    virtual void BeforeConsumeReadyMsg() noexcept = 0;

    private:
    /* See if there is a complete message in the buffer and update state
       accordingly. */
    TState TryAdvanceToNextMsg() noexcept;

    size_t PrepareForRead();

    TState ProcessReadResult(ssize_t read_result);

    struct TImpl {
      /* Client-visible state of message reader. */
      TState State;

      /* File descriptor to read from. */
      int Fd;

      /* Buffer to read data into. */
      Base::TBuf<uint8_t> Buf;

      /* Used as a sanity check to prevent calls to GetReadyMsg(),
         GetReadyMsgSize(), or GetReadyMsgOffest() from any subclass-implemeted
         method except BeforeConsumeReadyMsg(). */
      bool RestrictReadyMsgCalls = false;

      /* When a message is ready for the client to consume, this is the offset of
         the message from the location returned by GetData(). */
      size_t ReadyMsgOffset = 0;

      /* When a message is ready for the client to consume, this is the size in
         bytes of the message. */
      size_t ReadyMsgSize = 0;

      /* When a message is ready for the client to consume, this indicates the
         size in bytes of any trailing data.  For instance, if a message consists
         of a sequence of non-null bytes terminated by a null byte, this would be
         1.  If each message is preceded by a size field, this would be 0. */
      size_t TrailingDataSize = 0;

      /* A true value indicates that the other end of the socket or pipe has been
         closed and no more data is available. */
      bool EndOfInput;

      TImpl(int fd, Base::TBuf<uint8_t> &&buf);

      explicit TImpl(int fd)
          : TImpl(fd, Base::TBuf<uint8_t>()) {
      }

      TImpl(TImpl &&) noexcept = default;

      TImpl &operator=(TImpl &&) noexcept = default;
    };

    TImpl Impl;
  };  // TStreamMsgReader

}  // Base
