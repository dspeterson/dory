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

#include <sys/socket.h>

#include <base/error_util.h>

using namespace Base;

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

int Base::Wr::socketpair(TDisp disp, std::initializer_list<int> errors,
    int domain, int type, int protocol, int sv[2]) noexcept {
  const int ret = ::socketpair(domain, type, protocol, sv);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EAFNOSUPPORT, EFAULT, EMFILE, ENFILE, EOPNOTSUPP, EPROTONOSUPPORT})) {
    DieErrno("socketpair()", errno);
  }

  return ret;
}
