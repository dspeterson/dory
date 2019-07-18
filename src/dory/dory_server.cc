/* <dory/dory_server.cc>

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

   Implements <dory/dory_server.h>.
 */

#include <dory/dory_server.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>

#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <base/error_utils.h>
#include <base/no_default_case.h>
#include <capped/pool.h>
#include <dory/batch/batch_config_builder.h>
#include <dory/compress/compression_init.h>
#include <dory/conf/batch_conf.h>
#include <dory/conf/compression_conf.h>
#include <dory/discard_file_logger.h>
#include <dory/kafka_proto/metadata/version_util.h>
#include <dory/kafka_proto/produce/version_util.h>
#include <dory/msg.h>
#include <dory/util/init_notifier.h>
#include <dory/util/misc_util.h>
#include <dory/web_interface.h>
#include <log/log.h>
#include <server/counter.h>
#include <server/daemonize.h>
#include <signal/masker.h>
#include <signal/set.h>
#include <socket/address.h>
#include <socket/option.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Batch;
using namespace Dory::Compress;
using namespace Dory::Conf;
using namespace Dory::KafkaProto::Metadata;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::Util;
using namespace Log;
using namespace Server;
using namespace Signal;
using namespace Socket;
using namespace Thread;

SERVER_COUNTER(GotShutdownSignal);
SERVER_COUNTER(StreamClientWorkerStdException);
SERVER_COUNTER(StreamClientWorkerUnknownException);

static void LoadCompressionLibraries(const TCompressionConf &conf) {
  std::set<TCompressionType> in_use;
  in_use.insert(conf.GetDefaultTopicConfig().Type);
  const TCompressionConf::TTopicMap &topic_map = conf.GetTopicConfigs();

  for (const auto &item : topic_map) {
    in_use.insert(item.second.Type);
  }

  /* For each needed compression type, force the associated compression library
     to load.  This will throw if there is an error loading a library. */
  for (TCompressionType type : in_use) {
    CompressionInit(type);
  }
}

static bool CheckUnixDgSize(const TConfig &cfg) {
  bool large_sendbuf_required = false;

  if (!cfg.ReceiveSocketName.empty()) {
    switch (TestUnixDgSize(cfg.MaxInputMsgSize)) {
      case TUnixDgSizeTestResult::Pass:
        break;
      case TUnixDgSizeTestResult::PassWithLargeSendbuf:
        large_sendbuf_required = true;

        if (!cfg.AllowLargeUnixDatagrams) {
          THROW_ERROR(TDoryServer::TMustAllowLargeDatagrams);
        }

        break;
      case TUnixDgSizeTestResult::Fail:
        THROW_ERROR(TDoryServer::TMaxInputMsgSizeTooLarge);
        break;
      NO_DEFAULT_CASE;
    }
  }

  return large_sendbuf_required;
}

TDoryServer::TSignalHandlerInstaller::TSignalHandlerInstaller()
    : SigintInstaller(SIGINT, &HandleShutdownSignal),
      SigtermInstaller(SIGTERM, &HandleShutdownSignal) {
}

TDoryServer::TServerConfig
TDoryServer::CreateConfig(int argc, char **argv, bool &large_sendbuf_required,
    bool allow_input_bind_ephemeral, bool enable_lz4, size_t pool_block_size) {
  std::unique_ptr<TConfig> cfg(
      new TConfig(argc, argv, allow_input_bind_ephemeral));
  large_sendbuf_required = CheckUnixDgSize(*cfg);

  if (cfg->DiscardReportInterval < 1) {
    THROW_ERROR(TBadDiscardReportInterval);
  }

  if (cfg->RequiredAcks < -1) {
    THROW_ERROR(TBadRequiredAcks);
  }

  if (cfg->ReplicationTimeout >
      static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    THROW_ERROR(TBadReplicationTimeout);
  }

  if (cfg->DebugDir.empty() || (cfg->DebugDir[0] != '/')) {
    THROW_ERROR(TBadDebugDir);
  }

  if (cfg->DiscardLogMaxFileSize == 0) {
    THROW_ERROR(TBadDiscardLogMaxFileSize);
  }

  if (!cfg->DiscardLogPath.empty() &&
      ((cfg->DiscardLogMaxFileSize * 1024) < (2 * cfg->MaxInputMsgSize))) {
    /* We enforce this requirement to ensure that the discard logfile has
       enough capacity for at least one message of the maximum possible size.
       Multiplying by 2 ensures that there is more than enough space for the
       extra information in a log entry beyond just the message content. */
    THROW_ERROR(TBadDiscardLogMaxFileSize);
  }

  /* Verify that Dory supports any requested API version(s).  Once Dory has
     started, cases where the brokers don't support a requested API version
     will be handled. */

  if (cfg->MetadataApiVersion.IsKnown() &&
      !IsMetadataApiVersionSupported(*cfg->MetadataApiVersion)) {
    THROW_ERROR(TUnsupportedMetadataApiVersion);
  }

  if (cfg->ProduceApiVersion.IsKnown() &&
      !IsProduceApiVersionSupported(*cfg->ProduceApiVersion)) {
    THROW_ERROR(TUnsupportedProduceApiVersion);
  }

  Conf::TConf conf = Conf::TConf::TBuilder(enable_lz4).Build(
      cfg->ConfigPath.c_str());
  TGlobalBatchConfig batch_config =
      TBatchConfigBuilder().BuildFromConf(conf.GetBatchConf());

  /* Load any compression libraries we need, according to the compression info
     from our config file.  This will throw if a library fails to load.  We
     want to fail early if there is a problem loading a library. */
  LoadCompressionLibraries(conf.GetCompressionConf());

  /* The TDoryServer constructor will use the random number generator, so
     initialize it now. */
  struct timespec t;
  IfLt0(clock_gettime(CLOCK_MONOTONIC_RAW, &t));
  std::srand(static_cast<unsigned>(t.tv_sec ^ t.tv_nsec));

  return TServerConfig(std::move(cfg), std::move(conf),
      std::move(batch_config), pool_block_size);
}

void TDoryServer::HandleShutdownSignal(int /*signum*/) noexcept {
  std::lock_guard<std::mutex> lock(ServerListMutex);

  for (TDoryServer *server: ServerList) {
    assert(server);
    server->RequestShutdown();
  }
}

static void WorkerPoolFatalErrorHandler(const char *msg) noexcept {
  LOG(TPri::ERR) << "Fatal worker pool error: " << msg;
  _exit(1);
}

static void UnixStreamServerFatalErrorHandler(const char *msg) noexcept {
  LOG(TPri::ERR) << "Fatal UNIX stream input agent error: " << msg;
  _exit(1);
}

static void TcpServerFatalErrorHandler(const char *msg) noexcept {
  LOG(TPri::ERR) << "Fatal TCP input agent error: " << msg;
  _exit(1);
}

static inline size_t
ComputeBlockCount(size_t max_buffer_kb, size_t block_size) {
  return std::max<size_t>(1, (1024 * max_buffer_kb) / block_size);
}

TDoryServer::TDoryServer(TServerConfig &&config)
    : Config(std::move(config.Config)),
      Conf(std::move(config.Conf)),
      PoolBlockSize(config.PoolBlockSize),
      Pool(PoolBlockSize,
           ComputeBlockCount(Config->MsgBufferMax, PoolBlockSize),
           Capped::TPool::TSync::Mutexed),
      AnomalyTracker(DiscardFileLogger, Config->DiscardReportInterval,
                     Config->DiscardReportBadMsgPrefixSize),
      StatusPort(0),
      DebugSetup(Config->DebugDir.c_str(), Config->MsgDebugTimeLimit,
                 Config->MsgDebugByteLimit),
      Dispatcher(*Config, Conf.GetCompressionConf(), MsgStateTracker,
          AnomalyTracker, config.BatchConfig, DebugSetup),
      RouterThread(*Config, Conf, AnomalyTracker, MsgStateTracker,
          config.BatchConfig, DebugSetup, Dispatcher),
      MetadataTimestamp(RouterThread.GetMetadataTimestamp()) {
  if (!Config->ReceiveStreamSocketName.empty() ||
      Config->InputPort.IsKnown()) {
    /* Create thread pool if UNIX stream or TCP input is enabled. */
    StreamClientWorkerPool.MakeKnown(WorkerPoolFatalErrorHandler);
  }

  if (!Config->ReceiveSocketName.empty()) {
    UnixDgInputAgent.MakeKnown(*Config, Pool, MsgStateTracker, AnomalyTracker,
        RouterThread.GetMsgChannel());
  }

  if (!Config->ReceiveStreamSocketName.empty()) {
    assert(StreamClientWorkerPool.IsKnown());
    UnixStreamInputAgent.MakeKnown(STREAM_BACKLOG,
        Config->ReceiveStreamSocketName.c_str(),
        CreateStreamClientHandler(false), UnixStreamServerFatalErrorHandler);

    if (Config->ReceiveStreamSocketMode.IsKnown()) {
      UnixStreamInputAgent->SetMode(*Config->ReceiveStreamSocketMode);
    }
  }

  if (Config->InputPort.IsKnown()) {
    TcpInputAgent.MakeKnown(STREAM_BACKLOG, htonl(INADDR_LOOPBACK),
        *Config->InputPort, CreateStreamClientHandler(true),
        TcpServerFatalErrorHandler);
  }

  config.BatchConfig.Clear();
  std::lock_guard<std::mutex> lock(ServerListMutex);
  ServerList.push_front(this);
  MyServerListItem = ServerList.begin();
}

TDoryServer::~TDoryServer() {
  assert(this);
  std::lock_guard<std::mutex> lock(ServerListMutex);
  assert(*MyServerListItem == this);
  ServerList.erase(MyServerListItem);
}

void TDoryServer::BindStatusSocket(bool bind_ephemeral) {
  assert(this);
  TAddress status_address(
      Config->StatusLoopbackOnly ? TAddress::IPv4Loopback : TAddress::IPv4Any,
      bind_ephemeral ? 0 : Config->StatusPort);
  TmpStatusSocket = IfLt0(socket(status_address.GetFamily(), SOCK_STREAM, 0));
  int flag = true;
  IfLt0(setsockopt(TmpStatusSocket, SOL_SOCKET, SO_REUSEADDR, &flag,
                   sizeof(flag)));

  /* This will throw if the server is already running (unless we used an
     ephemeral port, which happens when test code runs us). */
  Bind(TmpStatusSocket, status_address);

  TAddress sock_name = GetSockName(TmpStatusSocket);
  StatusPort = sock_name.GetPort();
  assert(bind_ephemeral || (StatusPort == Config->StatusPort));
}

int TDoryServer::Run() {
  assert(this);

  /* Regardless of what happens, we must notify test code when we have either
     finished initialization or are shutting down (possibly due to a fatal
     exception). */
  TInitNotifier init_notifier(InitWaitSem);

  if (Started) {
    throw std::logic_error("Multiple calls to Run() method not supported");
  }

  Started = true;

  /* The main thread handles all signals, and keeps them all blocked except in
     specific places where it is ready to handle them.  This simplifies signal
     handling.  It is important that we have all signals blocked when creating
     threads, since they inherit our signal mask, and we want them to block all
     signals. */
  TMasker block_all(*TSet(TSet::Full));

  LOG(TPri::NOTICE) << "Server started";

  /* The destructor shuts down Dory's web interface if we start it below.  We
     want this to happen _after_ the message handling threads have shut down.
   */
  TWebInterface web_interface(StatusPort, MsgStateTracker, AnomalyTracker,
      MetadataTimestamp, RouterThread.GetMetadataUpdateRequestSem(),
      DebugSetup);

  bool no_error = StartMsgHandlingThreads();

  /* This starts the input agents and router thread but doesn't wait for the
     router thread to finish initialization. */
  if (no_error) {
    /* Initialization of all input agents succeeded.  Start the Mongoose HTTP
       server, which provides Dory's web interface.  It runs in separate
       threads. */
    web_interface.StartHttpServer(Config->StatusLoopbackOnly);

    /* We can close this now, since Mongoose has the port claimed. */
    TmpStatusSocket.Reset();

    LOG(TPri::NOTICE)
        << "Started web interface, waiting for signals and errors";

    init_notifier.Notify();

    /* Wait for signals and fatal errors.  Return when it is time for the
       server to shut down. */
    if (!HandleEvents()) {
      no_error = false;
    }
  }

  return Shutdown() && no_error ? EXIT_SUCCESS : EXIT_FAILURE;
}

void TDoryServer::RequestShutdown() noexcept {
  assert(this);

  try {
    if (!ShutdownRequested.test_and_set()) {
      GotShutdownSignal.Increment();
      ShutdownRequestSem.Push();
    }
  } catch (const std::exception &x) {
    LOG(TPri::ERR) << "Fatal exception in TDoryServer::RequestShutdown(): "
        << x.what();
    _exit(EXIT_FAILURE);
  } catch (...) {
    LOG(TPri::ERR)
        << "Fatal unknown exception in TDoryServer::RequestShutdown()";
    _exit(EXIT_FAILURE);
  }
}

std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>
    TDoryServer::CreateStreamClientHandler(bool is_tcp) {
  assert(this);
  return std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>(
      new TStreamClientHandler(is_tcp, *Config, Pool, MsgStateTracker,
          AnomalyTracker, RouterThread.GetMsgChannel(),
          *StreamClientWorkerPool));
}

bool TDoryServer::StartMsgHandlingThreads() {
  assert(this);

  if (!Config->DiscardLogPath.empty()) {
    /* We must do this before starting the input agents so all discards are
       tracked properly when discard file logging is enabled.  This starts a
       thread when discard file logging is enabled. */
    DiscardFileLogger.Init(Config->DiscardLogPath.c_str(),
        static_cast<uint64_t>(Config->DiscardLogMaxFileSize) * 1024,
        static_cast<uint64_t>(Config->DiscardLogMaxArchiveSize) * 1024,
        Config->DiscardLogBadMsgPrefixSize);
  }

  if (StreamClientWorkerPool.IsKnown()) {
    StreamClientWorkerPool->Start();
  }

  if (UnixDgInputAgent.IsKnown()) {
    LOG(TPri::NOTICE) << "Starting UNIX datagram input agent";

    if (!UnixDgInputAgent->SyncStart()) {
      LOG(TPri::NOTICE)
          << "Server shutting down due to error starting UNIX datagram input "
          << "agent";
      return false;
    }
  }

  if (UnixStreamInputAgent.IsKnown()) {
    assert(StreamClientWorkerPool.IsKnown());
    LOG(TPri::NOTICE) << "Starting UNIX stream input agent";

    if (!UnixStreamInputAgent->SyncStart()) {
      LOG(TPri::NOTICE)
          << "Server shutting down due to error starting UNIX stream input "
          << "agent";
      return false;
    }
  }

  if (TcpInputAgent.IsKnown()) {
    assert(StreamClientWorkerPool.IsKnown());
    LOG(TPri::NOTICE) << "Starting TCP input agent";

    if (!TcpInputAgent->SyncStart()) {
      LOG(TPri::NOTICE)
          << "Server shutting down due to error starting TCP input agent";
      return false;
    }
  }

  /* Wait for the input agents to finish initialization, but don't wait for the
     router thread since Kafka problems can delay its initialization
     indefinitely.  Even while the router thread is still starting, the input
     agents can receive messages from clients and queue them for routing.  The
     input agents must be fully functional as soon as possible, and always be
     responsive so clients never block while sending messages.  If Kafka
     problems delay router thread initialization indefinitely, messages will be
     queued until we run out of buffer space and start logging discards. */
  LOG(TPri::NOTICE) << "Starting router thread";
  RouterThread.Start();
  return true;
}

static void ReportStreamClientWorkerErrors(
    const std::list<TManagedThreadPoolBase::TWorkerError> &error_list) {
  for (const auto &error : error_list) {
    try {
      std::rethrow_exception(error.ThrownException);
    } catch (const std::exception &x) {
      StreamClientWorkerStdException.Increment();

      /* TODO: Consider adding individual rate limits for different error
         types. */
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Stream input connection handler terminated on error: "
          << x.what();
    } catch (...) {
      StreamClientWorkerUnknownException.Increment();
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Stream input connection handler terminated on unknown error";
    }
  }
}

bool TDoryServer::HandleEvents() {
  assert(this);

  /* This is for periodically verifying that we are getting queried for discard
     info. */
  TTimerFd discard_query_check_timer(
      1000 * (1 + Config->DiscardReportInterval));

  std::array<struct pollfd, 8> events;
  struct pollfd &discard_query_check = events[0];
  struct pollfd &unix_dg_input_agent_error = events[1];
  struct pollfd &unix_stream_input_agent_error = events[2];
  struct pollfd &tcp_input_agent_error = events[3];
  struct pollfd &router_thread_error = events[4];
  struct pollfd &shutdown_request = events[5];
  struct pollfd &worker_pool_worker_error = events[6];
  struct pollfd &worker_pool_fatal_error = events[7];
  discard_query_check.fd = discard_query_check_timer.GetFd();
  discard_query_check.events = POLLIN;
  unix_dg_input_agent_error.fd = UnixDgInputAgent.IsKnown() ?
      int(UnixDgInputAgent->GetShutdownWaitFd()) : -1;
  unix_dg_input_agent_error.events = POLLIN;
  unix_stream_input_agent_error.fd = UnixStreamInputAgent.IsKnown() ?
      int(UnixStreamInputAgent->GetShutdownWaitFd()) : -1;
  unix_stream_input_agent_error.events = POLLIN;
  tcp_input_agent_error.fd = TcpInputAgent.IsKnown() ?
      int(TcpInputAgent->GetShutdownWaitFd()) : -1;
  tcp_input_agent_error.events = POLLIN;
  router_thread_error.fd = RouterThread.GetShutdownWaitFd();
  router_thread_error.events = POLLIN;
  shutdown_request.fd = ShutdownRequestSem.GetFd();
  shutdown_request.events = POLLIN;

  if (StreamClientWorkerPool.IsKnown()) {
    worker_pool_worker_error.fd = StreamClientWorkerPool->GetErrorPendingFd();
    worker_pool_fatal_error.fd = StreamClientWorkerPool->GetShutdownWaitFd();
  } else {
    worker_pool_worker_error.fd = -1;
    worker_pool_fatal_error.fd = -1;
  }

  worker_pool_worker_error.events = POLLIN;
  worker_pool_fatal_error.events = POLLIN;
  bool fatal_error = false;

  for (; ; ) {
    for (auto &item : events) {
      item.revents = 0;
    }

    int ret = ppoll(&events[0], events.size(), nullptr,
        TSet(TSet::Exclude, { SIGINT, SIGTERM }).Get());
    assert(ret);

    if ((ret < 0) && (errno != EINTR)) {
      IfLt0(ret);  // this will throw
    }

    if (unix_dg_input_agent_error.revents) {
      assert(UnixDgInputAgent.IsKnown());
      LOG(TPri::ERR)
          << "Main thread detected UNIX datagram input agent termination on "
          << "fatal error";
      fatal_error = true;
    }

    if (unix_stream_input_agent_error.revents) {
      assert(UnixStreamInputAgent.IsKnown());
      LOG(TPri::ERR)
          << "Main thread detected UNIX stream input agent termination on "
          << "fatal error";
      fatal_error = true;
    }

    if (tcp_input_agent_error.revents) {
      assert(TcpInputAgent.IsKnown());
      LOG(TPri::ERR)
          << "Main thread detected TCP input agent termination on fatal error";
      fatal_error = true;
    }

    if (router_thread_error.revents) {
      LOG(TPri::ERR)
          << "Main thread detected router thread termination on fatal error";
      fatal_error = true;
    }

    if (worker_pool_fatal_error.revents) {
      assert(StreamClientWorkerPool.IsKnown());
      LOG(TPri::ERR) << "Main thread detected stream worker pool fatal error";
      fatal_error = true;
    }

    if (worker_pool_worker_error.revents) {
      assert(StreamClientWorkerPool.IsKnown());
      ReportStreamClientWorkerErrors(
          StreamClientWorkerPool->GetAllPendingErrors());
    }

    if (fatal_error) {
      break;
    }

    if (discard_query_check.revents) {
      discard_query_check_timer.Pop();
      AnomalyTracker.CheckGetInfoRate();
    }

    if (shutdown_request.revents) {
      LOG(TPri::NOTICE) << "Got shutdown signal while server running";
      break;
    }
  }

  return !fatal_error;
}

static void ShutDownInputAgent(Thread::TFdManagedThread &agent,
    const char *agent_name, bool &shutdown_ok) {
  if (agent.IsStarted()) {
    LOG(TPri::NOTICE) << "Shutting down " << agent_name << " input agent";

    /* Note: Calling RequestShutdown() is harmless if the agent has already
       shut down due to a fatal error. */
    agent.RequestShutdown();

    try {
      agent.Join();
      LOG(TPri::NOTICE) << agent_name << " input agent terminated normally";
    } catch (const TFdManagedThread::TWorkerError &x) {
      shutdown_ok = false;

      try {
        std::rethrow_exception(x.ThrownException);
      } catch (const std::exception &y) {
        LOG(TPri::ERR) << agent_name << " input agent terminated on error: "
            << y.what();
      } catch (...) {
        LOG(TPri::ERR) << agent_name
            << " input agent terminated on unknown error";
      }
    }
  }
}

void TDoryServer::DiscardFinalMsgs(std::list<TMsg::TPtr> &msg_list) {
  assert(this);

  for (TMsg::TPtr &msg : msg_list) {
    if (msg) {
      if (!Config->NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Main thread discarding queued message on server shutdown: "
            << "topic [" << msg->GetTopic() << "]";
      }

      AnomalyTracker.TrackDiscard(msg,
          TAnomalyTracker::TDiscardReason::ServerShutdown);
      MsgStateTracker.MsgEnterProcessed(*msg);
    } else {
      assert(false);
      LOG(TPri::ERR) << "Main thread got empty TMsg::TPtr during shutdown";
      BacktraceToLog();
    }
  }
}

bool TDoryServer::Shutdown() {
  assert(this);
  bool shutdown_ok = true;

  /* We could parallelize the shutdown by first calling each agent's
     RequestShutdown() method and then calling each agent's Join() method.
     However, the agents should be very quick to respond so it's not really
     worth the effort. */

  if (TcpInputAgent.IsKnown()) {
    ShutDownInputAgent(*TcpInputAgent, "TCP", shutdown_ok);
  }

  if (UnixStreamInputAgent.IsKnown()) {
    ShutDownInputAgent(*UnixStreamInputAgent, "UNIX stream", shutdown_ok);
  }

  if (UnixDgInputAgent.IsKnown()) {
    ShutDownInputAgent(*UnixDgInputAgent, "UNIX datagram", shutdown_ok);
  }

  if (StreamClientWorkerPool.IsKnown()) {
    StreamClientWorkerPool->RequestShutdown();
    StreamClientWorkerPool->WaitForShutdown();
    ReportStreamClientWorkerErrors(
        StreamClientWorkerPool->GetAllPendingErrors());
  }

  /* TODO: Make this more uniform relative to shutdown of input agents. */
  bool router_thread_started = RouterThread.IsStarted();

  if (router_thread_started) {
    LOG(TPri::NOTICE) << "Shutting down router thread";
    RouterThread.RequestShutdown();
    RouterThread.Join();
    bool router_thread_ok = RouterThread.ShutdownWasOk();
    LOG(TPri::NOTICE) << "Router thread terminated "
        << (router_thread_ok ? "normally" : "on error");

    if (!router_thread_ok) {
      shutdown_ok = false;
    }
  }

  /* In the case where a failure starting an input agent prevented us from
     starting the router thread, one of the nonfailing agents may have queued
     some messages for routing.  Here we discard any such messages. */
  std::list<TMsg::TPtr> msg_list = RouterThread.GetRemainingMsgs();
  assert(!router_thread_started || msg_list.empty());
  DiscardFinalMsgs(msg_list);

  LOG(TPri::NOTICE) << "Dory shutdown finished";

  /* Let the DiscardFileLogger destructor disable discard file logging.  Then
     we know it gets disabled only after everything that may generate discards
     has been destroyed. */
  return shutdown_ok;
}

const int TDoryServer::STREAM_BACKLOG;

std::mutex TDoryServer::ServerListMutex;

std::list<TDoryServer *> TDoryServer::ServerList;
