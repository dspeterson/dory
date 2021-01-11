/* <dory/compress/lz4/lz4_codec.h>

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

   LZ4 compression codec.
 */

#pragma once

#include <cstddef>
#include <optional>

#include <base/no_copy_semantics.h>
#include <dory/compress/compression_codec_api.h>

namespace Dory {

  namespace Compress {

    namespace Lz4 {

      /* Warning: This code is designed to work only with Kafka broker versions
         >= 0.10.0.0, due to a bug in a previous Kafka version.  See
         https://cwiki.apache.org/confluence/display/KAFKA/KIP-57+-+Interoperable+LZ4+Framing
         for details.  For simplicity, the intent is to not support lz4
         compression for broker versions < 0.10.0.0. */
      class TLz4Codec final : public TCompressionCodecApi {
        NO_COPY_SEMANTICS(TLz4Codec);

        public:
        static const TLz4Codec &The();  // singleton accessor

        ~TLz4Codec() override = default;

        std::optional<int> GetRealCompressionLevel(
            std::optional<int> requested_level) const noexcept override;

        size_t ComputeUncompressedResultBufSpace(const void *compressed_data,
            size_t compressed_size) const override;

        size_t Uncompress(const void *input_buf, size_t input_buf_size,
            void *output_buf, size_t output_buf_size) const override;

        protected:
        size_t DoComputeCompressedResultBufSpace(const void *uncompressed_data,
            size_t uncompressed_size, int compression_level) const override;

        size_t DoCompress(const void *input_buf, size_t input_buf_size,
            void *output_buf, size_t output_buf_size,
            int compression_level) const override;

        private:
        TLz4Codec() = default;
      };  // TLz4Codec

    }  // Lz4

  }  // Compress

}  // Dory
