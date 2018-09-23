/* <dory/compress/snappy/snappy_codec.h>

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

   Snappy compression codec.
 */

#pragma once

#include <cstddef>

#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <dory/compress/compression_codec_api.h>

namespace Dory {

  namespace Compress {

    namespace Snappy {

      class TLibSnappy;

      class TSnappyCodec final : public TCompressionCodecApi {
        NO_COPY_SEMANTICS(TSnappyCodec);

        public:
        static const TSnappyCodec &The();  // singleton accessor

        ~TSnappyCodec() override = default;

        Base::TOpt<int> GetRealCompressionLevel(
            const Base::TOpt<int> &requested_level) const noexcept override;

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
        TSnappyCodec();

        const TLibSnappy &Lib;
      };  // TSnappyCodec

    }  // Snappy

  }  // Compress

}  // Dory
