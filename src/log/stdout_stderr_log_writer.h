/* <log/stdout_stderr_log_writer.h>

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

   A log writer that writes to stdout or stderr, depending on the log level.
 */

#pragma once

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <log/file_log_writer_base.h>

namespace Log {

  class TStdoutStderrLogWriter : public TFileLogWriterBase {
    NO_COPY_SEMANTICS(TStdoutStderrLogWriter);

    public:
    TStdoutStderrLogWriter() = default;

    ~TStdoutStderrLogWriter() noexcept override = default;

    /* Write 'entry' to stdout or stderr, depending on the log level (severity
       of at least LOG_ERR goes to stderr).  A trailing newline will be
       appended. */
    void WriteEntry(TLogEntryAccessApi &entry) override;
  };  // TStdoutStderrLogWriter

}  // Log
