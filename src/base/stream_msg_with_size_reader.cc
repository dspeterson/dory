/* <base/stream_msg_with_size_reader.cc>

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

   Implements <base/stream_msg_with_size_reader.h>.
 */

#include <base/stream_msg_with_size_reader.h>

#include <limits>
#include <stdexcept>

#include <base/field_access.h>

using namespace Base;

static TOpt<uint64_t> ReadUnsigned8BitSizeField(const uint8_t *field_loc) {
  return TOpt<uint64_t>(*field_loc);
}

static TOpt<uint64_t> ReadUnsigned16BitSizeField(const uint8_t *field_loc) {
  return TOpt<uint64_t>(ReadUint16FromHeader(field_loc));
}

static TOpt<uint64_t> ReadUnsigned32BitSizeField(const uint8_t *field_loc) {
  return TOpt<uint64_t>(ReadUint32FromHeader(field_loc));
}

static TOpt<uint64_t> ReadUnsigned64BitSizeField(const uint8_t *field_loc) {
  return TOpt<uint64_t>(ReadUint64FromHeader(field_loc));
}

static TOpt<uint64_t> ReadSigned8BitSizeField(const uint8_t *field_loc) {
  int8_t size = static_cast<int8_t>(*field_loc);
  return (size < 0) ?
      TOpt<uint64_t>() : TOpt<uint64_t>(static_cast<uint64_t>(size));
}

static TOpt<uint64_t> ReadSigned16BitSizeField(const uint8_t *field_loc) {
  int16_t size = ReadInt16FromHeader(field_loc);
  return (size < 0) ?
      TOpt<uint64_t>() : TOpt<uint64_t>(static_cast<uint64_t>(size));
}

static TOpt<uint64_t> ReadSigned32BitSizeField(const uint8_t *field_loc) {
  int32_t size = ReadInt32FromHeader(field_loc);
  return (size < 0) ?
      TOpt<uint64_t>() : TOpt<uint64_t>(static_cast<uint64_t>(size));
}

static TOpt<uint64_t> ReadSigned64BitSizeField(const uint8_t *field_loc) {
  int64_t size = ReadInt64FromHeader(field_loc);
  return (size < 0) ?
      TOpt<uint64_t>() : TOpt<uint64_t>(static_cast<uint64_t>(size));
}

TStreamMsgWithSizeReaderBase::TStreamMsgWithSizeReaderBase(
    int fd, size_t size_field_size, bool size_field_is_signed,
    bool size_includes_size_field, bool include_size_field_in_msg,
    size_t max_msg_body_size, size_t preferred_read_size)
    : TStreamMsgReader(fd),
      SizeFieldSize(size_field_size),
      SizeFieldIsSigned(size_field_is_signed),
      SizeIncludesSizeField(size_includes_size_field),
      IncludeSizeFieldInMsg(include_size_field_in_msg),
      SizeFieldReadFn(ChooseSizeFieldReadFn(size_field_size,
          size_field_is_signed)),
      MaxMsgBodySize(max_msg_body_size),
      PreferredReadSize(preferred_read_size) {
  assert((std::numeric_limits<size_t>::max() - max_msg_body_size) >=
      size_field_size);
  assert(max_msg_body_size <=
      (static_cast<size_t>(std::numeric_limits<int>::max()) -
          size_field_size));
  assert(preferred_read_size);
}

size_t TStreamMsgWithSizeReaderBase::GetNextReadSize() {
  assert(this);
  return PreferredReadSize;
}

TStreamMsgReader::TGetMsgResult
TStreamMsgWithSizeReaderBase::GetNextMsg() noexcept {
  assert(this);
  size_t data_size = GetDataSize();

  if (data_size < SizeFieldSize) {
    assert(OptMsgBodySize.IsUnknown());
    return TGetMsgResult::NoMsgReady();
  }

  if (OptMsgBodySize.IsUnknown()) {
    auto opt_size = SizeFieldReadFn(GetData());

    if (opt_size.IsUnknown()) {
      /* Signed size field value was found to be negative. */
      OptDataInvalidReason.MakeKnown(TDataInvalidReason::InvalidSizeField);
      return TGetMsgResult::Invalid();
    }

    uint64_t size = *opt_size;

    if (SizeIncludesSizeField && (size < SizeFieldSize)) {
      OptDataInvalidReason.MakeKnown(TDataInvalidReason::InvalidSizeField);
      return TGetMsgResult::Invalid();
    }

    if ((size > MaxMsgBodySize) &&
        (!SizeIncludesSizeField ||
            ((size - MaxMsgBodySize) > SizeFieldSize))) {
      OptDataInvalidReason.MakeKnown(TDataInvalidReason::MsgBodyTooLarge);
      return TGetMsgResult::Invalid();
    }

    OptMsgBodySize.MakeKnown(static_cast<size_t>(
        SizeIncludesSizeField ? (size - SizeFieldSize) : size));
  }

  size_t body_size = *OptMsgBodySize;

  if (data_size < (SizeFieldSize + body_size)) {
    return TGetMsgResult::NoMsgReady();
  }

  size_t reported_size = body_size;
  size_t reported_offset = 0;

  if (IncludeSizeFieldInMsg) {
    reported_size += SizeFieldSize;
  } else {
    reported_offset += SizeFieldSize;
  }

  return TGetMsgResult::MsgReady(reported_offset, reported_size, 0);
}

void TStreamMsgWithSizeReaderBase::HandleReset() noexcept {
  assert(this);
  OptMsgBodySize.Reset();
  OptDataInvalidReason.Reset();
}

void TStreamMsgWithSizeReaderBase::BeforeConsumeReadyMsg() noexcept {
  assert(this);
  OptMsgBodySize.Reset();
}

TStreamMsgWithSizeReaderBase::TSizeFieldReadFn
TStreamMsgWithSizeReaderBase::ChooseSizeFieldReadFn(size_t size_field_size,
    bool size_field_is_signed) {
  if ((size_field_size != 1) && (size_field_size != 2) &&
      (size_field_size != 4) && (size_field_size != 8)) {
    throw std::logic_error(
        "Bad value for size_field_size in TStreamMsgWithSizeReaderBase");
  }

  switch (size_field_size) {
    case 2: {
      return size_field_is_signed ?
          ReadSigned16BitSizeField : ReadUnsigned16BitSizeField;
    }
    case 4: {
      return size_field_is_signed ?
          ReadSigned32BitSizeField : ReadUnsigned32BitSizeField;
    }
    case 8: {
      return size_field_is_signed ?
          ReadSigned64BitSizeField : ReadUnsigned64BitSizeField;
    }
  }

  return size_field_is_signed ?
      ReadSigned8BitSizeField : ReadUnsigned8BitSizeField;
}
