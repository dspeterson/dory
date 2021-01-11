/* <dory/compress/compression_codec_api.h>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Compression codec interface class.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>

#include <base/no_copy_semantics.h>

namespace Dory {

  namespace Compress {

    /* Compression codec base class.  Subclasses should be completely
       stateless singletons, and their methods may be called concurrently by
       multiple threads. */
    class TCompressionCodecApi {
      NO_COPY_SEMANTICS(TCompressionCodecApi);

      public:
      /* Exception base class for errors reported by compression codec. */
      class TError : public std::runtime_error {
        public:
        ~TError() override = default;

        explicit TError(const char *what_arg)
            : std::runtime_error(what_arg) {
        }
      };  // TError

      virtual ~TCompressionCodecApi() = default;

      /* Parameter 'requested_level' is a compression level requested by the
         user, which is empty in the case where no level is requested.  Return
         a result indicating the actual compression level that the algorithm
         will use.  If the algorithm does not support compression levels, it
         will return an empty result regardless of the input.  If the algorithm
         supports compression, then it returns a result as follows:

             - If the input value is empty, it returns the default compression
               level.

             - If the input value is nonempty and specifies a level that the
               algorithm recognizes as valid, it returns the input value.

             - If the algorithm views the input value as invalid, it returns
               the default compression level.
       */
      virtual std::optional<int> GetRealCompressionLevel(
          std::optional<int> requested_level) const noexcept = 0;

      /* Return true if the algorithm supports compression levels, or false
         otherwise. */
      bool SupportsCompressionLevels() const noexcept {
        return GetRealCompressionLevel(std::nullopt).has_value();
      }

      /* Return the maximum compressed size in bytes of 'uncompressed_size'
         bytes of data in buffer 'uncompressed_data'.  Throw TError on error.
         Parameter 'compression_level' is an optional requested compression
         level. */
      size_t ComputeCompressedResultBufSpace(const void *uncompressed_data,
          size_t uncompressed_size,
          std::optional<int> compression_level) const {
        return DoComputeCompressedResultBufSpace(uncompressed_data,
            uncompressed_size, CompressionLevelParam(compression_level));
      }

      /* Compress data in 'input_buf' of size 'input_buf_size' bytes.  Place
         compressed result in 'output_buf' of size 'output_buf_size' bytes.
         Return actual size in bytes of compressed data, which will be at most
         'output_buf_size'.  Throw TError on error.  Call
         ComputeCompressedResultBufSpace() to determine how many bytes to
         allocate for 'output_buf'.  Parameter 'compression_level' is an
         optional requested compression level. */
      virtual size_t Compress(const void *input_buf, size_t input_buf_size,
          void *output_buf, size_t output_buf_size,
          std::optional<int> compression_level) const {
        return DoCompress(input_buf, input_buf_size, output_buf,
            output_buf_size, CompressionLevelParam(compression_level));
      }

      /* Return the maximum uncompressed size in bytes of 'compressed_size'
         bytes of data in buffer 'compressed_data'.  Throw TError on error. */
      virtual size_t ComputeUncompressedResultBufSpace(
          const void *compressed_data, size_t compressed_size) const = 0;

      /* Uncompress data in 'input_buf' of size 'input_buf_size' bytes.  Place
         uncompressed result in 'output_buf' of size 'output_buf_size' bytes.
         Return actual size in bytes of uncompressed data, which will be at
         most 'output_buf_size'.  Throw TError on error.  Call
         ComputeUncompressedResultBufSpace() to determine how many bytes to
         allocate for 'output_buf'. */
      virtual size_t Uncompress(const void *input_buf, size_t input_buf_size,
          void *output_buf, size_t output_buf_size) const = 0;

      protected:
      /* Note: If the algorithm supports compression levels, then
         'compression_level' is guaranteed to be a value that it considers
         valid (i.e. a result obtained from GetRealCompressionLevel()).
         Otherwise, 'compression_level' will be 0, which the algorithm ignores.
       */
      virtual size_t DoComputeCompressedResultBufSpace(
          const void *uncompressed_data, size_t uncompressed_size,
          int compression_level) const = 0;

      /* Note: If the algorithm supports compression levels, then
         'compression_level' is guaranteed to be a value that it considers
         valid (i.e. a result obtained from GetRealCompressionLevel()).
         Otherwise, 'compression_level' will be 0, which the algorithm ignores.
       */
      virtual size_t DoCompress(const void *input_buf, size_t input_buf_size,
          void *output_buf, size_t output_buf_size,
          int compression_level) const = 0;

      TCompressionCodecApi() = default;

      private:
      int CompressionLevelParam(
          std::optional<int> requested_level) const noexcept;
    };  // TCompressionCodecApi

  }  // Compress

}  // Dory
