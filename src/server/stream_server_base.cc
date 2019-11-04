/* <server/stream_server_base.cc>

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

   Implements <server/stream_server_base.h>.
 */

#include <server/stream_server_base.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <utility>

#include <poll.h>

#include <base/error_util.h>
#include <base/on_destroy.h>
#include <base/wr/net_util.h>

using namespace Base;
using namespace Server;

void TStreamServerBase::TConnectionHandlerApi::HandleNonfatalAcceptError(
    int /*errno_value*/) {
  assert(this);
  /* Base class version is no-op.  Subclasses can override. */
}

TStreamServerBase::~TStreamServerBase() {
  assert(this);
  ShutdownOnDestroy();

  if (IsBound()) {
    /* Clean up in the case where Bind() was called but the server was not
       started. */
    try {
      CloseListeningSocket(ListeningSocket);
    } catch (...) {
    }
  }
}

void TStreamServerBase::Bind() {
  assert(this);

  if (IsBound()) {
    Die("TStreamServerBase::Bind() has already been called");
  }

  TFd listening_socket;
  InitListeningSocket(listening_socket);
  ListeningSocket = std::move(listening_socket);

  if (!IsBound()) {
    Die("InitListeningSocket() must throw on failure in TStreamServerBase");
  }
}

bool TStreamServerBase::SyncStart() {
  assert(this);

  if (IsStarted()) {
    Die("Cannot call SyncStart() when server is already started");
  }

  SyncStartSuccess = false;
  TEventSemaphore started;
  SyncStartNotify = &started;
  Start();
  started.Pop();
  SyncStartNotify = nullptr;
  return SyncStartSuccess;
}

void TStreamServerBase::Reset() {
  assert(this);

  if (IsStarted()) {
    RequestShutdown();
    Join();
  } else {
    CloseListeningSocket(ListeningSocket);
  }
}

TStreamServerBase::TStreamServerBase(int backlog, struct sockaddr *addr,
    socklen_t addr_space,
    std::unique_ptr<TConnectionHandlerApi> &&connection_handler)
    : Backlog(backlog),
      Addr(addr),
      AddrSpace(addr_space),
      SyncStartSuccess(false),
      SyncStartNotify(nullptr),
      ConnectionHandler(std::move(connection_handler)) {
  assert(ConnectionHandler);
  std::memset(Addr, 0, AddrSpace);  // not strictly necessary
}

void TStreamServerBase::CloseListeningSocket(TFd &sock) {
  assert(this);
  sock.Reset();
}

void TStreamServerBase::Run() {
  assert(this);
  auto close_socket = OnDestroy(
      [this]() noexcept {
        CloseListeningSocket(ListeningSocket);
      });

  try {
    if (!IsBound()) {
      Bind();
    }

    IfLt0(Wr::listen(ListeningSocket, Backlog));
  } catch (...) {
    if (SyncStartNotify) {
      SyncStartNotify->Push();
    }

    throw;
  }

  if (SyncStartNotify) {
    SyncStartSuccess = true;
    SyncStartNotify->Push();
  }

  AcceptClients();
}

void TStreamServerBase::AcceptClients() {
  assert(this);
  std::array<struct pollfd, 2> events;
  struct pollfd &shutdown_request_event = events[0];
  struct pollfd &new_client_event = events[1];
  shutdown_request_event.fd = GetShutdownRequestFd();
  shutdown_request_event.events = POLLIN;
  new_client_event.fd = ListeningSocket;
  new_client_event.events = POLLIN;

  for (; ; ) {
    for (auto &item : events) {
      item.revents = 0;
    }

    int ret = Wr::poll(&events[0], events.size(), -1);

    if (ret < 0) {
      assert(errno == EINTR);
      continue;
    }

    assert(ret > 0);

    if (shutdown_request_event.revents) {
      break;
    }

    assert(new_client_event.revents);
    socklen_t len = AddrSpace;
    ret = Wr::accept(ListeningSocket, Addr,
        (Addr == nullptr) ? nullptr : &len);

    if (ret < 0) {
      ConnectionHandler->HandleNonfatalAcceptError(errno);
      continue;
    }

    TFd client_fd(ret);
    ConnectionHandler->HandleConnection(std::move(client_fd), Addr,
        (Addr == nullptr) ? 0 : len);

    if (client_fd.IsOpen()) {
      Die("Client FD is nonempty after HandleClientConnection()");
    }
  }
}
