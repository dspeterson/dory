/* <dory/util/init_notifier.cc>

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

   Implements <dory/util/init_notifier.h>.
 */

#include <dory/util/init_notifier.h>

#include <system_error>

#include <unistd.h>

#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace Log;

TInitNotifier::TInitNotifier(TEventSemaphore &sem)
    : TEventSemaphoreNotifier(sem,
        [](const std::system_error &x) {
          LOG(TPri::ERR) << "Error notifying on server init finished: "
              << x.what();
          _exit(1);
        }) {
}
