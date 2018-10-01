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
#include <mutex>
#include <stdexcept>

#include <netinet/in.h>

#include <base/event_semaphore.h>
#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <base/timer_fd.h>
#include <base/thrower.h>
#include <capped/pool.h>
#include <dory/anomaly_tracker.h>
#include <dory/batch/global_batch_config.h>
#include <dory/conf/conf.h>
#include <dory/config.h>
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
#include <signal/handler_installer.h>
#include <thread/managed_thread_pool.h>

namespace Dory {

  class TDoryServer final {
    NO_COPY_SEMANTICS(TDoryServer);

    public:
    DEFINE_ERROR(TUnsupportedMetadataApiVersion, std::runtime_error,
        "Requested metadata API version is not supported.");

    DEFINE_ERROR(TUnsupportedProduceApiVersion, std::runtime_error,
        "Requested produce API version is not supported.");

    DEFINE_ERROR(TBadRequiredAcks, std::runtime_error,
        "required_acks value must be >= -1");

    DEFINE_ERROR(TBadReplicationTimeout, std::runtime_error,
        "replication_timeout value out of range");

    DEFINE_ERROR(TBadDiscardReportInterval, std::runtime_error,
        "discard_report_interval value must be positive");

    DEFINE_ERROR(TMaxInputMsgSizeTooSmall, std::runtime_error,
        "max_input_msg_size is too small");

    DEFINE_ERROR(TMaxInputMsgSizeTooLarge, std::runtime_error,
        "max_input_msg_size is too large");

    DEFINE_ERROR(TMustAllowLargeDatagrams, std::runtime_error, "You didn't specify allow_large_unix_datagrams, and max_input_msg_size is large enough that clients sending large datagrams will need to increase SO_SNDBUF above the default value.  Either decrease max_input_msg_size or specify allow_large_unix_datagrams.");

    DEFINE_ERROR(TBadDebugDir, std::runtime_error,
        "debug_dir must be an absolute path");

    DEFINE_ERROR(TBadDiscardLogMaxFileSize, std::runtime_error,
        "discard_log_max_file_size must be at least twice the maximum input message size.  To disable discard logfile generation, leave discard_log_path unspecified.");

    class TServerConfig final {
      NO_COPY_SEMANTICS(TServerConfig);

      public:
      TServerConfig(TServerConfig &&) = default;

      TServerConfig &operator=(TServerConfig &&) = default;

      const TConfig &GetCmdLineConfig() const {
        assert(this);
        return *Config;
      }

      private:
      TServerConfig(std::unique_ptr<const TConfig> &&config,
          Conf::TConf &&conf, Batch::TGlobalBatchConfig &&batch_config,
          size_t pool_block_size)
          : Config(std::move(config)),
            Conf(std::move(conf)),
            BatchConfig(std::move(batch_config)),
            PoolBlockSize(pool_block_size) {
      }

      std::unique_ptr<const TConfig> Config;

      Conf::TConf Conf;

      Batch::TGlobalBatchConfig BatchConfig;

      size_t PoolBlockSize;

      friend class TDoryServer;
    };  // TServerConfig

    /* Caller should instantiate this class before calling the Run() method of
       any TDoryServer instances. */
    class TSignalHandlerInstaller final {
      NO_COPY_SEMANTICS(TSignalHandlerInstaller);

      public:
      TSignalHandlerInstaller();

      private:
      Signal::THandlerInstaller SigintInstaller;

      Signal::THandlerInstaller SigtermInstaller;
    };  // TSignalHandlerInstaller

    static TServerConfig CreateConfig(int argc, char **argv,
        bool &large_sendbuf_required, bool allow_input_bind_ephemeral,
        bool enable_lz4, size_t pool_block_size = 128);

    static void HandleShutdownSignal(int signum) noexcept;

    explicit TDoryServer(TServerConfig &&config);

    ~TDoryServer();

    /* Must not be called until _after_ InitConfig() has been called. */
    size_t GetPoolBlockSize() const {
      assert(this);
      return PoolBlockSize;
    }

    const TConfig &GetConfig() const {
      assert(this);
      return *Config;
    }

    /* Used for testing. */
    const TAnomalyTracker &GetAnomalyTracker() const {
      assert(this);
      return AnomalyTracker;
    }

    /* Test code passes true for 'bind_ephemeral'. */
    void BindStatusSocket(bool bind_ephemeral = false);

    in_port_t GetStatusPort() const {
      assert(this);
      return StatusPort;
    }

    /* Return the port used by the TCP input agent, or 0 if agent is inactive.
       Do not call until server has been started.  This is intended for test
       code to use for finding the ephemeral port chosen by the kernel. */
    in_port_t GetInputPort() const {
      assert(this);
      return TcpInputAgent.IsKnown() ? TcpInputAgent->GetBindPort() : 0;
    }

    /* Return a file descriptor that becomes readable when the server has
       finished initialization or is shutting down.  Test code calls this. */
    const Base::TFd &GetInitWaitFd() const {
      assert(this);
      return InitWaitSem.GetFd();
    }

    /* This is called by test code. */
    size_t GetAckCount() const {
      assert(this);
      return Dispatcher.GetAckCount();
    }

    int Run();

    /* Called by SIGINT/SIGTERM handler.  Also called by test code to shut down
       dory. */
    void RequestShutdown() noexcept;

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
       TODO: consider providing command line option(s) for setting backlog. */
    static const int STREAM_BACKLOG = 16;

    /* Protects 'ServerList' below. */
    static std::mutex ServerListMutex;

    /* Global list of all TDoryServer objects. */
    static std::list<TDoryServer *> ServerList;

    /* Points to item in 'ServerList' for this TDoryServer object. */
    std::list<TDoryServer *>::iterator MyServerListItem;

    /* Configuration obtained from command line arguments. */
    const std::unique_ptr<const TConfig> Config;

    /* Configuration obtained from config file. */
    Conf::TConf Conf;

    const size_t PoolBlockSize;

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

    /* Set when we get a shutdown signal or test code calls RequestShutdown().
       Here we use atomic flag because test process might get signal while
       calling RequestShutdown().
     */
    std::atomic_flag ShutdownRequested{false};

    /* Becomes readable when the server has finished initialization or is
       shutting down.  Test code monitors this. */
    Base::TEventSemaphore InitWaitSem;

    /* SIGINT/SIGTERM handler pushes this. */
    Base::TEventSemaphore ShutdownRequestSem;
  };  // TDoryServer

}  // Dory
