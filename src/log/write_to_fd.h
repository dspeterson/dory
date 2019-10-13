/* <log/write_to_fd.h>

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

   Utility function for writing a log entry to a file descriptor.
 */

#pragma once

#include <log/log_entry_access_api.h>

namespace Log {

  enum class TFdWriteResult {
    Ok,
    ShortCount,
    Error
  };

  /* Write 'entry' to file descriptor 'fd'.  A trailing newline will be
     appended.  Return result of attempted write. */
  TFdWriteResult WriteToFd(int fd, TLogEntryAccessApi &entry) noexcept;

}  // Log
