/* <dory/stream_client_handler.cc>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/stream_client_handler.h>.
 */

#include <dory/stream_client_handler.h>

#include <cassert>
#include <utility>

#include <syslog.h>

#include <base/error_utils.h>
#include <dory/util/time_util.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Util;
using namespace Thread;

TStreamClientHandler::TStreamClientHandler(bool is_tcp, const TConfig &config,
    TPool &pool, TMsgStateTracker &msg_state_tracker,
    TAnomalyTracker &anomaly_tracker, TGatePutApi<TMsg::TPtr> &output_queue,
    TWorkerPool &worker_pool) noexcept
    : IsTcp(is_tcp),
      Config(config),
      Pool(pool),
      MsgStateTracker(msg_state_tracker),
      AnomalyTracker(anomaly_tracker),
      OutputQueue(output_queue),
      WorkerPool(worker_pool) {
}

void TStreamClientHandler::HandleConnection(Base::TFd &&sock,
    const struct sockaddr *, socklen_t) {
  assert(this);
  TWorkerPool::TReadyWorker worker = WorkerPool.GetReadyWorker();
  worker.GetWorkFn().SetState(IsTcp, Config, Pool, MsgStateTracker,
      AnomalyTracker, OutputQueue, WorkerPool.GetShutdownRequestFd(),
      std::move(sock));
  worker.Launch();
}

void TStreamClientHandler::HandleNonfatalAcceptError(int errno_value) {
  assert(this);
  static TLogRateLimiter lim(std::chrono::seconds(30));

  /* TODO: Consider implementing rate limiting on a per-errno-value basis. */
  if (lim.Test()) {
    char buf[256];
    syslog(LOG_ERR, "Error accepting %s input connection: %s",
        IsTcp ? "TCP" : "UNIX stream",
        Strerror(errno_value, buf, sizeof(buf)));
  }
}
