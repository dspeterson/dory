/* <base/wr/fd_util.h>

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

   Wrappers for system/library calls related to file descriptors.
 */

#pragma once

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <initializer_list>

#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/wr/common.h>
#include <base/wr/debug.h>

namespace Base {

  namespace Wr {

    int close(TDisp disp, std::initializer_list<int> errors, int fd) noexcept;

    inline void close(int fd) noexcept {
      const int ret = close(TDisp::AddFatal, {}, fd);
      assert(ret == 0);
    }

    int dup(TDisp disp, std::initializer_list<int> errors, int oldfd) noexcept;

    inline int dup(int oldfd) noexcept {
      const int ret = dup(TDisp::AddFatal, {}, oldfd);
      assert(ret >= 0);
      return ret;
    }

    int dup2(TDisp disp, std::initializer_list<int> errors, int oldfd,
        int newfd) noexcept;

    inline int dup2(int oldfd, int newfd) noexcept {
      const int ret = dup2(TDisp::AddFatal, {}, oldfd, newfd);
      assert(ret >= 0);
      return ret;
    }

    int epoll_create1(TDisp disp, std::initializer_list<int> errors,
        int flags) noexcept;

    inline int epoll_create1(int flags) noexcept {
      const int ret = epoll_create1(TDisp::AddFatal, {}, flags);
      assert(ret >= 0);
      return ret;
    }

    int epoll_ctl(TDisp disp, std::initializer_list<int> errors, int epfd,
        int op, int fd, epoll_event *event) noexcept;

    inline void epoll_ctl(int epfd, int op, int fd,
        epoll_event *event) noexcept {
      const int ret = epoll_ctl(TDisp::AddFatal, {}, epfd, op, fd, event);
      assert(ret == 0);
    }

    int epoll_wait(TDisp disp, std::initializer_list<int> errors, int epfd,
        epoll_event *events, int maxevents, int timeout) noexcept;

    inline int epoll_wait(int epfd, epoll_event *events, int maxevents,
        int timeout) noexcept {
      return epoll_wait(TDisp::AddFatal, {}, epfd, events, maxevents, timeout);
    }

    int eventfd(TDisp disp, std::initializer_list<int> errors,
        unsigned int initval, int flags) noexcept;

    inline int eventfd(unsigned int initval, int flags) noexcept {
      const int ret = eventfd(TDisp::AddFatal, {}, initval, flags);
      assert(ret >= 0);
      return ret;
    }

    int eventfd_write(TDisp disp, std::initializer_list<int> errors, int fd,
        eventfd_t value) noexcept;

    inline int eventfd_write(int fd, eventfd_t value) noexcept {
      return eventfd_write(TDisp::AddFatal, {}, fd, value);
    }

    int fcntl(TDisp disp, std::initializer_list<int> errors, int fd,
        int cmd) noexcept;

    inline int fcntl(int fd, int cmd) noexcept {
      return fcntl(TDisp::AddFatal, {}, fd, cmd);
    }

    template <typename T>
    int fcntl(TDisp disp, std::initializer_list<int> errors, int fd, int cmd,
        T arg) noexcept {
      const int ret = ::fcntl(fd, cmd, arg);

      if (ret < 0) {
        if (IsFatal(errno, disp, errors, true /* list_fatal */,
            {EBADF, EFAULT, EINVAL, EMFILE})) {
          DieErrnoWr("fcntl(fd, cmd, arg)", errno);
        }
      } else if ((arg == F_DUPFD) || (arg == F_DUPFD_CLOEXEC)) {
        TrackFdOp(TFdOp::Dup, fd, ret);
      }

      return ret;
    }

    template <typename T>
    int fcntl(int fd, int cmd, T arg) noexcept {
      return fcntl(TDisp::AddFatal, {}, fd, cmd, arg);
    }

    int pipe(TDisp disp, std::initializer_list<int> errors,
        int pipefd[2]) noexcept;

    inline void pipe(int pipefd[2]) noexcept {
      const int ret = pipe(TDisp::AddFatal, {}, pipefd);
      assert(ret == 0);
    }

    int pipe2(TDisp disp, std::initializer_list<int> errors, int pipefd[2],
        int flags) noexcept;

    inline void pipe2(int pipefd[2], int flags) noexcept {
      const int ret = pipe2(TDisp::AddFatal, {}, pipefd, flags);
      assert(ret == 0);
    }

    int poll(TDisp disp, std::initializer_list<int> errors, pollfd *fds,
        nfds_t nfds, int timeout) noexcept;

    inline int poll(pollfd *fds, nfds_t nfds, int timeout) noexcept {
      return poll(TDisp::AddFatal, {}, fds, nfds, timeout);
    }

    int ppoll(TDisp disp, std::initializer_list<int> errors, pollfd *fds,
        nfds_t nfds, const timespec *timeout_ts,
        const sigset_t *sigmask) noexcept;

    inline int ppoll(pollfd *fds, nfds_t nfds, const timespec *timeout_ts,
        const sigset_t *sigmask) noexcept {
      return ppoll(TDisp::AddFatal, {}, fds, nfds, timeout_ts, sigmask);
    }

    ssize_t read(TDisp disp, std::initializer_list<int> errors, int fd,
        void *buf, size_t count) noexcept;

    inline ssize_t read(int fd, void *buf, size_t count) noexcept {
      return read(TDisp::AddFatal, {}, fd, buf, count);
    }

    int timerfd_create(TDisp disp, std::initializer_list<int> errors,
        int clockid, int flags) noexcept;

    inline int timerfd_create(int clockid, int flags) noexcept {
      const int ret = timerfd_create(TDisp::AddFatal, {}, clockid, flags);
      assert(ret >= 0);
      return ret;
    }

    int timerfd_settime(TDisp disp, std::initializer_list<int> errors, int fd,
        int flags, const itimerspec *new_value,
        itimerspec *old_value) noexcept;

    inline void timerfd_settime(int fd, int flags, const itimerspec *new_value,
        itimerspec *old_value) noexcept {
      const int ret = timerfd_settime(TDisp::AddFatal, {}, fd, flags,
          new_value, old_value);
      assert(ret == 0);
    }

    ssize_t write(TDisp disp, std::initializer_list<int> errors, int fd,
        const void *buf, size_t count) noexcept;

    inline ssize_t write(int fd, const void *buf, size_t count) noexcept {
      return write(TDisp::AddFatal, {}, fd, buf, count);
    }

  }  // Wr

}  // Base
