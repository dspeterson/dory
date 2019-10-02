/* <base/thread_safe_rate_limiter.h>

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

   Thread-safe class for limiting the rate of occurrence of an event.
 */

#pragma once

#include <cassert>
#include <mutex>

#include <base/no_copy_semantics.h>
#include <base/rate_limiter.h>

namespace Base {

  /* TRateLimiter wrapped in a mutex for thread safety. */
  template <typename TTimePoint, typename TDuration>
  class TThreadSafeRateLimiter final {
    NO_COPY_SEMANTICS(TThreadSafeRateLimiter);

    public:
    using TClockFn = typename TRateLimiter<TTimePoint, TDuration>::TClockFn;

    TThreadSafeRateLimiter(const TClockFn &clock_fn, TDuration min_interval)
        : Limiter(clock_fn, min_interval) {
    }

    bool Test() noexcept {
      assert(this);
      std::lock_guard<std::mutex> lock(Mutex);
      return Limiter.Test();
    }

    private:
    std::mutex Mutex;

    TRateLimiter<TTimePoint, TDuration> Limiter;
  };

}  // Base
