/* <dory/dory_server.h>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Dory server implementation.
 */

#pragma once

#include <atomic>
#include <cassert>
#include <exception>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>

#include <netinet/in.h>

#include <base/event_semaphore.h>
#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <base/sig_handler_installer.h>
#include <base/sig_set.h>
#include <base/timer_fd.h>
#include <base/thrower.h>
#include <capped/pool.h>
#include <dory/anomaly_tracker.h>
#include <dory/batch/batch_config_builder.h>
#include <dory/batch/global_batch_config.h>
#include <dory/cmd_line_args.h>
#include <dory/conf/compression_conf.h>
#include <dory/conf/conf.h>
#include <dory/debug/debug_setup.h>
#include <dory/discard_file_logger.h>
#include <dory/unix_dg_input_agent.h>
#include <dory/metadata_timestamp.h>
#include <dory/msg_dispatch/kafka_dispatcher.h>
#include <dory/msg_state_tracker.h>
#include <dory/router_thread.h>
#include <dory/stream_client_handler.h>
#include <dory/stream_client_work_fn.h>
#include <server/stream_server_base.h>
#include <server/tcp_ipv4_server.h>
#include <server/unix_stream_server.h>
#include <thread/managed_thread_pool.h>

namespace Dory {

  class TDoryServer final {
    NO_COPY_SEMANTICS(TDoryServer);

    public:
    static bool CheckUnixDgSize(const TCmdLineArgs &args);

    /* Must be called before invoking TDoryServer constructor. */
    static void PrepareForInit(const Conf::TConf &conf);

    /* dory monitors shutdown_fd, and shuts down when it becomes readable. */
    TDoryServer(TCmdLineArgs &&args, Conf::TConf &&conf,
        const Base::TFd &shutdown_fd);

    const TCmdLineArgs &GetCmdLineArgs() const noexcept {
      assert(this);
      return CmdLineArgs;
    }

    const Conf::TConf &GetConf() const noexcept {
      assert(this);
      return Conf;
    }

    /* Used for testing. */
    const TAnomalyTracker &GetAnomalyTracker() const noexcept {
      assert(this);
      return AnomalyTracker;
    }

    /* Test code passes true for 'bind_ephemeral'. */
    void BindStatusSocket(bool bind_ephemeral = false);

    in_port_t GetStatusPort() const noexcept {
      assert(this);
      return StatusPort;
    }

    /* Return the port used by the TCP input agent, or 0 if agent is inactive.
       Do not call until server has been started.  This is intended for test
       code to use for finding the ephemeral port chosen by the kernel. */
    in_port_t GetInputPort() const noexcept {
      assert(this);
      return TcpInputAgent.IsKnown() ? TcpInputAgent->GetBindPort() : 0;
    }

    /* Return a file descriptor that becomes readable when the server has
       finished initialization or is shutting down.  Test code calls this. */
    const Base::TFd &GetInitWaitFd() const noexcept {
      assert(this);
      return InitWaitSem.GetFd();
    }

    /* This is called by test code. */
    size_t GetAckCount() const noexcept {
      assert(this);
      return Dispatcher.GetAckCount();
    }

    int Run();

    private:
    /* Thread pool type for handling local TCP and UNIX domain stream client
       connections. */
    using TWorkerPool = TStreamClientHandler::TWorkerPool;

    std::unique_ptr<Server::TStreamServerBase::TConnectionHandlerApi>
    CreateStreamClientHandler(bool is_tcp);

    /* Return true on success or false on error starting one of the input
       agents. */
    bool StartMsgHandlingThreads();

    bool HandleEvents();

    void DiscardFinalMsgs(std::list<TMsg::TPtr> &msg_list);

    bool Shutdown();

    /* For listen() system call for UNIX domain stream and local TCP sockets.
       TODO: consider providing config file option(s) for setting backlog. */
    static const int STREAM_BACKLOG = 16;

    /* Configuration obtained from command line arguments. */
    const TCmdLineArgs CmdLineArgs;

    /* Configuration obtained from config file. */
    const Conf::TConf Conf;

    const size_t PoolBlockSize;

    const Base::TFd &ShutdownFd;

    bool Started = false;

    Capped::TPool Pool;

    /* This is declared _before_ the input thread, router thread, and
       dispatcher so it gets destroyed after them.  Its destructor stops
       discard file logging, which we only want to do after everything else
       that might generate discards has been destroyed. */
    TDiscardFileLogger DiscardFileLogger;

    TMsgStateTracker MsgStateTracker;

    /* For tracking discarded messages and possible duplicates. */
    TAnomalyTracker AnomalyTracker;

    /* The only purpose of this is to prevent multiple instances of the server
       from running simultaneously.  In this case, we want to fail as early as
       possible.  Once Mongoose has started, it has the port claimed so we
       close this socket. */
    Base::TFd TmpStatusSocket;

    in_port_t StatusPort;

    Debug::TDebugSetup DebugSetup;

    MsgDispatch::TKafkaDispatcher Dispatcher;

    TRouterThread RouterThread;

    /* Thread pool for handling local TCP and UNIX domain stream client
       connections. */
    Base::TOpt<TWorkerPool> StreamClientWorkerPool;

    /* Server for handling UNIX domain datagram client messages.  This is the
       preferred way for clients to send messages to dory. */
    Base::TOpt<TUnixDgInputAgent> UnixDgInputAgent;

    /* Server for handling UNIX domain stream client connections.  This may be
       useful for clients who want to send messages too large for UNIX domain
       datagrams, or who can deal with UNIX domain stream, but not datagram,
       sockets. */
    Base::TOpt<Server::TUnixStreamServer> UnixStreamInputAgent;

    /* Server for handling local TCP client connections.  This should only be
       used by clients who are not easily able to use UNIX domain datagram or
       stream sockets. */
    Base::TOpt<Server::TTcpIpv4Server> TcpInputAgent;

    const TMetadataTimestamp &MetadataTimestamp;

    /* Becomes readable when the server has finished initialization or is
       shutting down.  Test code monitors this. */
    Base::TEventSemaphore InitWaitSem;
  };  // TDoryServer

}  // Dory
