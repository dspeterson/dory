/* <dory/mock_kafka_server/thread_terminate_handler.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Implements <dory/mock_kafka_server/thread_terminate_handler.h>.
 */

#include <dory/mock_kafka_server/thread_terminate_handler.h>

#include <cassert>
#include <memory>
#include <utility>

using namespace Base;
using namespace Dory;
using namespace Dory::MockKafkaServer;

void TThreadTerminateHandler::OnEvent(int /*fd*/, short /*flags*/) {
  assert(this);

  /* The terminate handler deletes the object whose method we are now
     executing.  To be safe, make sure are not trying to access any of our own
     object state while it is being deleted. */
  std::function<void()> terminate_handler(std::move(TerminateHandler));
  terminate_handler();
}

void TThreadTerminateHandler::OnShutdown() {
  assert(this);
  Unregister();
}
