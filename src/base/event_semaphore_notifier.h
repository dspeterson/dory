/* <base/event_semaphore_notifier.h>

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

   RAII event semaphore notifier class.  Destructor makes sure semaphore push
   operation has occurred.
 */

#pragma once

#include <functional>
#include <system_error>

#include <base/event_semaphore.h>
#include <base/no_copy_semantics.h>

namespace Base {

  class TEventSemaphoreNotifier {
    NO_COPY_SEMANTICS(TEventSemaphoreNotifier);

    public:
    using TErrorHandler =
        std::function<void(const std::system_error &) noexcept>;

    TEventSemaphoreNotifier(TEventSemaphore &sem,
        const TErrorHandler &error_handler);

    /* This class supports subclassing so that a subclass can pass its own
       error handler in the constructor.  The alternative of creating a wrapper
       class that provides the error handler is a bit more verbose. */
    virtual ~TEventSemaphoreNotifier() {
      Notify();
    }

    void Notify() noexcept;

    private:
    bool Done = false;

    TEventSemaphore &Sem;

    TErrorHandler ErrorHandler;
  };  // TEventSemaphoreNotifier

}  // Base

