/* <base/wr/signal_util.h>

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

   Wrappers for signal-related system/library calls.
 */

#pragma once

#include <cassert>
#include <initializer_list>

#include <signal.h>
#include <sys/types.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int kill(TDisp disp, std::initializer_list<int> errors, pid_t pid,
        int sig) noexcept;

    inline int kill(pid_t pid, int sig) noexcept {
      return kill(TDisp::AddFatal, {}, pid, sig);
    }

    int pthread_kill(TDisp disp, std::initializer_list<int> errors,
        pthread_t thread, int sig) noexcept;

    inline int pthread_kill(pthread_t thread, int sig) noexcept {
      return pthread_kill(TDisp::AddFatal, {}, thread, sig);
    }

    int pthread_sigmask(TDisp disp, std::initializer_list<int> errors,
        int how, const sigset_t *set, sigset_t *oldset) noexcept;

    inline void pthread_sigmask(int how, const sigset_t *set,
        sigset_t *oldset) noexcept {
      const int ret = pthread_sigmask(TDisp::AddFatal, {}, how, set, oldset);
      assert(ret == 0);
    }

    int sigaction(TDisp disp, std::initializer_list<int> errors, int signum,
        const struct sigaction *act, struct sigaction *oldact) noexcept;

    inline void sigaction(int signum, const struct sigaction *act,
        struct sigaction *oldact) noexcept {
      const int ret = sigaction(TDisp::AddFatal, {}, signum, act, oldact);
      assert(ret == 0);
    }

    int sigaddset(TDisp disp, std::initializer_list<int> errors, sigset_t *set,
        int signum) noexcept;

    inline void sigaddset(sigset_t *set, int signum) noexcept {
      const int ret = sigaddset(TDisp::AddFatal, {}, set, signum);
      assert(ret == 0);
    }

    int sigdelset(TDisp disp, std::initializer_list<int> errors, sigset_t *set,
        int signum) noexcept;

    inline void sigdelset(sigset_t *set, int signum) noexcept {
      const int ret = sigdelset(TDisp::AddFatal, {}, set, signum);
      assert(ret == 0);
    }

    int sigemptyset(TDisp disp, std::initializer_list<int> errors,
        sigset_t *set) noexcept;

    inline void sigemptyset(sigset_t *set) noexcept {
      const int ret = sigemptyset(TDisp::AddFatal, {}, set);
      assert(ret == 0);
    }

    int sigfillset(TDisp disp, std::initializer_list<int> errors,
        sigset_t *set) noexcept;

    inline void sigfillset(sigset_t *set) noexcept {
      const int ret = sigfillset(TDisp::AddFatal, {}, set);
      assert(ret == 0);
    }

    int sigismember(TDisp disp, std::initializer_list<int> errors,
        const sigset_t *set, int signum) noexcept;

    inline bool sigismember(const sigset_t *set, int signum) noexcept {
      const int ret = sigismember(TDisp::AddFatal, {}, set, signum);
      assert((ret == 0) || (ret == 1));
      return (ret != 0);
    }

    int sigprocmask(TDisp disp, std::initializer_list<int> errors, int how,
        const sigset_t *set, sigset_t *oldset) noexcept;

    inline void sigprocmask(int how, const sigset_t *set,
        sigset_t *oldset) noexcept {
      const int ret = sigprocmask(TDisp::AddFatal, {}, how, set, oldset);
      assert(ret == 0);
    }

  }  // Wr

}  // Base
