/* <thread/managed_thread_pool_config.cc>

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

   Implements <thread/managed_thread_pool_config.h>.
 */

#include <thread/managed_thread_pool_config.h>

#include <stdexcept>

using namespace Thread;

TManagedThreadPoolConfig::TManagedThreadPoolConfig(size_t min_pool_size,
    size_t max_pool_size, size_t prune_quantum_ms, size_t prune_quantum_count,
    size_t max_prune_fraction, size_t min_idle_fraction)
    : MinPoolSize(min_pool_size),
      MaxPoolSize(max_pool_size),
      PruneQuantumMs(prune_quantum_ms),
      PruneQuantumCount(prune_quantum_count),
      MaxPruneFraction(max_prune_fraction),
      MinIdleFraction(min_idle_fraction) {
  if (PruneQuantumMs == 0) {
    throw std::logic_error("PruneQuantumMs must be > 0");
  }

  if (PruneQuantumCount == 0) {
    throw std::logic_error("PruneQuantumCount must be > 0");
  }

  if (MaxPruneFraction > 1000) {
    throw std::logic_error("MaxPruneFraction must be <= 1000");
  }

  if (MinIdleFraction > 1000) {
    throw std::logic_error("MinIdleFraction must be <= 1000");
  }
}

bool TManagedThreadPoolConfig::operator==(
    const TManagedThreadPoolConfig &that) const noexcept {
  assert(this);
  return (MinPoolSize == that.MinPoolSize) &&
      (PruneQuantumMs == that.PruneQuantumMs) &&
      (PruneQuantumCount == that.PruneQuantumCount) &&
      (MaxPruneFraction == that.MaxPruneFraction) &&
      (MinIdleFraction == that.MinIdleFraction);
}

void TManagedThreadPoolConfig::SetPruneQuantumMs(size_t prune_quantum_ms) {
  assert(this);

  if (prune_quantum_ms == 0) {
    throw std::logic_error("PruneQuantumMs must be > 0");
  }

  PruneQuantumMs = prune_quantum_ms;
}

void TManagedThreadPoolConfig::SetPruneQuantumCount(
    size_t prune_quantum_count) {
  assert(this);

  if (prune_quantum_count == 0) {
    throw std::logic_error("PruneQuantumCount must be > 0");
  }

  PruneQuantumCount = prune_quantum_count;
}

void TManagedThreadPoolConfig::SetMaxPruneFraction(size_t max_prune_fraction) {
  assert(this);

  if (max_prune_fraction > 1000) {
    throw std::logic_error("MaxPruneFraction must be <= 1000");
  }

  MaxPruneFraction = max_prune_fraction;
}

void TManagedThreadPoolConfig::SetMinIdleFraction(size_t min_idle_fraction) {
  assert(this);

  if (min_idle_fraction > 1000) {
    throw std::logic_error("MinIdleFraction must be <= 1000");
  }

  MinIdleFraction = min_idle_fraction;
}
