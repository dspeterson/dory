/* <dory/util/init_notifier.h>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

   Server initialization notifier.
 */

#pragma once

#include <base/event_semaphore.h>
#include <base/no_copy_semantics.h>

namespace Dory {

  namespace Util {

    class TInitNotifier final {
      NO_COPY_SEMANTICS(TInitNotifier);

      public:
      explicit TInitNotifier(Base::TEventSemaphore &sem) noexcept
          : Sem(sem) {
      }

      ~TInitNotifier() {
        Notify();
      }

      void Notify() noexcept {
        if (!Done) {
          Sem.Push();
          Done = true;
        }
      }

      private:
      bool Done = false;

      Base::TEventSemaphore &Sem;
    };  // TInitNotifier

  }  // Util

}  // Dory
