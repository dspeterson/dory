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

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int accept(TDisp disp, std::initializer_list<int> errors, int sockfd,
        sockaddr *addr, socklen_t *addrlen) noexcept;

    inline int accept(int sockfd, sockaddr *addr,
        socklen_t *addrlen) noexcept {
      return accept(TDisp::AddFatal, {}, sockfd, addr, addrlen);
    }

    int bind(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const sockaddr *addr, socklen_t addrlen) noexcept;

    inline int bind(int sockfd, const sockaddr *addr,
        socklen_t addrlen) noexcept {
      return bind(TDisp::AddFatal, {}, sockfd, addr, addrlen);
    }

    int connect(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const sockaddr *addr, socklen_t addrlen) noexcept;

    inline int connect(int sockfd, const sockaddr *addr,
        socklen_t addrlen) noexcept {
      return connect(TDisp::AddFatal, {}, sockfd, addr, addrlen);
    }

    int getaddrinfo(TDisp disp, std::initializer_list<int> errors,
        TDisp errno_disp, std::initializer_list<int> errno_values,
        const char *node, const char *service, const addrinfo *hints,
        addrinfo **res) noexcept;

    inline int getaddrinfo_disp(TDisp disp, std::initializer_list<int> errors,
        const char *node, const char *service, const addrinfo *hints,
        addrinfo **res) noexcept {
      return getaddrinfo(disp, errors, TDisp::AddFatal, {}, node, service,
          hints, res);
    }

    inline int getaddrinfo_errno_disp(TDisp errno_disp,
        std::initializer_list<int> errno_values, const char *node,
        const char *service, const addrinfo *hints, addrinfo **res) noexcept {
      return getaddrinfo(TDisp::AddFatal, {}, errno_disp, errno_values, node,
          service, hints, res);
    }

    inline int getaddrinfo(const char *node, const char *service,
        const addrinfo *hints, addrinfo **res) noexcept {
      return getaddrinfo(TDisp::AddFatal, {}, TDisp::AddFatal, {}, node,
          service, hints, res);
    }

    int getnameinfo(TDisp disp, std::initializer_list<int> errors,
        TDisp errno_disp, std::initializer_list<int> errno_values,
        const sockaddr *sa, socklen_t salen, char *host, size_t hostlen,
        char *serv, size_t servlen, int flags) noexcept;

    inline int getnameinfo_disp(TDisp disp, std::initializer_list<int> errors,
        const sockaddr *sa, socklen_t salen, char *host, size_t hostlen,
        char *serv, size_t servlen, int flags) noexcept {
      return getnameinfo(disp, errors, TDisp::AddFatal, {}, sa, salen, host,
          hostlen, serv, servlen, flags);
    }

    inline int getnameinfo_errno_disp(TDisp errno_disp,
        std::initializer_list<int> errno_values, const sockaddr *sa,
        socklen_t salen, char *host, size_t hostlen, char *serv,
        size_t servlen, int flags) noexcept {
      return getnameinfo(TDisp::AddFatal, {}, errno_disp, errno_values, sa,
          salen, host, hostlen, serv, servlen, flags);
    }

    inline int getnameinfo(const sockaddr *sa, socklen_t salen, char *host,
        size_t hostlen, char *serv, size_t servlen, int flags) noexcept {
      return getnameinfo(TDisp::AddFatal, {}, TDisp::AddFatal, {}, sa, salen,
          host, hostlen, serv, servlen, flags);
    }

    int getpeername(TDisp disp, std::initializer_list<int> errors, int sockfd,
        sockaddr *addr, socklen_t *addrlen) noexcept;

    inline void getpeername(int sockfd, sockaddr *addr,
        socklen_t *addrlen) noexcept {
      const int ret = getpeername(TDisp::AddFatal, {}, sockfd, addr, addrlen);
      assert(ret == 0);
    }

    int getsockname(TDisp disp, std::initializer_list<int> errors, int sockfd,
        sockaddr *addr, socklen_t *addrlen) noexcept;

    inline void getsockname(int sockfd, sockaddr *addr,
        socklen_t *addrlen) noexcept {
      const int ret = getsockname(TDisp::AddFatal, {}, sockfd, addr, addrlen);
      assert(ret == 0);
    }

    int getsockopt(TDisp disp, std::initializer_list<int> errors, int sockfd,
        int level, int optname, void *optval, socklen_t *optlen) noexcept;

    inline void getsockopt(int sockfd, int level, int optname, void *optval,
        socklen_t *optlen) noexcept {
      const int ret = getsockopt(TDisp::AddFatal, {}, sockfd, level, optname,
          optval, optlen);
      assert(ret == 0);
    }

    const char *inet_ntop(TDisp disp, std::initializer_list<int> errors,
        int af, const void *src, char *dst, socklen_t size) noexcept;

    inline const char *inet_ntop(int af, const void *src, char *dst,
        socklen_t size) noexcept {
      return inet_ntop(TDisp::AddFatal, {}, af, src, dst, size);
    }

    int inet_pton(TDisp disp, std::initializer_list<int> errors, int af,
        const char *src, void *dst) noexcept;

    inline int inet_pton(int af, const char *src, void *dst) noexcept {
      const int ret = inet_pton(TDisp::AddFatal, {}, af, src, dst);
      assert((ret == 0) || (ret == 1));
      return ret;
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

    ssize_t recvfrom(TDisp disp, std::initializer_list<int> errors, int sockfd,
        void *buf, size_t len, int flags, sockaddr *src_addr,
        socklen_t *addrlen) noexcept;

    inline ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
        sockaddr *src_addr, socklen_t *addrlen) noexcept {
      return recvfrom(TDisp::AddFatal, {}, sockfd, buf, len, flags, src_addr,
          addrlen);
    }

    ssize_t recvmsg(TDisp disp, std::initializer_list<int> errors, int sockfd,
        msghdr *msg, int flags) noexcept;

    inline ssize_t recvmsg(int sockfd, msghdr *msg, int flags) noexcept {
      return recvmsg(TDisp::AddFatal, {}, sockfd, msg, flags);
    }

    ssize_t send(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const void *buf, size_t len, int flags) noexcept;

    inline ssize_t send(int sockfd, const void *buf, size_t len,
        int flags) noexcept {
      return send(TDisp::AddFatal, {}, sockfd, buf, len, flags);
    }

    ssize_t sendmsg(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const msghdr *msg, int flags) noexcept;

    inline ssize_t sendmsg(int sockfd, const msghdr *msg, int flags) noexcept {
      return sendmsg(TDisp::AddFatal, {}, sockfd, msg, flags);
    }

    ssize_t sendto(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const void *buf, size_t len, int flags, const sockaddr *dest_addr,
        socklen_t addrlen) noexcept;

    inline ssize_t sendto(int sockfd, const void *buf, size_t len,
        int flags, const sockaddr *dest_addr, socklen_t addrlen) noexcept {
      return sendto(TDisp::AddFatal, {}, sockfd, buf, len, flags, dest_addr,
          addrlen);
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
