/* <log/file_log_writer_base.cc>

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

   Implements <log/file_log_writer_base.h>.
 */

#include <log/file_log_writer_base.h>

#include <cassert>
#include <cstddef>
#include <cstring>

#include <unistd.h>

using namespace Log;

void TFileLogWriterBase::DoWriteEntry(int fd, TLogEntryAccessApi &entry) {
  assert(this);
  auto ret = entry.GetWithTrailingNewline();

  if (write(fd, ret.first, ret.second - ret.first) < 0) {
    throw TLogWriteError();
  }
}
