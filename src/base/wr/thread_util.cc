/* <base/wr/thread_util.cc>

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

   Implements <base/wr/thread_util.h>.
 */

#include <base/wr/thread_util.h>

#include <cerrno>

#include <base/error_util.h>

using namespace Base;

int Base::Wr::pthread_rwlock_destroy(TDisp disp,
    std::initializer_list<int> errors, pthread_rwlock_t *rwlock) noexcept {
  const int ret = ::pthread_rwlock_destroy(rwlock);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* list_fatal */,
      {EBUSY, EINVAL})) {
    DieErrno("pthread_rwlock_destroy()", ret);
  }

  return ret;
}

int Base::Wr::pthread_rwlock_init(TDisp disp,
    std::initializer_list<int> errors, pthread_rwlock_t *rwlock,
    const pthread_rwlockattr_t *attr) noexcept {
  const int ret = ::pthread_rwlock_init(rwlock, attr);

  if ((ret != 0) &&
      IsFatal(ret, disp, errors, true /* list_fatal */,
          {EAGAIN, ENOMEM, EPERM, EBUSY, EINVAL})) {
    DieErrno("pthread_rwlock_init()", ret);
  }

  return ret;
}

int Base::Wr::pthread_rwlock_rdlock(TDisp disp,
    std::initializer_list<int> errors, pthread_rwlock_t *rwlock) noexcept {
  const int ret = ::pthread_rwlock_rdlock(rwlock);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* list_fatal */,
      {EINVAL, EAGAIN, EDEADLK})) {
    DieErrno("pthread_rwlock_rdlock()", ret);
  }

  return ret;
}

int Base::Wr::pthread_rwlock_unlock(TDisp disp,
    std::initializer_list<int> errors, pthread_rwlock_t *rwlock) noexcept {
  const int ret = ::pthread_rwlock_unlock(rwlock);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* list_fatal */,
      {EINVAL, EPERM})) {
    DieErrno("pthread_rwlock_unlock()", ret);
  }

  return ret;
}

int Base::Wr::pthread_rwlock_wrlock(TDisp disp,
    std::initializer_list<int> errors, pthread_rwlock_t *rwlock) noexcept {
  const int ret = ::pthread_rwlock_wrlock(rwlock);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* list_fatal */,
      {EINVAL, EDEADLK})) {
    DieErrno("pthread_rwlock_wrlock()", ret);
  }

  return ret;
}
