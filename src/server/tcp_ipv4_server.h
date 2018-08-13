/* <server/tcp_ipv4_server.h>

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

   Class for server that uses TCP/IPv4 sockets for communication with clients.
 */

#pragma once

#include <cassert>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <base/no_copy_semantics.h>
#include <server/stream_server_base.h>

namespace Server {

  /* This may be either instantiated directly or used as a base class. */
  class TTcpIpv4Server : public TStreamServerBase {
    NO_COPY_SEMANTICS(TTcpIpv4Server);

    public:
    /* Note: 'bind_addr' is in network byte order order, and will typically be
       htonl(INADDR_ANY).  To bind to an ephemeral port, pass a value of 0 for
       'port'. */
    TTcpIpv4Server(int backlog, in_addr_t bind_addr, in_port_t port,
        TConnectionHandlerApi *connection_handler,
        const TFatalErrorHandler &fatal_error_handler);

    TTcpIpv4Server(int backlog, in_addr_t bind_addr, in_port_t port,
        TConnectionHandlerApi *connection_handler,
        TFatalErrorHandler &&fatal_error_handler);

    ~TTcpIpv4Server() noexcept override = default;

    in_addr_t GetBindAddr() const noexcept {
      assert(this);
      return BindAddr;
    }

    in_port_t GetPort() const noexcept {
      assert(this);
      return Port;
    }

    const struct sockaddr_in &GetClientAddr() const noexcept {
      assert(this);
      return ClientAddr;
    }

    /* Get the actual port we are bound to.  Unless we are bound to an
       ephemeral port, this will be the same value that was passed in to the
       constructor. */
    in_port_t GetBindPort() const;

    protected:
    void InitListeningSocket(Base::TFd &sock) override;

    private:
    const in_addr_t BindAddr;

    const in_port_t Port;

    struct sockaddr_in ClientAddr;
  };  // TTcpIpv4Server

}  // Server
