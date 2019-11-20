/* <dory/stream_client_handler.h>

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

   Handler class for new connection from UNIX domain stream or local TCP
   client.
 */

#pragma once

#include <base/no_copy_semantics.h>
#include <dory/stream_client_work_fn.h>
#include <server/stream_server_base.h>
#include <thread/managed_thread_pool.h>

namespace Dory {

  class TStreamClientHandler final
      : public Server::TStreamServerBase::TConnectionHandlerApi {
    NO_COPY_SEMANTICS(TStreamClientHandler);

    public:
    using TWorkerPool = Thread::TManagedThreadPool<TStreamClientWorkFn>;

    TStreamClientHandler(bool is_tcp, const TCmdLineArgs &config,
        Capped::TPool &pool, TMsgStateTracker &msg_state_tracker,
        TAnomalyTracker &anomaly_tracker,
        Thread::TGatePutApi<TMsg::TPtr> &output_queue,
        TWorkerPool &worker_pool) noexcept;

    void HandleConnection(Base::TFd &&sock,
        const struct sockaddr *addr, socklen_t addr_len) override;

    void HandleNonfatalAcceptError(int errno_value) override;

    private:
    /* true indicates that we are handling a local TCP connection.  false
       indicates that we are handling a UNIX domain stream connection. */
    const bool IsTcp;

    const TCmdLineArgs &Config;

    /* Blocks for TBlob objects containing message data get allocated from
       here. */
    Capped::TPool &Pool;

    TMsgStateTracker &MsgStateTracker;

    /* For tracking discarded messages and possible duplicates. */
    TAnomalyTracker &AnomalyTracker;

    /* Messages are queued here for the router thread. */
    Thread::TGatePutApi<TMsg::TPtr> &OutputQueue;

    /* We allocate workers from this thread pool to handle client
       connections. */
    TWorkerPool &WorkerPool;
  };  // TStreamClientHandler

}  // Dory
