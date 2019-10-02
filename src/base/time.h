/* <base/time.h>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   Provide a time class that wraps struct timespec and provides operators.
 */

#pragma once

#include <cstddef>

#include <time.h>

#include <base/no_copy_semantics.h>
#include <base/wr/time_util.h>

namespace Base {

  class TTime {
    public:
    TTime() noexcept {
      Time.tv_sec = 0;
      Time.tv_nsec = 0;
    }

    TTime(time_t sec, long nsec) noexcept {
      Time.tv_sec = sec;
      Time.tv_nsec = nsec;
    }

    void Now(clockid_t clk_id = CLOCK_REALTIME) noexcept {
      Wr::clock_gettime(clk_id, &Time);
    }

    size_t Remaining(clockid_t clk_id = CLOCK_REALTIME) const noexcept {
      TTime t;
      t.Now(clk_id);
      if (*this <= t) return 0;
      TTime diff = *this - t;
      return diff.Sec() * 1000 + diff.Nsec() / 1000000;
    }

    size_t RemainingMicroseconds(
        clockid_t clk_id = CLOCK_REALTIME) const noexcept {
      TTime t;
      t.Now(clk_id);

      if (*this <= t) {
        return 0;
      }

      TTime diff = *this - t;
      return diff.Sec() * 1000000 + diff.Nsec() / 1000;
    }

    time_t Sec() const noexcept {
      return Time.tv_sec;
    }

    long Nsec() const noexcept {
      return Time.tv_nsec;
    }

    TTime &operator=(const TTime &rhs) noexcept;

    bool operator==(const TTime &rhs) const noexcept;

    bool operator!=(const TTime &rhs) const noexcept;

    bool operator<(const TTime &rhs) const noexcept;

    bool operator>(const TTime &rhs) const noexcept;

    bool operator<=(const TTime &rhs) const noexcept;

    bool operator>=(const TTime &rhs) const noexcept;

    TTime &operator+=(const TTime &rhs) noexcept;

    TTime &operator-=(const TTime &rhs) noexcept;

    const TTime operator+(const TTime &rhs) const noexcept;

    const TTime operator-(const TTime &rhs) const noexcept;

    TTime &operator+=(size_t msec) noexcept;

    TTime &operator-=(size_t msec) noexcept;

    TTime &AddMicroseconds(size_t usec) noexcept;

    TTime &SubtractMicroseconds(size_t usec) noexcept;

    private:
    struct timespec Time;
  };  // TTime

}  // Base
