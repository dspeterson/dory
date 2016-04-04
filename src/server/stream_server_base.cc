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
#include <stdexcept>
#include <utility>

#include <poll.h>

#include <base/error_utils.h>

using namespace Base;
using namespace Server;

void TStreamServerBase::TConnectionHandlerApi::HandleNonfatalAcceptError(
    int /*errno_value*/) {
  assert(this);
  /* Base class version is no-op.  Subclasses can override. */
}

TStreamServerBase::~TStreamServerBase() noexcept {
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
    throw std::logic_error(
        "TStreamServerBase::Bind() has already been called");
  }

  TFd listening_socket;
  InitListeningSocket(listening_socket);
  ListeningSocket = std::move(listening_socket);

  if (!IsBound()) {
    throw std::logic_error(
        "InitListeningSocket() must throw on failure in TStreamServerBase");
  }
}

void TStreamServerBase::SyncStart() {
  assert(this);

  if (IsStarted()) {
    throw std::logic_error(
        "Cannot call SyncStart() when server is already started");
  }

  TEventSemaphore started;
  SyncStartNotify = &started;
  Start();
  started.Pop();
  SyncStartNotify = nullptr;
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
    socklen_t addr_space, TConnectionHandlerApi *connection_handler)
    : Backlog(backlog),
      Addr(addr),
      AddrSpace(addr_space),
      SyncStartNotify(nullptr),
      ConnectionHandler(connection_handler) {
  assert(ConnectionHandler);

  /* not strictly necessary */
  std::memset(Addr, 0, AddrSpace);
}

void TStreamServerBase::CloseListeningSocket(TFd &sock) {
  assert(this);
  sock.Reset();
}

void TStreamServerBase::Run() {
  assert(this);

  class t_closer final {
    public:
    explicit t_closer(TStreamServerBase &server) noexcept
        : Server(server) {
    }

    ~t_closer() noexcept {
      try {
        Server.CloseListeningSocket(Server.ListeningSocket);
      } catch (...) {
      }
    }

    private:
    TStreamServerBase &Server;
  } closer(*this);

  try {
    if (!IsBound()) {
      Bind();
    }

    IfLt0(listen(ListeningSocket, Backlog));
  } catch (...) {
    if (SyncStartNotify) {
      try {
        SyncStartNotify->Push();
      } catch (...) {
      }
    }

    throw;
  }

  if (SyncStartNotify) {
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

    int ret = IfLt0(poll(&events[0], events.size(), -1));
    assert(ret > 0);

    if (shutdown_request_event.revents) {
      break;
    }

    assert(new_client_event.revents);
    socklen_t len = AddrSpace;
    ret = accept(ListeningSocket, Addr,
        (Addr == nullptr) ? nullptr : &len);

    if (ret < 0) {
      /* Note: On some operating systems, EPROTO may be reported instead of
         ECONNABORTED.  EPERM may be reported if firewall rules forbid a
         connection.  See the man page for accept() regarding the errors below.
       */
      if ((errno == ECONNABORTED) || (errno == EPROTO) || (errno == EPERM) ||
          (errno == ENETDOWN) || (errno == ENOPROTOOPT) ||
          (errno == EHOSTDOWN) || (errno == ENONET) ||
          (errno == EHOSTUNREACH) || (errno == EOPNOTSUPP) ||
          (errno == ENETUNREACH) || (errno == EINTR)) {
        ConnectionHandler->HandleNonfatalAcceptError(errno);
        continue;
      }

      /* Anything else is fatal.  This will throw. */
      IfLt0(ret);
    }

    TFd client_fd(ret);
    ConnectionHandler->HandleConnection(std::move(client_fd), Addr,
        (Addr == nullptr) ? 0 : len);

    if (client_fd.IsOpen()) {
      throw std::logic_error(
          "Client FD is nonempty after HandleClientConnection()");
    }
  }
}
