/* <server/tcp_ipv6_server.cc>

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

   Implements <server/tcp_ipv6_server.h>.
 */

#include <server/tcp_ipv6_server.h>

#include <cstring>
#include <utility>

#include <base/error_utils.h>
#include <base/fd.h>

using namespace Base;
using namespace Server;

TTcpIpv6Server::TTcpIpv6Server(int backlog, const struct in6_addr &bind_addr,
    in_port_t port, uint32_t scope_id,
    TConnectionHandlerApi *connection_handler,
    const TFatalErrorHandler &fatal_error_handler)
    : TStreamServerBase(backlog,
          reinterpret_cast<struct sockaddr *>(&ClientAddr), sizeof(ClientAddr),
          connection_handler, fatal_error_handler),
      BindAddr(bind_addr),
      Port(port),
      ScopeId(scope_id) {
}

TTcpIpv6Server::TTcpIpv6Server(int backlog, const struct in6_addr &bind_addr,
    in_port_t port, TConnectionHandlerApi *connection_handler,
    const TFatalErrorHandler &fatal_error_handler)
    : TTcpIpv6Server(backlog, bind_addr, port, 0, connection_handler,
          fatal_error_handler) {
}

TTcpIpv6Server::TTcpIpv6Server(int backlog, const struct in6_addr &bind_addr,
    in_port_t port, uint32_t scope_id,
    TConnectionHandlerApi *connection_handler,
    TFatalErrorHandler &&fatal_error_handler)
    : TStreamServerBase(backlog,
          reinterpret_cast<struct sockaddr *>(&ClientAddr), sizeof(ClientAddr),
          connection_handler, std::move(fatal_error_handler)),
      BindAddr(bind_addr),
      Port(port),
      ScopeId(scope_id) {
}

TTcpIpv6Server::TTcpIpv6Server(int backlog, const struct in6_addr &bind_addr,
    in_port_t port, TConnectionHandlerApi *connection_handler,
    TFatalErrorHandler &&fatal_error_handler)
    : TTcpIpv6Server(backlog, bind_addr, port, 0, connection_handler,
          std::move(fatal_error_handler)) {
}

in_port_t TTcpIpv6Server::GetBindPort() const {
  assert(this);

  if (!IsBound()) {
    throw std::logic_error(
        "Cannot get bind port for unbound listening socket");
  }

  struct sockaddr_in6 addr;
  socklen_t addrlen = sizeof(addr);
  IfLt0(getsockname(GetListeningSocket(),
      reinterpret_cast<struct sockaddr *>(&addr), &addrlen));
  return ntohs(addr.sin6_port);
}

void TTcpIpv6Server::InitListeningSocket(TFd &sock) {
  assert(this);
  TFd sock_fd(socket(AF_INET6, SOCK_STREAM, 0));
  struct sockaddr_in6 serv_addr;
  std::memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin6_family = AF_INET6;
  serv_addr.sin6_port = Port;
  serv_addr.sin6_flowinfo = 0;
  serv_addr.sin6_addr = BindAddr;
  serv_addr.sin6_scope_id = ScopeId;
  IfLt0(bind(sock_fd, reinterpret_cast<const struct sockaddr *>(&serv_addr),
      sizeof(serv_addr)));
  sock = std::move(sock_fd);
}
