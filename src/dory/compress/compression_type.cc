/* <dory/compress/compression_type.cc>

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

   Implements <dory/compress/compression_type.h>.
 */

#include <dory/compress/compression_type.h>

#include <base/no_default_case.h>

using namespace Dory;
using namespace Dory::Compress;

const char *Dory::Compress::ToString(TCompressionType type) noexcept {
  switch (type) {
    case TCompressionType::None: {
      break;
    }
    case TCompressionType::Gzip: {
      return "gzip";
    }
    case TCompressionType::Snappy: {
      return "snappy";
    }
    case TCompressionType::Lz4: {
      return "lz4";
    }
    NO_DEFAULT_CASE;
  }

  return "none";
}
