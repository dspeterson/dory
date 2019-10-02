/* <base/wr/thread_util.h>

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

   Wrappers for thread-related system/library calls.
 */

#pragma once

#include <cassert>
#include <initializer_list>

#include <pthread.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int pthread_rwlock_destroy(TDisp disp, std::initializer_list<int> errors,
        pthread_rwlock_t *rwlock) noexcept;

    inline void pthread_rwlock_destroy(pthread_rwlock_t *rwlock) noexcept {
      const int ret = pthread_rwlock_destroy(TDisp::AddFatal, {}, rwlock);
      assert(ret == 0);
    }

    int pthread_rwlock_init(TDisp disp, std::initializer_list<int> errors,
        pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) noexcept;

    inline void pthread_rwlock_init(pthread_rwlock_t *rwlock,
        const pthread_rwlockattr_t *attr) noexcept {
      const int ret = pthread_rwlock_init(TDisp::AddFatal, {}, rwlock, attr);
      assert(ret == 0);
    }

    int pthread_rwlock_rdlock(TDisp disp, std::initializer_list<int> errors,
        pthread_rwlock_t *rwlock) noexcept;

    inline void pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) noexcept {
      const int ret = pthread_rwlock_rdlock(TDisp::AddFatal, {}, rwlock);
      assert(ret == 0);
    }

    int pthread_rwlock_unlock(TDisp disp, std::initializer_list<int> errors,
        pthread_rwlock_t *rwlock) noexcept;

    inline void pthread_rwlock_unlock(pthread_rwlock_t *rwlock) noexcept {
      const int ret = pthread_rwlock_unlock(TDisp::AddFatal, {}, rwlock);
      assert(ret == 0);
    }

    int pthread_rwlock_wrlock(TDisp disp, std::initializer_list<int> errors,
        pthread_rwlock_t *rwlock) noexcept;

    inline void pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) noexcept {
      const int ret = pthread_rwlock_wrlock(TDisp::AddFatal, {}, rwlock);
      assert(ret == 0);
    }

  }  // Wr

}  // Base
