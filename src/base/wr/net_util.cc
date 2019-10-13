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

#include <base/error_util.h>

using namespace Base;

int Base::Wr::accept(TDisp disp, std::initializer_list<int> errors, int sockfd,
    sockaddr *addr, socklen_t *addrlen) noexcept {
  const int ret = ::accept(sockfd, addr, addrlen);

  /* The man page says that ENOMEM often means that the memory allocation is
     limited by the socket buffer limits, not by the system memory.  Therefore
     treat ENOMEM as nonfatal. */
  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EFAULT, EINVAL, EMFILE, ENFILE, ENOTSOCK, EOPNOTSUPP})) {
    DieErrno("accept()", errno);
  }

  return ret;
}

int Base::Wr::bind(TDisp disp, std::initializer_list<int> errors, int sockfd,
    const sockaddr *addr, socklen_t addrlen) noexcept {
  const int ret = ::bind(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EINVAL, ENOTSOCK, EADDRNOTAVAIL, EFAULT, ENOMEM})) {
    DieErrno("bind()", errno);
  }

  return ret;
}

int Base::Wr::connect(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const sockaddr *addr, socklen_t addrlen) noexcept {
  const int ret = ::connect(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EAFNOSUPPORT, EADDRNOTAVAIL, EALREADY, EBADF, EFAULT, EISCONN,
          ENOTSOCK})) {
    DieErrno("connect()", errno);
  }

  return ret;
}

int Base::Wr::getsockname(TDisp disp, std::initializer_list<int> errors,
    int sockfd, sockaddr *addr, socklen_t *addrlen) noexcept {
  const int ret = ::getsockname(sockfd, addr, addrlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EFAULT, EINVAL, ENOBUFS, ENOTSOCK})) {
    DieErrno("getsockname()", errno);
  }

  return ret;
}

int Base::Wr::listen(TDisp disp, std::initializer_list<int> errors, int sockfd,
    int backlog) noexcept {
  const int ret = ::listen(sockfd, backlog);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, ENOTSOCK, EOPNOTSUPP})) {
    DieErrno("listen()", errno);
  }

  return ret;
}

ssize_t Base::Wr::recv(TDisp disp, std::initializer_list<int> errors,
    int sockfd, void *buf, size_t len, int flags) noexcept {
  const ssize_t ret = ::recv(sockfd, buf, len, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EFAULT, EINVAL, ENOMEM, ENOTCONN, ENOTSOCK})) {
    DieErrno("recv()", errno);
  }

  return ret;
}

ssize_t Base::Wr::recvmsg(TDisp disp, std::initializer_list<int> errors,
    int sockfd, msghdr *msg, int flags) noexcept {
  const ssize_t ret = ::recvmsg(sockfd, msg, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EFAULT, EINVAL, ENOMEM, ENOTCONN, ENOTSOCK})) {
    DieErrno("recvmsg()", errno);
  }

  return ret;
}

ssize_t Base::Wr::send(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const void *buf, size_t len, int flags) noexcept {
  const ssize_t ret = ::send(sockfd, buf, len, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EDESTADDRREQ, EFAULT, EINVAL, EISCONN, ENOMEM, ENOTCONN,
          ENOTSOCK, EOPNOTSUPP})) {
    DieErrno("send()", errno);
  }

  return ret;
}

ssize_t Base::Wr::sendmsg(TDisp disp, std::initializer_list<int> errors,
    int sockfd, const msghdr *msg, int flags) noexcept {
  const ssize_t ret = ::sendmsg(sockfd, msg, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EDESTADDRREQ, EFAULT, EINVAL, EISCONN, ENOMEM, ENOTCONN,
          ENOTSOCK, EOPNOTSUPP})) {
    DieErrno("sendmsg()", errno);
  }

  return ret;
}

int Base::Wr::setsockopt(TDisp disp, std::initializer_list<int> errors,
    int sockfd, int level, int optname, const void *optval,
    socklen_t optlen) noexcept {
  const int ret = ::setsockopt(sockfd, level, optname, optval, optlen);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EFAULT, EINVAL, ENOPROTOOPT, ENOTSOCK})) {
    DieErrno("setsockopt()", errno);
  }

  return ret;
}

int Base::Wr::socket(TDisp disp, std::initializer_list<int> errors, int domain,
    int type, int protocol) noexcept {
  const int ret = ::socket(domain, type, protocol);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EAFNOSUPPORT, EINVAL, EMFILE, ENFILE, ENOBUFS, ENOMEM,
          EPROTONOSUPPORT})) {
    DieErrno("socket()", errno);
  }

  return ret;
}

int Base::Wr::socketpair(TDisp disp, std::initializer_list<int> errors,
    int domain, int type, int protocol, int sv[2]) noexcept {
  const int ret = ::socketpair(domain, type, protocol, sv);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EAFNOSUPPORT, EFAULT, EMFILE, ENFILE, EOPNOTSUPP, EPROTONOSUPPORT})) {
    DieErrno("socketpair()", errno);
  }

  return ret;
}
