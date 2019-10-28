/* <base/wr/fd_util.cc>

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

   Implements <base/wr/fd_util.h>.
 */

#include <base/wr/fd_util.h>

#include <algorithm>
#include <string>

#include <time.h>

#include <base/sig_masker.h>
#include <base/sig_set.h>

using namespace Base;

int Base::Wr::close(TDisp disp, std::initializer_list<int> errors,
    int fd) noexcept {
  int ret = ::close(fd);

  if (ret == 0) {
    TrackFdOp(TFdOp::Close, fd);
  } else {
    if (errno == EINTR) {
      /* We were interrupted by a signal.  This should be rare.  Try again with
         all signals blocked.  It's important for close() to succeed so we
         don't leak file descriptors. */
      TSigMasker masker(TSigSet(TSigSet::TListInit::Exclude, {}));
      ret = ::close(fd);

      if (ret == 0) {
        TrackFdOp(TFdOp::Close, fd);
        return 0;
      }

      assert(errno != EINTR);
    }

    if (IsFatal(errno, disp, errors, true /* list_fatal */, {EBADF, EIO})) {
      DieErrnoWr("close()", errno);
    }
  }

  assert((ret == 0) || (errno != EINTR));
  return ret;
}

int Base::Wr::dup(TDisp disp, std::initializer_list<int> errors,
    int oldfd) noexcept {
  const int ret = ::dup(oldfd);

  if (ret < 0) {
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EBADF, EMFILE})) {
      DieErrnoWr("dup()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Dup, oldfd, ret);
  }

  return ret;
}

int Base::Wr::dup2(TDisp disp, std::initializer_list<int> errors, int oldfd,
    int newfd) noexcept {
  int ret = ::dup2(oldfd, newfd);

  if (ret < 0) {
    if ((errno == EINTR) || (errno == EBUSY)) {
      /* Either we were interrupted by a signal or we hit the race condition
         with open() and dup() mentioned in the man page.  Block all signals to
         eliminate the possibility of EINTR past this point. */
      TSigMasker masker(TSigSet(TSigSet::TListInit::Exclude, {}));

      /* Executing more than a single iteration should be rare, since it can
         happen only if we hit the race condition. */
      do {
        ret = ::dup2(oldfd, newfd);
      } while ((ret < 0) && (errno == EBUSY));

      if (ret >= 0) {
        TrackFdOp(TFdOp::Dup, oldfd, newfd);
        return ret;
      }
    }

    assert((ret < 0) && (errno != EINTR) && (errno != EBUSY));

    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EBADF, EINVAL, EMFILE})) {
      DieErrnoWr("dup2()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Dup, oldfd, newfd);
  }

  return ret;
}

int Base::Wr::epoll_create1(TDisp disp, std::initializer_list<int> errors,
    int flags) noexcept {
  const int ret = ::epoll_create1(flags);

  if (ret < 0) {
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EINVAL, EMFILE, ENFILE, ENOMEM})) {
      DieErrnoWr("epoll_create1()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Create1, ret);
  }

  return ret;
}

int Base::Wr::epoll_ctl(TDisp disp, std::initializer_list<int> errors,
    int epfd, int op, int fd, epoll_event *event) noexcept {
  const int ret = ::epoll_ctl(epfd, op, fd, event);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EEXIST, EINVAL, ENOENT, ENOMEM, ENOSPC, EPERM})) {
    DieErrnoWr("epoll_ctl()", errno);
  }

  return ret;
}

int Base::Wr::epoll_wait(TDisp disp, std::initializer_list<int> errors,
    int epfd, epoll_event *events, int maxevents, int timeout) noexcept {
  const int ret = ::epoll_wait(epfd, events, maxevents, timeout);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL})) {
    DieErrnoWr("epoll_wait()", errno);
  }

  return ret;
}

int Base::Wr::eventfd(TDisp disp, std::initializer_list<int> errors,
    unsigned int initval, int flags) noexcept {
  const int ret = ::eventfd(initval, flags);

  if (ret < 0) {
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EINVAL, EMFILE, ENFILE, ENODEV, ENOMEM})) {
      DieErrnoWr("eventfd()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Create1, ret);
  }

  return ret;
}

int Base::Wr::eventfd_write(TDisp disp, std::initializer_list<int> errors,
    int fd, eventfd_t value) noexcept {
  const int ret = ::eventfd_write(fd, value);

  /* Narrowly define the default set of nonfatal errors as a subset of those
     that can be returned by write().  The set of possible errors from write()
     is rather open-ended due to the wide variety of file descriptor types.  If
     buggy code passes in a file descriptor of some unexpected type, we want to
     treat any resulting unexpected errno values as fatal. */
  if ((ret != 0) && IsFatal(errno, disp, errors, false /* list_fatal */,
      {EAGAIN, EWOULDBLOCK, EINTR})) {
    DieErrnoWr("eventfd_write()", errno);
  }

  return ret;
}

int Base::Wr::fcntl(TDisp disp, std::initializer_list<int> errors, int fd,
    int cmd) noexcept {
  static const std::initializer_list<int> arg_cmd_list =
      {F_DUPFD, F_DUPFD_CLOEXEC, F_SETFD, F_SETFL, F_SETLK, F_SETLKW, F_GETLK,
          F_SETOWN, F_GETOWN_EX, F_SETOWN_EX, F_SETSIG, F_SETLEASE, F_NOTIFY,
          F_SETPIPE_SZ};

  if (std::find(arg_cmd_list.begin(), arg_cmd_list.end(), cmd) !=
      arg_cmd_list.end()) {
    std::string msg("Must provide arg with fcntl() cmd ");
    msg += std::to_string(cmd);
    Die(msg.c_str());
  }

  const int ret = ::fcntl(fd, cmd);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT})) {
    DieErrnoWr("fcntl(fd, cmd)", errno);
  }

  return ret;
}

int Base::Wr::pipe(TDisp disp, std::initializer_list<int> errors,
    int pipefd[2]) noexcept {
  const int ret = ::pipe(pipefd);

  if (ret == 0) {
    TrackFdOp(TFdOp::Create2, pipefd[0], pipefd[1]);
  } else if (IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EMFILE, ENFILE})) {
    DieErrnoWr("pipe()", errno);
  }

  return ret;
}

int Base::Wr::pipe2(TDisp disp, std::initializer_list<int> errors,
    int pipefd[2], int flags) noexcept {
  const int ret = ::pipe2(pipefd, flags);

  if (ret == 0) {
    TrackFdOp(TFdOp::Create2, pipefd[0], pipefd[1]);
  } else if (IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EINVAL, EMFILE, ENFILE})) {
    DieErrnoWr("pipe2()", errno);
  }

  return ret;
}

int Base::Wr::poll(TDisp disp, std::initializer_list<int> errors, pollfd *fds,
    nfds_t nfds, int timeout) noexcept {
  const int ret = ::poll(fds, nfds, timeout);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EINVAL, ENOMEM})) {
    DieErrnoWr("poll()", errno);
  }

  return ret;
}

int Base::Wr::ppoll(TDisp disp, std::initializer_list<int> errors, pollfd *fds,
    nfds_t nfds, const timespec *timeout_ts,
    const sigset_t *sigmask) noexcept {
  const int ret = ::ppoll(fds, nfds, timeout_ts, sigmask);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EINVAL, ENOMEM})) {
    DieErrnoWr("ppoll()", errno);
  }

  return ret;
}

ssize_t Base::Wr::read(TDisp disp, std::initializer_list<int> errors, int fd,
    void *buf, size_t count) noexcept {
  const int ret = ::read(fd, buf, count);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL})) {
    DieErrnoWr("read()", errno);
  }

  return ret;
}

int Base::Wr::timerfd_create(TDisp disp, std::initializer_list<int> errors,
    int clockid, int flags) noexcept {
  const int ret = ::timerfd_create(clockid, flags);

  if (ret < 0) {
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EINVAL, EMFILE, ENFILE, ENODEV, ENOMEM})) {
      DieErrnoWr("timerfd_create()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Create1, ret);
  }

  return ret;
}

int Base::Wr::timerfd_settime(TDisp disp, std::initializer_list<int> errors,
    int fd, int flags, const itimerspec *new_value,
    itimerspec *old_value) noexcept {
  const int ret = ::timerfd_settime(fd, flags, new_value, old_value);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, EINVAL})) {
    DieErrnoWr("timerfd_settime()", errno);
  }

  return ret;
}

ssize_t Base::Wr::write(TDisp disp, std::initializer_list<int> errors,
    int fd, const void *buf, size_t count) noexcept {
  const ssize_t ret = ::write(fd, buf, count);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EDESTADDRREQ, EFAULT, EINVAL})) {
    DieErrnoWr("write()", errno);
  }

  return ret;
}
