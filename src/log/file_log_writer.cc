/* <log/file_log_writer.cc>

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

   Implements <log/file_log_writer.h>.
 */

#include <log/file_log_writer.h>

#include <cassert>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/error_utils.h>

using namespace Base;
using namespace Log;

static TFd OpenLogPath(const char *path) {
  TFd fd = open(path, O_CREAT | O_APPEND | O_WRONLY,
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  IfLt0(fd);
  return std::move(fd);
}

TFileLogWriter::TFileLogWriter(const char *path)
    : Fd(OpenLogPath(path)) {
}

void TFileLogWriter::WriteEntry(TLogEntryAccessApi &entry) {
  assert(this);
  DoWriteEntry(Fd, entry);
}
