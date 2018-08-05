/* <thread/managed_thread_pool_config.h>

   ----------------------------------------------------------------------------
   Copyright 2018 Dave Peterson <dave@dspeterson.com>

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

   Managed thread pool config class.
 */

#pragma once

#include <cassert>
#include <cstddef>

namespace Thread {

  /* Managed thread pool config class. */
  class TManagedThreadPoolConfig final {
    public:
    TManagedThreadPoolConfig();

    /* Config parameters:

           min_pool_size: Prevents pool manager from pruning threads if after
               pruning, the pool size (active + idle) would be below this
               limit.  Must be >= 0.  Default value is 0.

           max_pool_size: A value > 0 specifies the maximum total # of
               threads (not including the manager) that the pool may contain.
               A value of 0 specifies no upper bound.  Default value is 0.

           prune_quantum_ms: The prune interval length in milliseconds.  At the
               end of each interval, the manager thread wakes up and sees if
               there is anything to prune.  Must be > 0.  Default value is
               30000.

           prune_quantum_count: The number of intervals in the pool's idle
               list.  Each interval corresponds to a time quantum whose length
               is 'prune_quantum_ms'.  The manager only prunes threads in the
               oldest quantum.  Must be > 0.  See TSegmentedList for details.
               Default value is 10.

           max_prune_fraction: Must be >= 0 and <= 1000.  Prevents the manager
               from performing a pruning operation that would destroy more than
               this many thousandths of the total pool size (active + idle),
               _unless_ the operation prunes only a single thread and
               (max_prune_fraction > 0).  For instance, a value of 500 and a
               pool size of 100 would allow pruning up to 50 threads.  As
               another example, a value of 500 and a pool size of 1 would allow
               the single thread to be pruned even though this would destroy
               more than 500 thousandths of the pool size, since pruning a
               single thread is always allowed as long as (max_prune_fraction >
               0).  Setting max_prune_fraction to 0 disables pruning.  Default
               value is 500.

           min_idle_fraction: Must be >= 0 and <= 1000.  Prevents the manager
               from performing a pruning operation that would leave fewer than
               this many thousandths of the total pool size idle.  For
               instance, a value of 15 would prevent a pruning operation that
               would leave fewer than 1.5 percent of the worker threads idle.
               Default value is 20.
     */
    TManagedThreadPoolConfig(size_t min_pool_size, size_t max_pool_size,
        size_t prune_quantum_ms, size_t prune_quantum_count,
        size_t max_prune_fraction, size_t min_idle_fraction);

    TManagedThreadPoolConfig(const TManagedThreadPoolConfig &) = default;

    TManagedThreadPoolConfig &operator=(
        const TManagedThreadPoolConfig &that) = default;

    bool operator==(const TManagedThreadPoolConfig &that) const noexcept;

    bool operator!=(const TManagedThreadPoolConfig &that) const noexcept {
      assert(this);
      return !(*this == that);
    }

    size_t GetMinPoolSize() const noexcept {
      assert(this);
      return MinPoolSize;
    }

    void SetMinPoolSize(size_t min_pool_size) noexcept {
      assert(this);
      MinPoolSize = min_pool_size;
    }

    size_t GetMaxPoolSize() const noexcept {
      assert(this);
      return MaxPoolSize;
    }

    void SetMaxPoolSize(size_t max_pool_size) noexcept {
      assert(this);
      MaxPoolSize = max_pool_size;
    }

    size_t GetPruneQuantumMs() const noexcept {
      assert(this);
      return PruneQuantumMs;
    }

    void SetPruneQuantumMs(size_t prune_quantum_ms);

    size_t GetPruneQuantumCount() const noexcept {
      assert(this);
      return PruneQuantumCount;
    }

    void SetPruneQuantumCount(size_t prune_quantum_count);

    size_t GetMaxPruneFraction() const noexcept {
      assert(this);
      return MaxPruneFraction;
    }

    void SetMaxPruneFraction(size_t max_prune_fraction);

    size_t GetMinIdleFraction() const noexcept {
      assert(this);
      return MinIdleFraction;
    }

    void SetMinIdleFraction(size_t min_idle_fraction);

    private:
    size_t MinPoolSize;

    size_t MaxPoolSize;

    size_t PruneQuantumMs;

    size_t PruneQuantumCount;

    size_t MaxPruneFraction;

    size_t MinIdleFraction;
  };  // TManagedThreadPoolConfig

}  // Thread
