/* <dory/unix_dg_input_agent.cc>

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

   Implements <dory/unix_dg_input_agent.h>.
 */

#include <dory/unix_dg_input_agent.h>

#include <array>
#include <cassert>
#include <system_error>

#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/counter.h>
#include <base/error_util.h>
#include <base/gettid.h>
#include <base/wr/file_util.h>
#include <base/wr/net_util.h>
#include <dory/input_dg/input_dg_util.h>
#include <log/log.h>
#include <socket/address.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Conf;
using namespace Log;
using namespace Socket;
using namespace Thread;

DEFINE_COUNTER(UnixDgInputAgentForwardMsg);

TUnixDgInputAgent::TUnixDgInputAgent(const TConf &conf, TPool &pool,
    TMsgStateTracker &msg_state_tracker, TAnomalyTracker &anomaly_tracker,
    TGatePutApi<TMsg::TPtr> &output_queue)
    : Conf(conf),
      Pool(pool),
      MsgStateTracker(msg_state_tracker),
      AnomalyTracker(anomaly_tracker),
      InputBuf(conf.InputConfigConf.MaxDatagramMsgSize),
      OutputQueue(output_queue) {
}

TUnixDgInputAgent::~TUnixDgInputAgent() {
  /* This will shut down the thread if something unexpected happens.  Setting
     the 'Destroying' flag tells the thread to shut down immediately when it
     gets the shutdown request. */
  Destroying = true;
  ShutdownOnDestroy();
}

bool TUnixDgInputAgent::SyncStart() {
  if (IsStarted()) {
    Die("Cannot call SyncStart() when UNIX datagram input agent is already "
        "started");
  }

  SyncStartSuccess = false;
  TEventSemaphore started;
  SyncStartNotify = &started;
  Start();
  started.Pop();
  SyncStartNotify = nullptr;
  return SyncStartSuccess;
}

void TUnixDgInputAgent::Run() {
  int tid = static_cast<int>(Gettid());
  LOG(TPri::NOTICE) << "UNIX datagram input thread " << tid << " started";

  try {
    OpenUnixSocket();
  } catch (...) {
    if (SyncStartNotify) {
      try {
        SyncStartNotify->Push();
      } catch (...) {
        LOG(TPri::ERR)
            << "Failed to notify on error starting UNIX datagram input agent";
        Die("Terminating on fatal error");
      }
    }

    throw;
  }

  if (SyncStartNotify) {
    SyncStartSuccess = true;
    SyncStartNotify->Push();
  }

  LOG(TPri::NOTICE)
      << "UNIX datagram input thread finished initialization, forwarding "
      << "messages";
  ForwardMessages();
}

void TUnixDgInputAgent::OpenUnixSocket() {
  LOG(TPri::NOTICE) << "UNIX datagram input thread opening socket";
  TAddress input_socket_address;
  input_socket_address.SetFamily(AF_LOCAL);
  input_socket_address.SetPath(Conf.InputSourcesConf.UnixDgPath.c_str());

  try {
    Bind(InputSocket, input_socket_address);
  } catch (const std::system_error &x) {
    LOG(TPri::ERR) << "Failed to create datagram socket file: " << x.what();
    Die("Terminating on fatal error");
  }

  /* Set the permission bits on the socket file if they were specified.  If
     unspecified, the umask determines the permission bits. */
  if (Conf.InputSourcesConf.UnixDgMode.IsKnown()) {
    try {
      IfLt0(Wr::chmod(Conf.InputSourcesConf.UnixDgPath.c_str(),
          *Conf.InputSourcesConf.UnixDgMode));
    } catch (const std::system_error &x) {
      LOG(TPri::ERR) << "Failed to set permissions on datagram socket file: "
          << x.what();
      Die("Terminating on fatal error");
    }
  }
}

TMsg::TPtr TUnixDgInputAgent::ReadOneMsg() {
  char * const msg_begin = reinterpret_cast<char *>(&InputBuf[0]);
  const ssize_t result = Wr::recv(Wr::TDisp::Nonfatal, {}, InputSocket,
      msg_begin, InputBuf.size(), 0);
  assert(result >= 0);

  return InputDg::BuildMsgFromDg(msg_begin, static_cast<size_t>(result),
      Conf.LoggingConf.LogDiscards, Pool, AnomalyTracker, MsgStateTracker);
}

void TUnixDgInputAgent::ForwardMessages() {
  std::array<struct pollfd, 2> events;
  struct pollfd &shutdown_request_event = events[0];
  struct pollfd &input_socket_event = events[1];
  shutdown_request_event.fd = GetShutdownRequestFd();
  shutdown_request_event.events = POLLIN;
  input_socket_event.fd = InputSocket.GetFd();
  input_socket_event.events = POLLIN;
  TMsg::TPtr msg;

  for (; ; ) {
    for (auto &item : events) {
      item.revents = 0;
    }

    /* We should have signals blocked, so treat EINTR as fatal. */
    int ret = Wr::poll(Wr::TDisp::AddFatal, {EINTR}, &events[0], events.size(),
        -1);
    assert(ret > 0);

    if (shutdown_request_event.revents) {
      if (!Destroying) {
        LOG(TPri::NOTICE)
            << "UNIX datagram input thread got shutdown request, closing "
            << "socket";
        /* We received a shutdown request from the thread that created us.
           Close the input socket and terminate. */
        InputSocket.Reset();
      }

      break;
    }

    assert(input_socket_event.revents);
    assert(!msg);
    msg = ReadOneMsg();

    if (msg) {
      /* Forward message to router thread. */
      OutputQueue.Put(std::move(msg));
      UnixDgInputAgentForwardMsg.Increment();
    }
  }
}
