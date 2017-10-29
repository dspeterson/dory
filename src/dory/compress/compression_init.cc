/* <dory/compress/compression_init.cc>

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

   Implements <dory/compress/compression_init.h>.
 */

#include <dory/compress/compression_init.h>

#include <dory/compress/get_compression_codec.h>
#include <dory/compress/snappy/snappy_codec.h>

using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Snappy;

void Dory::Compress::CompressionInit(TCompressionType type) {
  GetCompressionCodec(type);
}

void Dory::Compress::CompressionInit() {
  /* Force all supported compression libraries that we access using dlopen()
     and dlsym() to load.  We dynamically link to zlib at build time
     (i.e. -lz), and we statically link to the lz4 library, so they don't
     appear here.  This will throw if there is an error loading a library. */
  TSnappyCodec::The();
}
