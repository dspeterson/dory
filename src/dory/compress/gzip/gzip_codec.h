/* <dory/compress/gzip/gzip_codec.h>

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

   gzip compression codec.
 */

#pragma once

#include <cstddef>

#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <dory/compress/compression_codec_api.h>

namespace Dory {

  namespace Compress {

    namespace Gzip {

      class TGzipCodec final : public TCompressionCodecApi {
        NO_COPY_SEMANTICS(TGzipCodec);

        public:
        static const TGzipCodec &The();  // singleton accessor

        virtual ~TGzipCodec() noexcept { }

        virtual Base::TOpt<int> GetRealCompressionLevel(
            const Base::TOpt<int> &requested_level) const noexcept override;

        virtual size_t ComputeUncompressedResultBufSpace(
            const void *compressed_data,
            size_t compressed_size) const override;

        virtual size_t Uncompress(const void *input_buf, size_t input_buf_size,
            void *output_buf, size_t output_buf_size) const override;

        protected:
        virtual size_t DoComputeCompressedResultBufSpace(
            const void *uncompressed_data, size_t uncompressed_size,
            int compression_level) const override;

        virtual size_t DoCompress(const void *input_buf, size_t input_buf_size,
            void *output_buf, size_t output_buf_size,
            int compression_level) const override;

        private:
        TGzipCodec() = default;
      };  // TGzipCodec

    }  // Gzip

  }  // Compress

}  // Dory
