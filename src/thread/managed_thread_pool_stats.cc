/* <thread/managed_thread_pool_stats.cc>

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

   Implements <thread/managed_thread_pool_stats.h>.
 */

#include <thread/managed_thread_pool_stats.h>

using namespace Thread;

TManagedThreadPoolStats::TManagedThreadPoolStats()
    : SetConfigCount(0),
      ReconfigCount(0),
      PruneOpCount(0),
      PrunedThreadCount(0),
      MinPrunedByOp(0),
      MaxPrunedByOp(0),
      PoolHitCount(0),
      PoolMissCount(0),
      PoolMaxSizeEnforceCount(0),
      CreateWorkerCount(0),
      PutBackCount(0),
      FinishWorkCount(0),
      QueueErrorCount(0),
      NotifyErrorCount(0),
      LiveWorkerCount(0),
      IdleWorkerCount(0) {
}
