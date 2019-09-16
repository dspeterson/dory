/* <dory/compress/gzip/gzip_codec.cc>

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

   Implements <dory/compress/gzip/gzip_codec.h>.
 */

#include <dory/compress/gzip/gzip_codec.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

#include <boost/lexical_cast.hpp>
#include <zlib.h>

#include <base/counter.h>
#include <base/on_destroy.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Gzip;

DEFINE_COUNTER(ZlibBufError);
DEFINE_COUNTER(ZlibCompressSuccess);
DEFINE_COUNTER(ZlibDataError);
DEFINE_COUNTER(ZlibDecompressOutOfSpace);
DEFINE_COUNTER(ZlibNeedDictError);
DEFINE_COUNTER(ZlibNotAtEndAfterCompress);
DEFINE_COUNTER(ZlibOverflowComputingCompressBufSize);
DEFINE_COUNTER(ZlibStreamError);
DEFINE_COUNTER(ZlibUnknownError);
DEFINE_COUNTER(ZlibVersionError);

static void ThrowZlibError(const z_stream &strm,
    const char *zlib_function_name, const char *blurb) {
  std::string msg("zlib function ");
  msg += zlib_function_name;
  msg += " reported ";
  msg += blurb;

  if (strm.msg) {
    msg += ": ";
    msg += strm.msg;
  }

  throw TCompressionCodecApi::TError(msg.c_str());
}

static int CheckStatus(int status, const z_stream &strm,
    const char *zlib_function_name) {
  switch (status) {
    case Z_OK:
    case Z_STREAM_END: {
      break;
    }
    case Z_MEM_ERROR: {
      throw std::bad_alloc();
    }
    case Z_STREAM_ERROR: {
      ZlibStreamError.Increment();
      ThrowZlibError(strm, zlib_function_name, "Z_STREAM_ERROR");
      break;  // not reached
    }
    case Z_VERSION_ERROR: {
      ZlibVersionError.Increment();
      ThrowZlibError(strm, zlib_function_name, "Z_VERSION_ERROR");
      break;  // not reached
    }
    case Z_NEED_DICT: {
      ZlibNeedDictError.Increment();
      ThrowZlibError(strm, zlib_function_name, "Z_NEED_DICT");
      break;  // not reached
    }
    case Z_DATA_ERROR: {
      ZlibDataError.Increment();
      ThrowZlibError(strm, zlib_function_name, "Z_DATA_ERROR");
      break;  // not reached
    }
    case Z_BUF_ERROR: {
      ZlibBufError.Increment();
      ThrowZlibError(strm, zlib_function_name, "Z_BUF_ERROR");
      break;  // not reached
    }
    default: {
      ZlibUnknownError.Increment();
      std::string blurb("unknown error ");
      blurb += boost::lexical_cast<std::string>(status);
      ThrowZlibError(strm, zlib_function_name, blurb.c_str());
      break;  // not reached
    }
  }

  return status;
}

static std::mutex SingletonInitMutex;

static std::unique_ptr<const TGzipCodec> Singleton;

const TGzipCodec &TGzipCodec::The() {
  if (!Singleton) {
    std::lock_guard<std::mutex> lock(SingletonInitMutex);

    if (!Singleton) {
      Singleton.reset(new TGzipCodec);
    }
  }

  return *Singleton;
}

static const int DEFAULT_LEVEL = Z_DEFAULT_COMPRESSION;
static const int MIN_LEVEL = Z_NO_COMPRESSION;
static const int MAX_LEVEL = Z_BEST_COMPRESSION;

TOpt<int> TGzipCodec::GetRealCompressionLevel(
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

static size_t DoUncompress(const void *compressed_data, size_t compressed_size,
    void *output_buf, size_t output_buf_size, bool preserve_output) {
  z_stream strm;
  std::memset(&strm, 0, sizeof(strm));
  CheckStatus(inflateInit2(&strm, 15 + 32), strm, "inflateInit2");
  TOnDestroy destroy_strm(
      [&strm]() noexcept {
        inflateEnd(&strm);
      }
  );
  strm.next_in =
      reinterpret_cast<Bytef *>(const_cast<void *>(compressed_data));
  strm.avail_in = static_cast<uInt>(compressed_size);
  gz_header hdr;
  CheckStatus(inflateGetHeader(&strm, &hdr), strm, "inflateGetHeader");
  auto *out_pos = reinterpret_cast<uint8_t *>(output_buf);
  size_t size = output_buf_size;

  for (; ; ) {
    strm.next_out = out_pos;
    strm.avail_out = static_cast<uInt>(size);

    if (CheckStatus(inflate(&strm, Z_NO_FLUSH), strm, "inflate") ==
        Z_STREAM_END) {
      break;
    }

    if (preserve_output) {
      if (strm.avail_out == 0) {
        /* If we get here, we were called from Uncompress() and
           'output_buf_size' is smaller than the value returned by
           ComputeUncompressedResultBufSpace(). */
        ZlibDecompressOutOfSpace.Increment();
        throw TCompressionCodecApi::TError(
            "Not enough space for zlib decompress");
      }

      size_t consumed = size - strm.avail_out;
      out_pos += consumed;
      size -= consumed;
    }
  }

  auto result_size = static_cast<size_t>(strm.total_out);

  if (result_size != strm.total_out) {
    /* Our result size is so large that downcasting it to size_t (on a platform
       for which this is a downcast) caused overflow.  Something is seriously
       wrong, and we have likely trashed memory.  Die immediately. */
    throw std::logic_error("Overflow in result size for zlib decompression");
  }

  if (preserve_output && (result_size > output_buf_size)) {
    /* This should never happen, but if it does, we have likely trashed memory.
       Die immediately. */
    throw std::logic_error("Buffer overflow during zlib decompression");
  }

  return result_size;
}

size_t TGzipCodec::ComputeUncompressedResultBufSpace(
    const void *compressed_data, size_t compressed_size) const {
  assert(this);
  uint8_t discard_buf[512];
  return DoUncompress(compressed_data, compressed_size, discard_buf,
      sizeof(discard_buf), false);
}

size_t TGzipCodec::Uncompress(const void *input_buf, size_t input_buf_size,
    void *output_buf, size_t output_buf_size) const {
  assert(this);
  return DoUncompress(input_buf, input_buf_size, output_buf, output_buf_size,
      true);
}

namespace {

  class TDeflateInitializer final {
    NO_COPY_SEMANTICS(TDeflateInitializer);

    public:
    TDeflateInitializer(z_stream &strm, int compression_level)
        : Strm(strm) {
      std::memset(&Strm, 0, sizeof(Strm));
      CheckStatus(deflateInit2(&Strm, compression_level, Z_DEFLATED, 15 + 16,
          8, Z_DEFAULT_STRATEGY), Strm, "deflateInit2");
    }

    ~TDeflateInitializer() {
      deflateEnd(&Strm);
    }

    size_t ComputeCompressedResultBufSpace(size_t uncompressed_size) {
      assert(this);
      uLong max_size = deflateBound(&Strm, uncompressed_size);
      auto result = static_cast<size_t>(max_size);

      if (result != max_size) {
        ZlibOverflowComputingCompressBufSize.Increment();
        throw TCompressionCodecApi::TError(
            "Overflow while computing zlib compression buffer size");
      }

      return result;
    }

    private:
    z_stream &Strm;
  };  // TDeflateInitializer

}  // namespace

size_t TGzipCodec::DoComputeCompressedResultBufSpace(
    const void * /*uncompressed_data*/, size_t uncompressed_size,
    int compression_level) const {
  assert(this);
  z_stream strm;
  return TDeflateInitializer(strm, compression_level)
      .ComputeCompressedResultBufSpace(uncompressed_size);
}

size_t TGzipCodec::DoCompress(const void *input_buf, size_t input_buf_size,
    void *output_buf, size_t output_buf_size,
    int compression_level) const {
  assert(this);
  z_stream strm;
  TDeflateInitializer init(strm, compression_level);
  size_t min_result_size =
      init.ComputeCompressedResultBufSpace(input_buf_size);

  if (output_buf_size < min_result_size) {
    /* This is guaranteed not to happen if 'output_buf_size' is at least as
       large as the value returned by ComputeCompressedResultBufSpace(). */
    throw std::logic_error("zlib compressed output buffer too small");
  }

  strm.next_in = reinterpret_cast<Bytef *>(const_cast<void *>(input_buf));
  strm.avail_in = static_cast<uInt>(input_buf_size);
  strm.next_out = reinterpret_cast<Bytef *>(output_buf);
  strm.avail_out = static_cast<uInt>(min_result_size);

  if (CheckStatus(deflate(&strm, Z_FINISH), strm, "deflate") != Z_STREAM_END) {
    /* According to the zlib documentation, this should never happen, given our
       usage of the API. */
    ZlibNotAtEndAfterCompress.Increment();
    throw TCompressionCodecApi::TError(
        "Should have reached end of zlib stream after compress");
  }

  auto result_size = static_cast<size_t>(strm.total_out);

  if (result_size != strm.total_out) {
    /* Our result size is so large that downcasting it to size_t (on a platform
       for which this is a downcast) caused overflow.  Something is seriously
       wrong, and we have likely trashed memory.  Die immediately. */
    throw std::logic_error("Overflow in result size for zlib compression");
  }

  if (result_size > min_result_size) {
    /* This should never happen, but if it does, we have likely trashed memory.
       Die immediately. */
    throw std::logic_error("Buffer overflow during zlib compression");
  }

  ZlibCompressSuccess.Increment();
  return result_size;
}
