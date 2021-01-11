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
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <memory>
#include <set>

#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#include <base/counter.h>
#include <base/error_util.h>
#include <base/file_reader.h>
#include <base/no_default_case.h>
#include <base/sig_masker.h>
#include <base/wr/fd_util.h>
#include <base/wr/net_util.h>
#include <base/wr/time_util.h>
#include <capped/pool.h>
#include <dory/compress/compression_init.h>
#include <dory/conf/batch_conf.h>
#include <dory/conf/compression_conf.h>
#include <dory/discard_file_logger.h>
#include <dory/kafka_proto/metadata/version_util.h>
#include <dory/kafka_proto/produce/version_util.h>
#include <dory/msg.h>
#include <dory/util/init_notifier.h>
#include <dory/util/invalid_arg_error.h>
#include <dory/util/misc_util.h>
#include <dory/web_interface.h>
#include <log/log.h>
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
using namespace Socket;
using namespace Thread;

DEFINE_COUNTER(StreamClientWorkerStdException);
DEFINE_COUNTER(StreamClientWorkerUnknownException);

bool TDoryServer::CheckUnixDgSize(const TConf &conf) {
  bool large_sendbuf_required = false;

  if (!conf.InputSourcesConf.UnixDgPath.empty()) {
    switch (TestUnixDgSize(conf.InputConfigConf.MaxDatagramMsgSize)) {
      case TUnixDgSizeTestResult::Pass:
        break;
      case TUnixDgSizeTestResult::PassWithLargeSendbuf:
        large_sendbuf_required = true;

        if (!conf.InputConfigConf.AllowLargeUnixDatagrams) {
          throw TInvalidArgError(
              "You didn't specify allow_large_unix_datagrams, and "
              "max_input_msg_size is large enough that clients sending large "
              "datagrams will need to increase SO_SNDBUF above the default "
              "value.  Either decrease max_input_msg_size or specify "
              "allow_large_unix_datagrams.");
        }

        break;
      case TUnixDgSizeTestResult::Fail:
        throw TInvalidArgError("max_input_msg_size is too large");
      NO_DEFAULT_CASE;
    }
  }

  return large_sendbuf_required;
}

static void LoadCompressionLibraries(const TCompressionConf &conf) {
  std::set<TCompressionType> in_use;
  in_use.insert(conf.DefaultTopicConfig.Type);
  const TCompressionConf::TTopicMap &topic_map = conf.TopicConfigs;

  for (const auto &item : topic_map) {
    in_use.insert(item.second.Type);
  }

  /* For each needed compression type, force the associated compression library
     to load.  This will throw if there is an error loading a library. */
  for (TCompressionType type : in_use) {
    CompressionInit(type);
  }
}

void TDoryServer::PrepareForInit(const Conf::TConf &conf) {
  /* Put code here that should be called only once, even in the case where
     multiple TDoryServer objects are created. */

  /* Load any compression libraries we need, according to the compression info
     from our config file.  This will throw if a library fails to load. */
  LoadCompressionLibraries(conf.CompressionConf);

  /* The TDoryServer constructor will use the random number generator, so
     initialize it now. */
  struct timespec t;
  Wr::clock_gettime(CLOCK_MONOTONIC_RAW, &t);
  std::srand(static_cast<unsigned>(t.tv_sec ^ t.tv_nsec));
}

static inline size_t
ComputeBlockCount(size_t max_buffer, size_t block_size) {
  return std::max<size_t>(1, max_buffer / block_size);
}

TDoryServer::TDoryServer(TCmdLineArgs &&args, TConf &&conf,
    const TFd &shutdown_fd)
    : CmdLineArgs(std::move(args)),
      Conf(std::move(conf)),
      PoolBlockSize(128),
      ShutdownFd(shutdown_fd),
      Pool(PoolBlockSize,
           ComputeBlockCount(Conf.InputConfigConf.MaxBuffer, PoolBlockSize),
           Capped::TPool::TSync::Mutexed),
      AnomalyTracker(DiscardFileLogger,
          Conf.HttpInterfaceConf.DiscardReportInterval,
          Conf.HttpInterfaceConf.BadMsgPrefixSize),
      StatusPort(0),
      DebugSetup(Conf.MsgDebugConf.Path.c_str(), Conf.MsgDebugConf.TimeLimit,
                 Conf.MsgDebugConf.ByteLimit),
      Dispatcher(CmdLineArgs, Conf, MsgStateTracker, AnomalyTracker,
          DebugSetup),
      RouterThread(CmdLineArgs, Conf, AnomalyTracker, MsgStateTracker,
          DebugSetup, Dispatcher),
      MetadataTimestamp(RouterThread.GetMetadataTimestamp()) {
  if (!Conf.InputSourcesConf.UnixStreamPath.empty() ||
      Conf.InputSourcesConf.LocalTcpPort) {
    /* Create thread pool if UNIX stream or TCP input is enabled. */
    StreamClientWorkerPool.emplace();
  }

  if (!Conf.InputSourcesConf.UnixDgPath.empty()) {
    UnixDgInputAgent.emplace(Conf, Pool, MsgStateTracker, AnomalyTracker,
        RouterThread.GetMsgChannel());
  }

  if (!Conf.InputSourcesConf.UnixStreamPath.empty()) {
    assert(StreamClientWorkerPool.has_value());
    UnixStreamInputAgent.emplace(STREAM_BACKLOG,
        Conf.InputSourcesConf.UnixStreamPath.c_str(),
        CreateStreamClientHandler(false));

    if (Conf.InputSourcesConf.UnixStreamMode) {
      UnixStreamInputAgent->SetMode(*Conf.InputSourcesConf.UnixStreamMode);
    }
  }

  if (Conf.InputSourcesConf.LocalTcpPort) {
    TcpInputAgent.emplace(STREAM_BACKLOG, htonl(INADDR_LOOPBACK),
        *Conf.InputSourcesConf.LocalTcpPort, CreateStreamClientHandler(true));
  }
}

void TDoryServer::BindStatusSocket(bool bind_ephemeral) {
  TAddress status_address(
      Conf.HttpInterfaceConf.LoopbackOnly ?
          TAddress::IPv4Loopback : TAddress::IPv4Any,
      bind_ephemeral ? 0 : Conf.HttpInterfaceConf.Port);
  TmpStatusSocket = Wr::socket(Wr::TDisp::Nonfatal, {},
      status_address.GetFamily(), SOCK_STREAM, 0);
  assert(TmpStatusSocket.IsOpen());
  int flag = true;
  Wr::setsockopt(TmpStatusSocket, SOL_SOCKET, SO_REUSEADDR, &flag,
      sizeof(flag));

  /* This will throw if the server is already running (unless we used an
     ephemeral port, which happens when test code runs us). */
  Bind(TmpStatusSocket, status_address);

  TAddress sock_name = GetSockName(TmpStatusSocket);
  StatusPort = sock_name.GetPort();
  assert(bind_ephemeral || (StatusPort == Conf.HttpInterfaceConf.Port));
}

int TDoryServer::Run() {
  /* Regardless of what happens, we must notify test code when we have either
     finished initialization or are shutting down (possibly due to a fatal
     exception). */
  TInitNotifier init_notifier(InitWaitSem);

  if (Started) {
    Die("Multiple calls to Run() method not supported");
  }

  Started = true;
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
    web_interface.StartHttpServer(Conf.HttpInterfaceConf.LoopbackOnly);

    /* We can close this now, since Mongoose has the port claimed. */
    TmpStatusSocket.Reset();

    LOG(TPri::NOTICE)
        << "Started web interface, waiting for shutdown request or errors";

    init_notifier.Notify();

    /* Wait for shutdown request or fatal error.  Return when it is time for
       the server to shut down. */
    if (!HandleEvents()) {
      no_error = false;
    }
  }

  return Shutdown() && no_error ? EXIT_SUCCESS : EXIT_FAILURE;
}

std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>
    TDoryServer::CreateStreamClientHandler(bool is_tcp) {
  return std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>(
      new TStreamClientHandler(is_tcp, Conf, Pool, MsgStateTracker,
          AnomalyTracker, RouterThread.GetMsgChannel(),
          *StreamClientWorkerPool));
}

bool TDoryServer::StartMsgHandlingThreads() {
  if (!Conf.DiscardLoggingConf.Path.empty()) {
    /* We must do this before starting the input agents so all discards are
       tracked properly when discard file logging is enabled.  This starts a
       thread when discard file logging is enabled. */
    DiscardFileLogger.Init(Conf.DiscardLoggingConf.Path.c_str(),
        static_cast<uint64_t>(Conf.DiscardLoggingConf.MaxFileSize),
        static_cast<uint64_t>(Conf.DiscardLoggingConf.MaxArchiveSize),
        Conf.DiscardLoggingConf.MaxMsgPrefixSize);
  }

  if (StreamClientWorkerPool) {
    StreamClientWorkerPool->Start();
  }

  if (UnixDgInputAgent) {
    LOG(TPri::NOTICE) << "Starting UNIX datagram input agent";

    if (!UnixDgInputAgent->SyncStart()) {
      LOG(TPri::NOTICE)
          << "Server shutting down due to error starting UNIX datagram input "
          << "agent";
      return false;
    }
  }

  if (UnixStreamInputAgent) {
    assert(StreamClientWorkerPool.has_value());
    LOG(TPri::NOTICE) << "Starting UNIX stream input agent";

    if (!UnixStreamInputAgent->SyncStart()) {
      LOG(TPri::NOTICE)
          << "Server shutting down due to error starting UNIX stream input "
          << "agent";
      return false;
    }
  }

  if (TcpInputAgent) {
    assert(StreamClientWorkerPool.has_value());
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
  /* This is for periodically verifying that we are getting queried for discard
     info. */
  TTimerFd discard_query_check_timer(
      1000 * (1 + Conf.HttpInterfaceConf.DiscardReportInterval));

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
  unix_dg_input_agent_error.fd = UnixDgInputAgent ?
      int(UnixDgInputAgent->GetShutdownWaitFd()) : -1;
  unix_dg_input_agent_error.events = POLLIN;
  unix_stream_input_agent_error.fd = UnixStreamInputAgent ?
      int(UnixStreamInputAgent->GetShutdownWaitFd()) : -1;
  unix_stream_input_agent_error.events = POLLIN;
  tcp_input_agent_error.fd = TcpInputAgent ?
      int(TcpInputAgent->GetShutdownWaitFd()) : -1;
  tcp_input_agent_error.events = POLLIN;
  router_thread_error.fd = RouterThread.GetShutdownWaitFd();
  router_thread_error.events = POLLIN;
  shutdown_request.fd = ShutdownFd;
  shutdown_request.events = POLLIN;

  if (StreamClientWorkerPool) {
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

    /* Treat EINTR as fatal because all signals should be blocked. */
    const int ret = Wr::poll(Wr::TDisp::AddFatal, {EINTR}, &events[0],
        events.size(), -1 /* infinite timeout */);
    assert(ret > 0);

    if (unix_dg_input_agent_error.revents) {
      assert(UnixDgInputAgent.has_value());
      LOG(TPri::ERR)
          << "Main thread detected UNIX datagram input agent termination on "
          << "fatal error";
      fatal_error = true;
    }

    if (unix_stream_input_agent_error.revents) {
      assert(UnixStreamInputAgent.has_value());
      LOG(TPri::ERR)
          << "Main thread detected UNIX stream input agent termination on "
          << "fatal error";
      fatal_error = true;
    }

    if (tcp_input_agent_error.revents) {
      assert(TcpInputAgent.has_value());
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
      assert(StreamClientWorkerPool.has_value());
      LOG(TPri::ERR) << "Main thread detected stream worker pool fatal error";
      fatal_error = true;
    }

    if (worker_pool_worker_error.revents) {
      assert(StreamClientWorkerPool.has_value());
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
  for (TMsg::TPtr &msg : msg_list) {
    if (msg) {
      if (Conf.LoggingConf.LogDiscards) {
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
      LogStackTrace(TPri::ERR);
    }
  }
}

bool TDoryServer::Shutdown() {
  bool shutdown_ok = true;

  /* We could parallelize the shutdown by first calling each agent's
     RequestShutdown() method and then calling each agent's Join() method.
     However, the agents should be very quick to respond so it's not really
     worth the effort. */

  if (TcpInputAgent) {
    ShutDownInputAgent(*TcpInputAgent, "TCP", shutdown_ok);
  }

  if (UnixStreamInputAgent) {
    ShutDownInputAgent(*UnixStreamInputAgent, "UNIX stream", shutdown_ok);
  }

  if (UnixDgInputAgent) {
    ShutDownInputAgent(*UnixDgInputAgent, "UNIX datagram", shutdown_ok);
  }

  if (StreamClientWorkerPool) {
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
