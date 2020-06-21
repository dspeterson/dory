/* <base/time_util.h>

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

   Time-related utilities.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <base/error_util.h>
#include <base/no_copy_semantics.h>
#include <base/rate_limiter.h>
#include <base/thrower.h>

namespace Base {

  void SleepMilliseconds(size_t milliseconds) noexcept;

  void SleepMicroseconds(size_t microseconds) noexcept;

  /* Return the number of seconds since the epoch.  Fractional seconds are
     truncated. */
  uint64_t GetEpochSeconds() noexcept;

  /* Return the number of milliseconds since the epoch.  Fractional
     milliseconds are truncated. */
  uint64_t GetEpochMilliseconds() noexcept;

  /* Return the number of milliseconds since some unspecified point in the
     past.  Uses clock_gettime() with clock type of CLOCK_MONOTONIC_RAW. */
  uint64_t GetMonotonicRawMilliseconds() noexcept;

}  // Base
