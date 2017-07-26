/* <base/event_semaphore_notifier.cc>

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

   Implements <base/event_semaphore_notifier.h>.
 */

#include <base/event_semaphore_notifier.h>

using namespace Base;

TEventSemaphoreNotifier::TEventSemaphoreNotifier(TEventSemaphore &sem,
    const TErrorHandler &error_handler)
    : Done(false),
      Sem(sem), 
      ErrorHandler(error_handler) {
}

void TEventSemaphoreNotifier::Notify() noexcept {
  if (!Done) {
    try {
      Sem.Push();
      Done = true;
    } catch (const std::system_error &x) {
      ErrorHandler(x);
    }
  }
}
