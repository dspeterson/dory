/* <base/wr/net_util.cc>

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

   Implements <base/wr/net_util.h>.
 */

#include <base/wr/net_util.h>

#include <cerrno>
#include <string>

#include <arpa/inet.h>

#include <base/error_util.h>
#include <base/wr/debug.h>

using namespace Base;

int Base::Wr::accept(TDisp disp, std::initializer_list<int> errors, int sockfd,
    sockaddr *addr, socklen_t *addrlen) noexcept {
  const int ret = ::accept(sockfd, addr, addrlen);

  if (ret < 0) {
    /* The man page says that ENOMEM often means that the memory allocation is
       limited by the socket buffer limits, not by the system memory.
       Therefore treat ENOMEM as nonfatal. */
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EBADF, EFAULT, EINVAL, EMFILE, ENFILE, ENOTSOCK, EOPNOTSUPP})) {
      DieErrnoWr("accept()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Create1, ret);
  }

  return ret;
}

int Base::Wr::bind(TDisp disp, std::initializer_list<int> errors, int sockfd,
    const sockaddr *addr, socklen_t addrlen) noexcept {
  const int ret = ::bind(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EINVAL, ENOTSOCK, EADDRNOTAVAIL, EFAULT, ENOMEM})) {
    DieErrnoWr("bind()", errno);
  }

  return ret;
}

int Base::Wr::connect(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const sockaddr *addr, socklen_t addrlen) noexcept {
  const int ret = ::connect(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EAFNOSUPPORT, EADDRNOTAVAIL, EALREADY, EBADF, EFAULT, EISCONN,
          ENOTSOCK})) {
    DieErrnoWr("connect()", errno);
  }

  return ret;
}

int Base::Wr::getaddrinfo(TDisp disp, std::initializer_list<int> errors,
    TDisp errno_disp, std::initializer_list<int> errno_values,
    const char *node, const char *service, const addrinfo *hints,
    addrinfo **res) noexcept {
  const int ret = ::getaddrinfo(node, service, hints, res);

  if (ret == 0) {
    return ret;  // success
  }

  if (ret == EAI_SYSTEM) {
    /* The man page doesn't mention what kinds of errno values can occur in
       this case.  Just look for a handful that should be treated as fatal in
       just about any context. */
    if (IsFatal(errno, errno_disp, errno_values, true /* list_fatal */,
        {EBADF, EFAULT, EINVAL, ENOMEM, EMFILE, ENFILE, ENOPROTOOPT,
            EPROTONOSUPPORT, ESOCKTNOSUPPORT, EOPNOTSUPP, EPFNOSUPPORT,
            EAFNOSUPPORT})) {
      DieErrnoWr("getaddrinfo()", errno);
    }

    return ret;
  }

  if (ret == EAI_MEMORY) {
    DieNoStackTrace("getaddrinfo() failed with EAI_MEMORY (out of memory)");
  }

  if (IsFatal(ret, disp, errors, true /* list_fatal */,
      {EAI_BADFLAGS, EAI_FAMILY, EAI_SERVICE, EAI_SOCKTYPE})) {
    std::string msg("getaddrinfo()failed with error code ");
    msg += std::to_string(ret);
    msg += ": ";
    msg += gai_strerror(ret);
    Die(msg.c_str());
  }

  return ret;
}

int Base::Wr::getnameinfo(TDisp disp, std::initializer_list<int> errors,
    TDisp errno_disp, std::initializer_list<int> errno_values,
    const sockaddr *sa, socklen_t salen, char *host, size_t hostlen,
    char *serv, size_t servlen, int flags) noexcept {
  const int ret = ::getnameinfo(sa, salen, host, hostlen, serv, servlen,
      flags);

  if (ret == 0) {
    return ret;  // success
  }

  if (ret == EAI_SYSTEM) {
    /* The man page doesn't mention what kinds of errno values can occur in
       this case.  Just look for a handful that should be treated as fatal in
       just about any context. */
    if (IsFatal(errno, errno_disp, errno_values, true /* list_fatal */,
        {EBADF, EFAULT, EINVAL, ENOMEM, EMFILE, ENFILE, ENOPROTOOPT,
            EPROTONOSUPPORT, ESOCKTNOSUPPORT, EOPNOTSUPP, EPFNOSUPPORT,
            EAFNOSUPPORT})) {
      DieErrnoWr("getnameinfo()", errno);
    }

    return ret;
  }

  if (ret == EAI_MEMORY) {
    DieNoStackTrace("getnameinfo() failed with EAI_MEMORY (out of memory)");
  }

  if (IsFatal(ret, disp, errors, true /* list_fatal */,
      {EAI_BADFLAGS, EAI_FAMILY})) {
    std::string msg("getnameinfo()failed with error code ");
    msg += std::to_string(ret);
    msg += ": ";
    msg += gai_strerror(ret);
    Die(msg.c_str());
  }

  return ret;
}

int Base::Wr::getpeername(TDisp disp, std::initializer_list<int> errors,
    int sockfd, sockaddr *addr, socklen_t *addrlen) noexcept {
  const int ret = ::getpeername(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOBUFS, ENOTCONN, ENOTSOCK})) {
    DieErrnoWr("getpeername()", errno);
  }

  return ret;
}

int Base::Wr::getsockname(TDisp disp, std::initializer_list<int> errors,
    int sockfd, sockaddr *addr, socklen_t *addrlen) noexcept {
  const int ret = ::getsockname(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOBUFS, ENOTSOCK})) {
    DieErrnoWr("getsockname()", errno);
  }

  return ret;
}

int Base::Wr::getsockopt(TDisp disp, std::initializer_list<int> errors,
    int sockfd, int level, int optname, void *optval, socklen_t *optlen) noexcept {
  const int ret = ::getsockopt(sockfd, level, optname, optval, optlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOPROTOOPT, ENOTSOCK})) {
    DieErrnoWr("getsockopt()", errno);
  }

  return ret;
}

const char *Base::Wr::inet_ntop(TDisp disp, std::initializer_list<int> errors,
    int af, const void *src, char *dst, socklen_t size) noexcept {
  const char *const ret = ::inet_ntop(af, src, dst, size);

  if (!ret && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EAFNOSUPPORT})) {
    DieErrnoWr("inet_ntop()", errno);
  }

  return ret;
}

int Base::Wr::inet_pton(TDisp disp, std::initializer_list<int> errors, int af,
    const char *src, void *dst) noexcept {
  const int ret = ::inet_pton(af, src, dst);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EAFNOSUPPORT})) {
    DieErrnoWr("inet_pton()", errno);
  }

  return ret;
}

int Base::Wr::listen(TDisp disp, std::initializer_list<int> errors, int sockfd,
    int backlog) noexcept {
  const int ret = ::listen(sockfd, backlog);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, ENOTSOCK, EOPNOTSUPP})) {
    DieErrnoWr("listen()", errno);
  }

  return ret;
}

ssize_t Base::Wr::recv(TDisp disp, std::initializer_list<int> errors,
    int sockfd, void *buf, size_t len, int flags) noexcept {
  const ssize_t ret = ::recv(sockfd, buf, len, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOMEM, ENOTCONN, ENOTSOCK})) {
    DieErrnoWr("recv()", errno);
  }

  return ret;
}

ssize_t Base::Wr::recvfrom(TDisp disp, std::initializer_list<int> errors,
    int sockfd, void *buf, size_t len, int flags, sockaddr *src_addr,
    socklen_t *addrlen) noexcept {
  const ssize_t ret = ::recvfrom(sockfd, buf, len, flags, src_addr, addrlen);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOMEM, ENOTCONN, ENOTSOCK})) {
    DieErrnoWr("recvfrom()", errno);
  }

  return ret;
}

ssize_t Base::Wr::recvmsg(TDisp disp, std::initializer_list<int> errors,
    int sockfd, msghdr *msg, int flags) noexcept {
  const ssize_t ret = ::recvmsg(sockfd, msg, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOMEM, ENOTCONN, ENOTSOCK})) {
    DieErrnoWr("recvmsg()", errno);
  }

  return ret;
}

ssize_t Base::Wr::send(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const void *buf, size_t len, int flags) noexcept {
  const ssize_t ret = ::send(sockfd, buf, len, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EDESTADDRREQ, EFAULT, EINVAL, EISCONN, ENOMEM, ENOTCONN,
          ENOTSOCK, EOPNOTSUPP})) {
    DieErrnoWr("send()", errno);
  }

  return ret;
}

ssize_t Base::Wr::sendmsg(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const msghdr *msg, int flags) noexcept {
  const ssize_t ret = ::sendmsg(sockfd, msg, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EDESTADDRREQ, EFAULT, EINVAL, EISCONN, ENOMEM, ENOTCONN,
          ENOTSOCK, EOPNOTSUPP})) {
    DieErrnoWr("sendmsg()", errno);
  }

  return ret;
}

ssize_t Base::Wr::sendto(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr,
    socklen_t addrlen) noexcept {
  const ssize_t ret = ::sendto(sockfd, buf, len, flags, dest_addr, addrlen);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EDESTADDRREQ, EFAULT, EINVAL, EISCONN, ENOMEM, ENOTCONN,
          ENOTSOCK, EOPNOTSUPP})) {
    DieErrnoWr("sendto()", errno);
  }

  return ret;
}

int Base::Wr::setsockopt(TDisp disp, std::initializer_list<int> errors,
    int sockfd, int level, int optname, const void *optval,
    socklen_t optlen) noexcept {
  const int ret = ::setsockopt(sockfd, level, optname, optval, optlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL, ENOPROTOOPT, ENOTSOCK})) {
    DieErrnoWr("setsockopt()", errno);
  }

  return ret;
}

int Base::Wr::socket(TDisp disp, std::initializer_list<int> errors, int domain,
    int type, int protocol) noexcept {
  const int ret = ::socket(domain, type, protocol);

  if (ret < 0) {
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EAFNOSUPPORT, EINVAL, EMFILE, ENFILE, ENOBUFS, ENOMEM,
            EPROTONOSUPPORT})) {
      DieErrnoWr("socket()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Create1, ret);
  }

  return ret;
}

int Base::Wr::socketpair(TDisp disp, std::initializer_list<int> errors,
    int domain, int type, int protocol, int sv[2]) noexcept {
  const int ret = ::socketpair(domain, type, protocol, sv);

  if (ret == 0) {
    TrackFdOp(TFdOp::Create2, sv[0], sv[1]);
  } else if (IsFatal(errno, disp, errors, true /* list_fatal */,
      {EAFNOSUPPORT, EFAULT, EMFILE, ENFILE, EOPNOTSUPP, EPROTONOSUPPORT})) {
    DieErrnoWr("socketpair()", errno);
  }

  return ret;
}
