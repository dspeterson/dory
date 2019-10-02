/* <base/wr/signal_util.cc>

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

   Implements <base/wr/signal_util.h>.
 */

#include <base/wr/signal_util.h>

#include <cerrno>

#include <base/error_util.h>

using namespace Base;

int Base::Wr::pthread_sigmask(TDisp disp, std::initializer_list<int> errors,
    int how, const sigset_t *set, sigset_t *oldset) noexcept {
  const int ret = ::pthread_sigmask(how, set, oldset);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EINVAL})) {
    DieErrno("pthread_sigmask()", ret);
  }

  return ret;
}

int Base::Wr::sigaction(TDisp disp, std::initializer_list<int> errors,
    int signum, const struct sigaction *act,
    struct sigaction *oldact) noexcept {
  const int ret = ::sigaction(signum, act, oldact);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EINVAL})) {
    DieErrno("sigaction()", errno);
  }

  return ret;
}

int Base::Wr::sigaddset(TDisp disp, std::initializer_list<int> errors,
    sigset_t *set, int signum) noexcept {
  const int ret = ::sigaddset(set, signum);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EINVAL})) {
    DieErrno("sigaddset()", errno);
  }

  return ret;
}

int Base::Wr::sigdelset(TDisp disp, std::initializer_list<int> errors,
    sigset_t *set, int signum) noexcept {
  const int ret = ::sigdelset(set, signum);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EINVAL})) {
    DieErrno("sigdelset()", errno);
  }

  return ret;
}

int Base::Wr::sigemptyset(TDisp disp, std::initializer_list<int> errors,
    sigset_t *set) noexcept {
  const int ret = ::sigemptyset(set);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EINVAL})) {
    DieErrno("sigemptyset()", errno);
  }

  return ret;
}

int Base::Wr::sigfillset(TDisp disp, std::initializer_list<int> errors,
    sigset_t *set) noexcept {
  const int ret = ::sigfillset(set);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EINVAL})) {
    DieErrno("sigfillset()", errno);
  }

  return ret;
}

int Base::Wr::sigismember(TDisp disp, std::initializer_list<int> errors,
    const sigset_t *set, int signum) noexcept {
  const int ret = ::sigismember(set, signum);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EINVAL})) {
    DieErrno("sigismember()", errno);
  }

  return ret;
}
