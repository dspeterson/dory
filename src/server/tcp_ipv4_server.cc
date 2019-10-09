/* <server/tcp_ipv4_server.cc>

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

   Implements <server/tcp_ipv4_server.h>.
 */

#include <server/tcp_ipv4_server.h>

#include <cstring>
#include <utility>

#include <arpa/inet.h>

#include <base/error_util.h>
#include <base/fd.h>
#include <base/wr/net_util.h>

using namespace Base;
using namespace Server;

TTcpIpv4Server::TTcpIpv4Server(int backlog, in_addr_t bind_addr,
    in_port_t port,
    std::unique_ptr<TConnectionHandlerApi> &&connection_handler,
    const TFatalErrorHandler &fatal_error_handler)
    : TStreamServerBase(backlog,
          reinterpret_cast<sockaddr *>(&ClientAddr), sizeof(ClientAddr),
          std::move(connection_handler), fatal_error_handler),
      BindAddr(bind_addr),
      Port(port) {
}

TTcpIpv4Server::TTcpIpv4Server(int backlog, in_addr_t bind_addr,
    in_port_t port,
    std::unique_ptr<TConnectionHandlerApi> &&connection_handler,
    TFatalErrorHandler &&fatal_error_handler)
    : TStreamServerBase(backlog,
          reinterpret_cast<sockaddr *>(&ClientAddr), sizeof(ClientAddr),
          std::move(connection_handler), std::move(fatal_error_handler)),
      BindAddr(bind_addr),
      Port(port) {
}

in_port_t TTcpIpv4Server::GetBindPort() const noexcept {
  assert(this);

  if (!IsBound()) {
    Die("Cannot get bind port for unbound listening socket");
  }

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  Wr::getsockname(GetListeningSocket(), reinterpret_cast<sockaddr *>(&addr),
      &addrlen);
  return ntohs(addr.sin_port);
}

void TTcpIpv4Server::InitListeningSocket(TFd &sock) {
  assert(this);
  TFd sock_fd(Wr::socket(Wr::TDisp::Nonfatal, {}, AF_INET, SOCK_STREAM, 0));
  int flag = true;
  Wr::setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  sockaddr_in serv_addr;
  std::memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(Port);
  serv_addr.sin_addr.s_addr = BindAddr;
  IfLt0(Wr::bind(sock_fd, reinterpret_cast<const sockaddr *>(&serv_addr),
      sizeof(serv_addr)));
  sock = std::move(sock_fd);
}
