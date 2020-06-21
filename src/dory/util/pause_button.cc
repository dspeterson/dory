/* <dory/util/pause_button.cc>

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

   Implements <dory/util/pause_button.h>.
 */

#include <dory/util/pause_button.h>

#include <base/counter.h>

using namespace Dory;
using namespace Dory::Util;

DEFINE_COUNTER(PauseStarted);

void TPauseButton::Push() {
  std::lock_guard<std::mutex> lock(Mutex);

  if (!PauseActivated) {
    Button.Push();
    PauseActivated = true;
    PauseStarted.Increment();
  }
}

void TPauseButton::Reset() {
  Button.Reset();
  PauseActivated = false;
}
