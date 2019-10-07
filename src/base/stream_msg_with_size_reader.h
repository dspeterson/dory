/* <base/stream_msg_with_size_reader.h>

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

   Class for reading a stream of messages in which each message is preceded by
   a size field.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <base/stream_msg_reader.h>

namespace Base {

  template <typename TSizeFieldType>
  class TStreamMsgWithSizeReader;

  /* Base class for TStreamMsgWithSizeReader below.  Only
     TStreamMsgWithSizeReader can invoke the constructor.  Contains method(s)
     that may be invoked by client code. */
  class TStreamMsgWithSizeReaderBase : public TStreamMsgReader {
    NO_COPY_SEMANTICS(TStreamMsgWithSizeReaderBase);

    /* So TStreamMsgWithSizeReader can invoke the constructor. */
    template <typename TSizeFieldType>
    friend class TStreamMsgWithSizeReader;

    /* Private so no class other than friend can invoke constructor.

       Parameters:
           'fd': File descriptor to read from.
           'size_field_size': Size in bytes of size field.
           'size_field_is_signed': True if size field is signed, or false
               otherwise.  If size field is signed, it is verified that the
               value is nonnegative.
           'size_includes_size_field': True if the size specified by the size
               field includes the size of the size field, or false otherwise.
           'include_size_field_in_msg': If true, include the size field in the
               result returned by GetReadyMsg() and GetReadyMsgSize().
           'max_msg_body_size': Maximum allowed message body size in bytes.
               The size field is never considered part of the message body,
               even if 'include_size_field_in_msg' is true.
           'preferred_read_size': Preferred # of bytes to read, if a read is
               needed.  Each read will be of this size unless we are close to
               the limit specified by 'max_buffer_size'.
     */
    TStreamMsgWithSizeReaderBase(int fd, size_t size_field_size,
        bool size_field_is_signed, bool size_includes_size_field,
        bool include_size_field_in_msg, size_t max_msg_body_size,
        size_t preferred_read_size);

    public:
    enum class TDataInvalidReason {
      InvalidSizeField,
      MsgBodyTooLarge
    };  // TDataInvalidReason

    size_t GetSizeFieldSize() const noexcept {
      assert(this);
      return SizeFieldSize;
    }

    bool GetSizeFieldIsSigned() const noexcept {
      assert(this);
      return SizeFieldIsSigned;
    }

    bool GetSizeIncludesSizeField() const noexcept {
      assert(this);
      return SizeIncludesSizeField;
    }

    bool GetIncludeSizeFieldInMsg() const noexcept {
      assert(this);
      return IncludeSizeFieldInMsg;
    }

    size_t GetMaxMsgBodySize() const noexcept {
      assert(this);
      return MaxMsgBodySize;
    }

    void SetMaxMsgBodySize(size_t size) noexcept {
      assert(this);
      MaxMsgBodySize = size;
    }

    size_t GetPreferredReadSize() const noexcept {
      assert(this);
      return PreferredReadSize;
    }

    void SetPreferredReadSize(size_t size) noexcept {
      assert(this);
      PreferredReadSize = size;
    }

    TOpt<TDataInvalidReason> GetDataInvalidReason() const noexcept {
      assert(this);
      return OptDataInvalidReason;
    }

    ~TStreamMsgWithSizeReaderBase() override = default;

    protected:
    size_t GetNextReadSize() noexcept override;

    TGetMsgResult GetNextMsg() noexcept override;

    void HandleReset() noexcept override;

    void BeforeConsumeReadyMsg() noexcept override;

    private:
    /* Pointer to function with the following signature:

           TOpt<uint64_t> fn(const uint8_t *field_loc) noexcept;
     */
    using TSizeFieldReadFn = std::remove_reference<decltype(std::declval<
        TOpt<uint64_t> (*)(const uint8_t *field_loc) noexcept>())>::type;

    static TSizeFieldReadFn ChooseSizeFieldReadFn(size_t size_field_size,
        bool size_field_is_signed) noexcept;

    /* Size in bytes of size field. */
    const size_t SizeFieldSize;

    /* True if size field is signed, or false otherwise. */
    const bool SizeFieldIsSigned;

    /* True if size specified by size field includes size of size field, or
       false otherwise. */
    const bool SizeIncludesSizeField;

    /* If true, include the size field in the result returned by GetReadyMsg()
       and GetReadyMsgSize(). */
    const bool IncludeSizeFieldInMsg;

    /* A function for reading the size field. */
    const TSizeFieldReadFn SizeFieldReadFn;

    /* Maximum allowed message body size in bytes. */
    size_t MaxMsgBodySize;

    /* Preferred # of bytes to read, if a read is needed.  Each read will be of
       this size unless we are close to the limit specified by
       'MaxBufferSize'. */
    size_t PreferredReadSize;

    /* Size of message body in bytes, when value has been obtained from size
       field. */
    TOpt<size_t> OptMsgBodySize;

    /* Indicates reason why data is invalid, if invalid data is encountered. */
    TOpt<TDataInvalidReason> OptDataInvalidReason;
  };  // TStreamMsgWithSizeReaderBase

  /* Reads a stream of messages from a socket or pipe.  Each message is
     preceded by a size field.  T indicates the size and signedness of the size
     field.  For instance, TStreamMsgWithSizeReader<uint16_t> reads messages
     whose size fields are 16-bit unsigned integers, and
     TStreamMsgWithSizeReader<int32_t> reads messages whose size fields are
     32-bit signed integers.  Size field is stored in network byte order. */
  template <typename T>
  class TStreamMsgWithSizeReader : public TStreamMsgWithSizeReaderBase {
    NO_COPY_SEMANTICS(TStreamMsgWithSizeReader);

    public:
    using TSizeFieldType = T;

    using TDataInvalidReason =
        TStreamMsgWithSizeReaderBase::TDataInvalidReason;

    /* Parameters:
           'fd': File descriptor to read from.
           'size_includes_size_field': True if the size specified by the size
               field includes the size of the size field, or false otherwise.
           'max_msg_body_size': Maximum allowed message body size in bytes.
           'preferred_read_size': Preferred # of bytes to read, if a read is
               needed.  Each read will be of this size unless we are close to
               the limit specified by 'max_buffer_size'.
     */
    TStreamMsgWithSizeReader(int fd, bool size_includes_size_field,
        bool include_size_field_in_msg, size_t max_msg_body_size,
        size_t preferred_read_size)
        : TStreamMsgWithSizeReaderBase(fd, sizeof(TSizeFieldType),
              std::is_signed<TSizeFieldType>::value, size_includes_size_field,
              include_size_field_in_msg, max_msg_body_size,
              preferred_read_size) {
    }

    /* Parameters:
           'size_includes_size_field': True if the size specified by the size
               field includes the size of the size field, or false otherwise.
           'max_msg_body_size': Maximum allowed message body size in bytes.
           'preferred_read_size': Preferred # of bytes to read, if a read is
               needed.  Each read will be of this size unless we are close to
               the limit specified by 'max_buffer_size'.
     */
    TStreamMsgWithSizeReader(bool size_includes_size_field,
        bool include_size_field_in_msg, size_t max_msg_body_size,
        size_t preferred_read_size)
        : TStreamMsgWithSizeReaderBase(-1, sizeof(TSizeFieldType),
              std::is_signed<TSizeFieldType>::value, size_includes_size_field,
              include_size_field_in_msg, max_msg_body_size,
              preferred_read_size) {
    }
  };  // TStreamMsgWithSizeReader

}  // Base
