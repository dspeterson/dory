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

#include <utility>

#include <log/log.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Conf;
using namespace Log;
using namespace Thread;

TStreamClientHandler::TStreamClientHandler(bool is_tcp,
    const TConf &conf, TPool &pool, TMsgStateTracker &msg_state_tracker,
    TAnomalyTracker &anomaly_tracker, TGatePutApi<TMsg::TPtr> &output_queue,
    TWorkerPool &worker_pool) noexcept
    : IsTcp(is_tcp),
      Conf(conf),
      Pool(pool),
      MsgStateTracker(msg_state_tracker),
      AnomalyTracker(anomaly_tracker),
      OutputQueue(output_queue),
      WorkerPool(worker_pool) {
}

void TStreamClientHandler::HandleConnection(Base::TFd &&sock,
    const struct sockaddr *, socklen_t) {
  TWorkerPool::TReadyWorker worker = WorkerPool.GetReadyWorker();
  worker.GetWorkFn().SetState(IsTcp, Conf, Pool, MsgStateTracker,
      AnomalyTracker, OutputQueue, WorkerPool.GetShutdownRequestFd(),
      std::move(sock));
  worker.Launch();
}

void TStreamClientHandler::HandleNonfatalAcceptError(int errno_value) {
  /* TODO: Consider implementing rate limiting on a per-errno-value basis. */
  LOG_ERRNO_R(TPri::ERR, errno_value, std::chrono::seconds(30))
      << "Error accepting " << (IsTcp ? "TCP" : "UNIX stream")
      << " input connection: ";
}
