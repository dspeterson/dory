/* <server/tcp_ipv6_server.h>

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

   Class for server that uses TCP/IPv6 sockets for communication with clients.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <memory>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <base/no_copy_semantics.h>
#include <server/stream_server_base.h>

namespace Server {

  /* This may be either instantiated directly or used as a base class. */
  class TTcpIpv6Server : public TStreamServerBase {
    NO_COPY_SEMANTICS(TTcpIpv6Server);

    public:
    /* Note: 'bind_addr' will typically be 'in6addr_any'. */
    TTcpIpv6Server(int backlog, const in6_addr &bind_addr, in_port_t port,
        uint32_t scope_id,
        std::unique_ptr<TConnectionHandlerApi> &&connection_handler);

    /* Note: 'bind_addr' will typically be 'in6addr_any'. */
    TTcpIpv6Server(int backlog, const in6_addr &bind_addr, in_port_t port,
        std::unique_ptr<TConnectionHandlerApi> &&connection_handler);

    ~TTcpIpv6Server() override = default;

    const in6_addr &GetBindAddr() const noexcept {
      return BindAddr;
    }

    in_port_t GetPort() const noexcept {
      return Port;
    }

    uint32_t GetScopeId() const noexcept {
      return ScopeId;
    }

    const sockaddr_in6 &GetClientAddr() const noexcept {
      return ClientAddr;
    }

    /* Get the actual port we are bound to.  Unless we are bound to an
       ephemeral port, this will be the same value that was passed in to the
       constructor. */
    in_port_t GetBindPort() const noexcept;

    protected:
    void InitListeningSocket(Base::TFd &sock) override;

    private:
    const in6_addr BindAddr;

    const in_port_t Port;

    const uint32_t ScopeId;

    sockaddr_in6 ClientAddr;
  };  // TTcpIpv6Server

}  // Server
