/* <log/file_log_writer.h>

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

   A log writer that writes to a file.
 */

#pragma once

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <log/file_log_writer_base.h>

namespace Log {

  class TFileLogWriter : public TFileLogWriterBase {
    NO_COPY_SEMANTICS(TFileLogWriter);

    public:
    /* Throws std::system_error on error opening file. */
    explicit TFileLogWriter(const char *path);

    virtual ~TFileLogWriter() noexcept = default;

    /* Write 'entry' to file.  A trailing newline will be appended. */
    virtual void WriteEntry(TLogEntryAccessApi &entry);

    private:
    Base::TFd Fd;
  };  // TFileLogWriter

}  // Log
