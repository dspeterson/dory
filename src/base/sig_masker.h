/* <base/sig_masker.h>

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

   RAII for setting the signal mask.
 */

#pragma once

#include <cassert>

#include <signal.h>

#include <base/error_util.h>
#include <base/no_copy_semantics.h>
#include <base/sig_set.h>
#include <base/wr/signal_util.h>

namespace Base {

  /* RAII for setting the signal mask. */
  class TSigMasker {
    NO_COPY_SEMANTICS(TSigMasker);

    public:
    /* Set the mask to the given set. */
    TSigMasker(const sigset_t &new_set) noexcept {
      Wr::pthread_sigmask(SIG_SETMASK, &new_set, &OldSet);
    }

    TSigMasker(const TSigSet &new_set) noexcept
        : TSigMasker(*new_set) {
    }

    /* Restore the mask. */
    ~TSigMasker() {
      assert(this);
      Wr::pthread_sigmask(SIG_SETMASK, &OldSet, nullptr);
    }

    const sigset_t &GetOldSet() const noexcept {
      assert(this);
      return OldSet;
    }

    private:
    /* The set to which we will restore. */
    sigset_t OldSet;
  };  // TSigMasker

}  // Base
