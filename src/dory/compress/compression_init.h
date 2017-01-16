/* <dory/compress/compression_init.h>

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

   Compression initialization code.
 */

#pragma once

#include <dory/compress/compression_type.h>

namespace Dory {

  namespace Compress {

    /* Load the compression library for the given compression type.  Before
       starting Dory, this should be called for each compression type that Dory
       is configured to use.  The idea is to fail early in the case where a
       compression library fails to load. */
    void CompressionInit(TCompressionType type);

    /* Same as above, but don't be selective about which libraries we load.
       The mock Kafka server uses this, since it must be prepared to handle all
       supported compression types. */
    void CompressionInit();

  }  // Compress

}  // Dory
