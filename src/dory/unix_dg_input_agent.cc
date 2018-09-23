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

#include <algorithm>
#include <array>
#include <exception>
#include <system_error>

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <base/error_utils.h>
#include <base/gettid.h>
#include <dory/input_dg/input_dg_util.h>
#include <dory/util/time_util.h>
#include <server/counter.h>
#include <socket/address.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Util;
using namespace Socket;
using namespace Thread;

SERVER_COUNTER(UnixDgInputAgentForwardMsg);

TUnixDgInputAgent::TUnixDgInputAgent(const TConfig &config, TPool &pool,
    TMsgStateTracker &msg_state_tracker, TAnomalyTracker &anomaly_tracker,
    TGatePutApi<TMsg::TPtr> &output_queue)
    : Config(config),
      Destroying(false),
      Pool(pool),
      MsgStateTracker(msg_state_tracker),
      AnomalyTracker(anomaly_tracker),
      InputSocket(SOCK_DGRAM, 0),
      InputBuf(config.MaxInputMsgSize),
      OutputQueue(output_queue),
      SyncStartSuccess(false),
      SyncStartNotify(nullptr) {
}

TUnixDgInputAgent::~TUnixDgInputAgent() {
  /* This will shut down the thread if something unexpected happens.  Setting
     the 'Destroying' flag tells the thread to shut down immediately when it
     gets the shutdown request. */
  Destroying = true;
  ShutdownOnDestroy();
}

bool TUnixDgInputAgent::SyncStart() {
  assert(this);

  if (IsStarted()) {
    throw std::logic_error("Cannot call SyncStart() when UNIX datagram input "
        "agent is already started");
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
  assert(this);
  int tid = static_cast<int>(Gettid());
  syslog(LOG_NOTICE, "UNIX datagram input thread %d started", tid);

  try {
    OpenUnixSocket();
  } catch (...) {
    if (SyncStartNotify) {
      try {
        SyncStartNotify->Push();
      } catch (...) {
        syslog(LOG_ERR,
            "Failed to notify on error starting UNIX datagram input agent");
        _exit(EXIT_FAILURE);
      }
    }

    throw;
  }

  if (SyncStartNotify) {
    SyncStartSuccess = true;
    SyncStartNotify->Push();
  }

  syslog(LOG_NOTICE, "UNIX datagram input thread finished initialization, "
      "forwarding messages");
  ForwardMessages();
}

void TUnixDgInputAgent::OpenUnixSocket() {
  assert(this);
  syslog(LOG_NOTICE, "UNIX datagram input thread opening socket");
  TAddress input_socket_address;
  input_socket_address.SetFamily(AF_LOCAL);
  input_socket_address.SetPath(Config.ReceiveSocketName.c_str());

  try {
    Bind(InputSocket, input_socket_address);
  } catch (const std::system_error &x) {
    syslog(LOG_ERR, "Failed to create datagram socket file: %s", x.what());
    _exit(EXIT_FAILURE);
  }

  /* Set the permission bits on the socket file if they were specified as a
     command line argument.  If unspecified, the umask determines the
     permission bits. */
  if (Config.ReceiveSocketMode.IsKnown()) {
    try {
      IfLt0(chmod(Config.ReceiveSocketName.c_str(),
          *Config.ReceiveSocketMode));
    } catch (const std::system_error &x) {
      syslog(LOG_ERR, "Failed to set permissions on datagram socket file: %s",
          x.what());
      _exit(EXIT_FAILURE);
    }
  }
}

TMsg::TPtr TUnixDgInputAgent::ReadOneMsg() {
  assert(this);
  char * const msg_begin = reinterpret_cast<char *>(&InputBuf[0]);
  ssize_t result = IfLt0(recv(InputSocket, msg_begin, InputBuf.size(), 0));
  return InputDg::BuildMsgFromDg(msg_begin, static_cast<size_t>(result),
      Config, Pool, AnomalyTracker, MsgStateTracker);
}

void TUnixDgInputAgent::ForwardMessages() {
  assert(this);
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

    int ret = IfLt0(poll(&events[0], events.size(), -1));
    assert(ret > 0);

    if (shutdown_request_event.revents) {
      if (!Destroying) {
        syslog(LOG_NOTICE, "UNIX datagram input thread got shutdown request, "
            "closing socket");
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
