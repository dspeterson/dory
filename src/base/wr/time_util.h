/* <base/wr/time_util.h>

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

   Wrappers for time-related system/library calls.
 */

#pragma once

#include <cassert>
#include <ctime>
#include <initializer_list>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int clock_gettime(TDisp disp, std::initializer_list<int> errors,
        clockid_t clk_id, timespec *tp) noexcept;

    inline void clock_gettime(clockid_t clk_id, timespec *tp) noexcept {
      const int ret = clock_gettime(TDisp::AddFatal, {}, clk_id, tp);
      assert(ret == 0);
    }

    int nanosleep(TDisp disp, std::initializer_list<int> errors,
        const timespec *req, timespec *rem) noexcept;

    /* Return false on successful sleep for requested interval, or true on
       interruption by signal.  All other errors are fatal. */
    inline bool nanosleep(const timespec *req, timespec *rem) noexcept {
      const int ret = nanosleep(TDisp::AddFatal, {}, req, rem);
      return (ret != 0);
    }

  }  // Wr

}  // Base
