/* <log/write_to_fd.cc>

   ----------------------------------------------------------------------------
   Copyright 2018 Dave Peterson <dave@dspeterson.com>

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

   Implements <log/write_to_fd.h>.
 */

#include <log/write_to_fd.h>

#include <unistd.h>

#include <base/error_utils.h>

using namespace Base;
using namespace Log;

void Log::WriteToFd(int fd, TLogEntryAccessApi &entry) {
  auto ret = entry.Get(true /* with_prefix */,
      true /* with_trailing_newline */);
  IfLt0(write(fd, ret.first, ret.second - ret.first));
}
