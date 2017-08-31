/* <dory/stream_client_work_fn.h>

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

   Thread pool work function class for UNIX domain stream or local TCP client.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <base/buf.h>
#include <base/fd.h>
#include <base/field_access.h>
#include <base/stream_msg_with_size_reader.h>
#include <capped/pool.h>
#include <dory/anomaly_tracker.h>
#include <dory/config.h>
#include <dory/msg_state_tracker.h>
#include <dory/util/poll_array.h>
#include <thread/gate_put_api.h>

namespace Dory {

  class TStreamClientWorkFn final {
    NO_COPY_SEMANTICS(TStreamClientWorkFn);

    public:
    explicit TStreamClientWorkFn(nullptr_t) noexcept;

    TStreamClientWorkFn &operator=(nullptr_t) noexcept;

    void operator()();

    void SetState(bool is_tcp, const TConfig &config, Capped::TPool &pool,
        TMsgStateTracker &msg_state_tracker, TAnomalyTracker &anomaly_tracker,
        Thread::TGatePutApi<TMsg::TPtr> &output_queue,
        const Base::TFd &shutdown_request_fd,
        Base::TFd &&client_socket) noexcept;

    private:
    enum class TMainLoopPollItem {
      Sock = 0,
      ShutdownRequest = 1
    };  // TMainLoopPollItem

    void HandleClientClosed() const;

    void HandleDataInvalid();

    bool HandleSockReadReady();

    /* true indicates that we are handling a local TCP connection.  false
       indicates that we are handling a UNIX domain stream connection. */
    bool IsTcp;

    const TConfig *Config;

    /* Blocks for TBlob objects containing message data get allocated from
       here. */
    Capped::TPool *Pool;

    TMsgStateTracker *MsgStateTracker;

    /* For tracking discarded messages and possible duplicates. */
    TAnomalyTracker *AnomalyTracker;

    /* Messages are queued here for the router thread. */
    Thread::TGatePutApi<TMsg::TPtr> *OutputQueue;

    /* Becomes readable when thread pool receives a shutdown request. */
    const Base::TFd *ShutdownRequestFd;

    /* UNIX domain stream or local TCP socket connected to client. */
    Base::TFd ClientSocket;

    using TStreamReader = Base::TStreamMsgWithSizeReader<int32_t>;

    /* This handles the details of reading messages from the client socket. */
    TStreamReader StreamReader;

    /* Used for poll() system call in main loop. */
    Util::TPollArray<TMainLoopPollItem, 2> MainLoopPollArray;
  };  // TStreamClientWorkFn

}  // Dory
