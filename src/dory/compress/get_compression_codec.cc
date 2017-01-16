/* <dory/compress/get_compression_codec.cc>

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

   Implements <dory/compress/get_compression_codec.h>.
 */

#include <dory/compress/get_compression_codec.h>

#include <base/no_default_case.h>
#include <dory/compress/snappy/snappy_codec.h>

using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Snappy;

const TCompressionCodecApi *
Dory::Compress::GetCompressionCodec(TCompressionType type) {
  switch (type) {
    case TCompressionType::None:
      break;
    case TCompressionType::Snappy:
      return &TSnappyCodec::The();
    NO_DEFAULT_CASE;
  }

  return nullptr;
}
