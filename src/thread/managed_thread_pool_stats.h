/* <thread/managed_thread_pool_stats.h>

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

   Managed thread pool stats class.
 */

#pragma once

#include <cstddef>

namespace Thread {

  /* Statistics on managed thread pool operation. */
  struct TManagedThreadPoolStats final {
    /* # of times SetConfig() is called with new config. */
    size_t SetConfigCount = 0;

    /* # of times manager does reconfig (may be less than # of times
       SetConfig() is called). */
    size_t ReconfigCount = 0;

    /* # of prune operations performed by manager.  A single prune operation
       prunes 0 or more (possibly many) threads. */
    size_t PruneOpCount = 0;

    /* Total # of threads pruned. */
    size_t PrunedThreadCount = 0;

    /* Minimum # of threads pruned in a single operation. */
    size_t MinPrunedByOp = 0;

    /* Maximum # of threads pruned in a single operation. */
    size_t MaxPrunedByOp = 0;

    /* # of times a worker was successfully allocated from pool. */
    size_t PoolHitCount = 0;

    /* # of times a new worker was created because the pool had no idle
       workers.  This will be less than 'CreateWorkerCount' in the case where
       the pool was populated with an initial set of workers on startup, before
       handling any requests for workers. */
    size_t PoolMissCount = 0;

    /* # of times a worker was not obtained from the pool due to the configured
       size limit. */
    size_t PoolMaxSizeEnforceCount = 0;

    /* # of times a new worker is created.  This includes pool misses and
      threads created to initially populate pool. */
    size_t CreateWorkerCount = 0;

    /* # of times a worker is released without being launched.  Note that the
       worker will not have an actual thread in the case where the worker was
       just created due to the idle list being empty.  In this case the worker
       gets immediately destroyed rather than moving to the idle list.
       Likewise, If the pool is shutting down, the worker will not be placed on
       the idle list even if it actually contains an actual thread.  For both
       of these reasons, this value may be greater than 'PrunedThreadCount'
       once the pool has finished shutting down. */
    size_t PutBackCount = 0;

    /* # of times a thread finishes work. */
    size_t FinishWorkCount = 0;

    /* # of times a worker error is queued for receipt by client. */
    size_t QueueErrorCount = 0;

    /* # of times the client is notified of a queued worker error. */
    size_t NotifyErrorCount = 0;

    /* # of busy or idle workers. */
    size_t LiveWorkerCount = 0;

    /* # of idle workers. */
    size_t IdleWorkerCount = 0;

    TManagedThreadPoolStats() = default;
  };  // TManagedThreadPoolStats

}  // Thread
