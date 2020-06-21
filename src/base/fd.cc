/* <base/fd.cc>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   Implements <base/fd.h>.
 */

#include <base/fd.h>

#include <ctime>

#include <poll.h>

#include <base/sig_set.h>
#include <base/wr/fd_util.h>
#include <base/wr/net_util.h>

using namespace Base;

static void PreparePoll(int fd, int timeout, timespec *&tsp,
    pollfd &p) noexcept {
  assert(fd >= 0);

  if (timeout < 0) {
    tsp = nullptr;  // infinite timeout
  } else {
    timespec &ts = *tsp;
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000000;
  }

  p.fd = fd;
  p.events = POLLIN;
  p.revents = 0;
}

bool TFd::IsReadable(int timeout) const noexcept {
  assert(this);
  timespec ts;
  timespec *tsp = &ts;
  pollfd p;
  PreparePoll(OsHandle, timeout, tsp, p);

  /* Block all signals inside ppoll() so we don't have to deal with EINTR. */
  int ret = Wr::ppoll(&p, 1, tsp,
      TSigSet(TSigSet::TListInit::Exclude, {}).Get());

  return (ret != 0);
}

bool TFd::IsReadableIntr(int timeout) const {
  assert(this);
  timespec ts;
  timespec *tsp = &ts;
  pollfd p;
  PreparePoll(OsHandle, timeout, tsp, p);
  int ret = IfLt0(Wr::ppoll(&p, 1, tsp, nullptr));
  return (ret != 0);
}

void TFd::Pipe(TFd &readable, TFd &writeable, int flags) noexcept {
  int fds[2];
  Wr::pipe2(fds, flags);
  readable = TFd(fds[0], NoThrow);
  writeable = TFd(fds[1], NoThrow);
}

void TFd::SocketPair(TFd &lhs, TFd &rhs, int domain, int type,
    int proto) noexcept {
  int fds[2];
  Wr::socketpair(domain, type, proto, fds);
  lhs = TFd(fds[0], NoThrow);
  rhs = TFd(fds[1], NoThrow);
}

const TFd Base::In(0), Base::Out(1), Base::Err(2);
