/* <dory/compress/lz4/lz4_codec.cc>

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

   Implements <dory/compress/lz4/lz4_codec.h>.
 */

#include <dory/compress/lz4/lz4_codec.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <boost/lexical_cast.hpp>

#include <base/on_destroy.h>
#include <server/counter.h>
#include <third_party/lz4/lz4-1.8.0/lib/lz4frame.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Lz4;

SERVER_COUNTER(Lz4CompressSuccess);
SERVER_COUNTER(Lz4Error);

static size_t CheckLz4Status(size_t status, const char *lz4_function_name) {
  assert(lz4_function_name);

  if (LZ4F_isError(status)) {
    Lz4Error.Increment();
    std::string msg("Function ");
    msg += lz4_function_name;
    msg += " reported error: [";
    msg += LZ4F_getErrorName(status);
    msg += "]";
    throw TCompressionCodecApi::TError(msg.c_str());
  }

  return status;
}

static std::mutex SingletonInitMutex;

static std::unique_ptr<const TLz4Codec> Singleton;

const TLz4Codec &TLz4Codec::The() {
  if (!Singleton) {
    std::lock_guard<std::mutex> lock(SingletonInitMutex);

    if (!Singleton) {
      Singleton.reset(new TLz4Codec);
    }
  }

  return *Singleton;
}

static const int DEFAULT_LEVEL = 0;
static const int MIN_LEVEL = 0;
static const int MAX_LEVEL = 16;

TOpt<int> TLz4Codec::GetRealCompressionLevel(
    const TOpt<int> &requested_level) const noexcept {
  assert(this);

  if (requested_level.IsUnknown()) {
    return TOpt<int>(DEFAULT_LEVEL);
  }

  int requested = *requested_level;
  return TOpt<int>(
      ((requested >= MIN_LEVEL) && (requested <= MAX_LEVEL)) ?
      requested : DEFAULT_LEVEL);
}

size_t TLz4Codec::ComputeUncompressedResultBufSpace(
    const void *compressed_data, size_t compressed_size) const {
  assert(this);
  LZ4F_decompressionContext_t dctx = nullptr;
  CheckLz4Status(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION),
      "LZ4F_createDecompressionContext");
  assert(dctx);
  TOnDestroy destroy_ctx(
      [dctx]() noexcept {
        LZ4F_freeDecompressionContext(dctx);
      }
  );

  LZ4F_frameInfo_t frame_info;
  size_t src_size = compressed_size;
  CheckLz4Status(LZ4F_getFrameInfo(dctx, &frame_info, compressed_data,
      &src_size), "LZ4F_getFrameInfo");
  unsigned long long uncompressed_size = frame_info.contentSize;

  if (compressed_size > (std::numeric_limits<size_t>::max() / 256)) {
    /* If the compressed data size is so large that multiplication by 255 below
       is likely to cause overflow, then something is wrong. */
    Lz4Error.Increment();
    throw TCompressionCodecApi::TError(
        "Size of lz4 compressed data is out of bounds");
  }

  /* The maximum compression ratio of lz4 is 255.  See
     https://stackoverflow.com/questions/25740471/lz4-library-decompressed-data-upper-bound-size-estimation
     for more information. */
  if ((uncompressed_size == 0) ||
      (uncompressed_size > (255 * compressed_size))) {
    Lz4Error.Increment();
    std::string msg(
        "Bad uncompressed data size in lz4 frame: compressed size ");
    msg += boost::lexical_cast<std::string>(compressed_size);
    msg += " uncompressed size ";
    msg += boost::lexical_cast<std::string>(uncompressed_size);
    throw TCompressionCodecApi::TError(msg.c_str());
  }

  return static_cast<size_t>(uncompressed_size);
}

static void CheckReadBufferOverflow(size_t bytes_consumed, size_t capacity,
    const char *fn_name) {
  assert(fn_name);

  if (bytes_consumed > capacity) {
    std::string msg("Bug in ");
    msg += fn_name;
    msg += "(): consumed ";
    msg += boost::lexical_cast<std::string>(bytes_consumed);
    msg += " bytes from buffer with size of only ";
    msg += boost::lexical_cast<std::string>(capacity);
    /* There is a bug in the compression library that caused data to be
       consumed past the end of our buffer. */
    throw std::logic_error(msg.c_str());
  }
}

static void CheckWriteBufferOverflow(size_t bytes_written, size_t capacity,
    const char *fn_name) {
  assert(fn_name);

  if (bytes_written > capacity) {
    std::string msg("Bug in ");
    msg += fn_name;
    msg += "(): overwrote buffer of size ";
    msg += boost::lexical_cast<std::string>(capacity);
    msg += " with output of size ";
    msg += boost::lexical_cast<std::string>(bytes_written);
    /* There is a bug in the compression library that caused data to be written
       past the end of our buffer.  Terminate immediately, since memory has
       been trashed. */
    throw std::logic_error(msg);
  }
}

size_t TLz4Codec::Uncompress(const void *input_buf, size_t input_buf_size,
    void *output_buf, size_t output_buf_size) const {
  assert(this);
  LZ4F_decompressionContext_t dctx = nullptr;
  CheckLz4Status(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION),
      "LZ4F_createDecompressionContext");
  assert(dctx);
  TOnDestroy destroy_ctx(
      [dctx]() noexcept {
        LZ4F_freeDecompressionContext(dctx);
      }
  );

  const auto *in_buf = reinterpret_cast<const uint8_t *>(input_buf);
  auto *out_buf = reinterpret_cast<uint8_t *>(output_buf);
  LZ4F_frameInfo_t frame_info;
  size_t src_size = input_buf_size;
  CheckLz4Status(LZ4F_getFrameInfo(dctx, &frame_info, in_buf, &src_size),
      "LZ4F_getFrameInfo");
  CheckReadBufferOverflow(src_size, input_buf_size, "LZ4F_getFrameInfo");

  if (output_buf_size < static_cast<size_t>(frame_info.contentSize)) {
    /* This should never happen, assuming that the caller's output buffer is at
       least as large as the value returned by
       ComputeUncompressedResultBufSpace(). */
    throw std::logic_error("Output buffer for lz4 decompression is too small");
  }

  size_t in_offset = src_size;
  size_t out_offset = 0;

  for (; ; ) {
    src_size = input_buf_size - in_offset;

    if (src_size == 0) {
      break;
    }

    size_t saved_src_size = src_size;
    size_t dst_size = output_buf_size - out_offset;

    if (dst_size == 0) {
      /* This should never happen, assuming that the caller's output buffer is
         at least as large as the value returned by
         ComputeUncompressedResultBufSpace(). */
      Lz4Error.Increment();
      throw TCompressionCodecApi::TError(
          "Ran out of space in output buffer during lz4 decompression");
    }

    size_t saved_dst_size = dst_size;
    CheckLz4Status(LZ4F_decompress(dctx, out_buf + out_offset, &dst_size,
        in_buf + in_offset, &src_size, nullptr), "LZ4F_decompress");
    CheckWriteBufferOverflow(dst_size, saved_dst_size, "LZ4F_decompress");
    CheckReadBufferOverflow(src_size, saved_src_size, "LZ4F_decompress");
    out_offset += dst_size;
    in_offset += src_size;
  }

  return out_offset;  // size in bytes of uncompressed output
}

size_t TLz4Codec::DoComputeCompressedResultBufSpace(
    const void * /*uncompressed_data*/, size_t uncompressed_size,
    int /*compression_level*/) const {
  assert(this);
  return CheckLz4Status(LZ4F_compressBound(uncompressed_size, nullptr),
      "LZ4F_compressBound");
}

size_t TLz4Codec::DoCompress(const void *input_buf, size_t input_buf_size,
    void *output_buf, size_t output_buf_size, int compression_level) const {
  assert(this);
  assert((compression_level >= MIN_LEVEL) && (compression_level <= MAX_LEVEL));
  auto *out_buf = reinterpret_cast<uint8_t *>(output_buf);
  LZ4F_compressionContext_t cctx = nullptr;
  CheckLz4Status(LZ4F_createCompressionContext(&cctx, LZ4F_VERSION),
      "LZ4F_createCompressionContext");
  assert(cctx);
  TOnDestroy destroy_ctx(
      [cctx]() noexcept {
        LZ4F_freeCompressionContext(cctx);
      }
  );

  LZ4F_preferences_t prefs;
  std::memset(&prefs, 0, sizeof(prefs));
  prefs.compressionLevel = compression_level;
  prefs.frameInfo.blockMode = LZ4F_blockIndependent;
  prefs.frameInfo.contentSize = input_buf_size;
  size_t bytes_written = CheckLz4Status(
      LZ4F_compressBegin(cctx, out_buf, output_buf_size, &prefs),
      "LZ4F_compressBegin");
  CheckWriteBufferOverflow(bytes_written, output_buf_size,
      "LZ4F_compressBegin");
  out_buf += bytes_written;
  size_t capacity = output_buf_size - bytes_written;

  bytes_written = CheckLz4Status(
      LZ4F_compressUpdate(cctx, out_buf, capacity, input_buf, input_buf_size,
          nullptr),
      "LZ4F_compressUpdate");
  CheckWriteBufferOverflow(bytes_written, capacity, "LZ4F_compressUpdate");
  out_buf += bytes_written;
  capacity -= bytes_written;

  bytes_written = CheckLz4Status(LZ4F_compressEnd(cctx, out_buf, capacity,
      nullptr), "LZ4F_compressEnd");
  CheckWriteBufferOverflow(bytes_written, capacity, "LZ4F_compressEnd");
  out_buf += bytes_written;
  capacity -= bytes_written;

  size_t compressed_size = output_buf_size - capacity;

  if (compressed_size > output_buf_size) {
    throw std::logic_error(
        "Compressed size exceeds output buffer size in TLz4Codec::DoCompress()"
    );
  }

  Lz4CompressSuccess.Increment();
  return compressed_size;
}
