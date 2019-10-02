/* <base/time_util.cc>

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

   Implements <base/time_util.h>.
 */

#include <base/time_util.h>

#include <cerrno>

#include <time.h>

#include <base/wr/time_util.h>

using namespace Base;

namespace Base {

  void SleepMilliseconds(size_t milliseconds) noexcept {
    if (milliseconds == 0) {
      return;
    }

    struct timespec delay, remaining;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = (milliseconds % 1000) * 1000000;

    while (Wr::nanosleep(&delay, &remaining)) {
      delay = remaining;
    }
  }

  void SleepMicroseconds(size_t microseconds) noexcept {
    if (microseconds == 0) {
      return;
    }

    struct timespec delay, remaining;
    delay.tv_sec = microseconds / 1000000;
    delay.tv_nsec = (microseconds % 1000000) * 1000;

    while (Wr::nanosleep(&delay, &remaining)) {
      delay = remaining;
    }
  }

  uint64_t GetEpochSeconds() noexcept {
    struct timespec t;
    Wr::clock_gettime(CLOCK_REALTIME, &t);
    return static_cast<uint64_t>(t.tv_sec);
  }

  uint64_t GetEpochMilliseconds() noexcept {
    struct timespec t;
    Wr::clock_gettime(CLOCK_REALTIME, &t);
    return (static_cast<uint64_t>(t.tv_sec) * 1000) + (t.tv_nsec / 1000000);
  }

  uint64_t GetMonotonicRawMilliseconds() noexcept {
    struct timespec t;
    Wr::clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (static_cast<uint64_t>(t.tv_sec) * 1000) + (t.tv_nsec / 1000000);
  }

}  // Base
