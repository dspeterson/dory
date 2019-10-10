/* <base/wr/process_util.h>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Wrappers for process-related system/library calls.
 */

#pragma once

#include <initializer_list>

#include <sys/resource.h>
#include <sys/time.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int getrlimit(TDisp disp, std::initializer_list<int> errors, int resource,
        rlimit *rlim) noexcept;

    inline void getrlimit(int resource, rlimit *rlim) noexcept {
      const int ret = getrlimit(TDisp::AddFatal, {}, resource, rlim);
      assert(ret == 0);
    }

  }  // Wr

}  // Base
