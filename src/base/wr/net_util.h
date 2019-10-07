/* <base/wr/net_util.h>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Wrappers for network-related system/library calls.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <initializer_list>

#include <sys/socket.h>
#include <sys/types.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int connect(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const struct sockaddr *addr, socklen_t addrlen) noexcept;

    inline int connect(int sockfd, const struct sockaddr *addr,
        socklen_t addrlen) noexcept {
      return connect(TDisp::AddFatal, {}, sockfd, addr, addrlen);
    }

    int listen(TDisp disp, std::initializer_list<int> errors, int sockfd,
        int backlog) noexcept;

    inline int listen(int sockfd, int backlog) noexcept {
      return listen(TDisp::AddFatal, {}, sockfd, backlog);
    }

    ssize_t recv(TDisp disp, std::initializer_list<int> errors, int sockfd,
        void *buf, size_t len, int flags) noexcept;

    inline ssize_t recv(int sockfd, void *buf, size_t len,
        int flags) noexcept {
      return recv(TDisp::AddFatal, {}, sockfd, buf, len, flags);
    }

    ssize_t send(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const void *buf, size_t len, int flags) noexcept;

    inline ssize_t send(int sockfd, const void *buf, size_t len,
        int flags) noexcept {
      return send(TDisp::AddFatal, {}, sockfd, buf, len, flags);
    }

    int setsockopt(TDisp disp, std::initializer_list<int> errors, int sockfd,
        int level, int optname, const void *optval, socklen_t optlen) noexcept;

    inline void setsockopt(int sockfd, int level, int optname,
        const void *optval, socklen_t optlen) noexcept {
      const int ret = setsockopt(TDisp::AddFatal, {}, sockfd, level, optname,
          optval, optlen);
      assert(ret == 0);
    }

    int socket(TDisp disp, std::initializer_list<int> errors, int domain,
        int type, int protocol) noexcept;

    inline int socket(int domain, int type, int protocol) noexcept {
      return socket(TDisp::AddFatal, {}, domain, type, protocol);
    }

    int socketpair(TDisp disp, std::initializer_list<int> errors, int domain,
        int type, int protocol, int sv[2]) noexcept;

    inline void socketpair(int domain, int type, int protocol,
        int sv[2]) noexcept {
      const int ret = socketpair(TDisp::AddFatal, {}, domain, type, protocol,
          sv);
      assert(ret == 0);
    }

  }  // Wr

}  // Base
