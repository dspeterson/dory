/* <base/event_semaphore.cc>

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

   Implements <base/event_semaphore.h>.
 */

#include <base/event_semaphore.h>

#include <cassert>
#include <cerrno>
#include <cstdint>

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/sig_masker.h>
#include <base/wr/fd_util.h>

using namespace Base;

static void SetNonBlocking(int fd) noexcept {
  const int flags = Wr::fcntl(fd, F_GETFD, 0);
  assert(flags >= 0);
  const int ret = Wr::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  assert(ret >= 0);
}

TEventSemaphore::TEventSemaphore(int initial_count, bool nonblocking) noexcept
    : Fd(Wr::eventfd(initial_count, EFD_SEMAPHORE)) {
  if (nonblocking) {
    SetNonBlocking(Fd);
  }
}

void TEventSemaphore::Reset(int initial_count) noexcept {
  int flags = Wr::fcntl(Fd, F_GETFL, 0);
  TFd new_fd = Wr::eventfd(initial_count, EFD_SEMAPHORE);

  /* Xfer old flags to new FD, including nonblocking option if previously
     specified. */
  int ret = Wr::fcntl(new_fd, F_SETFL, flags);
  assert(ret >= 0);

  /* Save setting of "close on exec" flag. */
  flags = Wr::fcntl(Fd, F_GETFD, 0);
  assert(flags >= 0);

  /* dup() the new FD into the old one.  This prevents the FD number from
     changing, which clients may find helpful.  'new_fd' gets closed by its
     destructor on return. */
  int dup_fd = Wr::dup2(new_fd, Fd);
  assert(dup_fd == Fd);

  /* Preserve setting of close on exec flag. */
  ret = Wr::fcntl(dup_fd, F_SETFD, flags);
  assert(ret >= 0);
}

bool TEventSemaphore::Pop() noexcept {
  uint64_t dummy;
  ssize_t ret = Wr::read(Wr::TDisp::AddFatal, {EIO, EISDIR}, Fd, &dummy,
      sizeof(dummy));

  if (ret >= 0) {
    return true;  // fast path: success
  }

  if (errno == EAGAIN) {
    /* The nonblocking option was passed to the constructor, and the
       semaphore was unavailable when we tried to do the pop. */
    return false;  // fast path: nonblocking failure
  }

  /* We were interrupted by a signal.  Try again with all signals blocked, so
     only EAGAIN or fatal errors can cause read() to fail.  This avoids the
     cost of the extra system calls to block and unblock signals in most
     cases. */
  assert(errno == EINTR);
  TSigMasker masker(TSigSet(TSigSet::TListInit::Exclude, {}));
  ret = Wr::read(Wr::TDisp::AddFatal, {EIO, EISDIR}, Fd, &dummy,
      sizeof(dummy));

  if (ret >= 0) {
    return true;  // success
  }

  assert(errno == EAGAIN);

  /* The nonblocking option was passed to the constructor, and the
     semaphore was unavailable when we tried to do the pop. */
  return false;
}

bool TEventSemaphore::PopIntr() {
  uint64_t dummy;
  ssize_t ret = Wr::read(Wr::TDisp::AddFatal, {EIO, EISDIR}, Fd, &dummy,
      sizeof(dummy));

  if (ret >= 0) {
    return true;  // success
  }

  if (errno == EINTR) {
    ThrowSystemError(errno);  // interrupted by signal
  }

  /* The nonblocking option was passed to the constructor, and the
     semaphore was unavailable when we tried to do the pop. */
  assert(errno == EAGAIN);
  return false;
}

void TEventSemaphore::Push(int count) noexcept {
  Wr::eventfd_write(Fd, static_cast<eventfd_t>(count));
}
