/* <dory/util/misc_util.h>

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

   Utility functions.
 */

#pragma once

#include <cstddef>
#include <string>

#include <log/pri.h>

namespace Dory {

  namespace Util {

    void InitLogging(const char *prog_name, Log::TPri max_level,
        bool log_echo, const char *logfile_path);

    /* Result of call to TestUnixDgSize() below. */
    enum class TUnixDgSizeTestResult {
      /* Test passed with default value for SO_SNDBUF. */
      Pass,

      /* Test passed after setting SO_SNDBUF to size of test datagram. */
      PassWithLargeSendbuf,

      /* Test failed. */
      Fail
    };

    /* Attempt to send and receive a UNIX domain datagram of 'size' bytes.
       Return true on success or false on failure.  Throw on fatal system
       error.  */
    TUnixDgSizeTestResult TestUnixDgSize(size_t size);

  }  // Util

}  // Dory
