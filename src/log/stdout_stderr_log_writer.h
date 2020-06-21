/* <log/stdout_stderr_log_writer.h>

   ----------------------------------------------------------------------------
   Copyright 2017-2019 Dave Peterson <dave@dspeterson.com>

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

#include <cstddef>

#include <log/error_handler.h>
#include <log/log_entry_access_api.h>
#include <log/log_writer_base.h>
#include <log/pri.h>

namespace Log {

  class TStdoutStderrLogWriter final : public TLogWriterBase {
    public:
    /* Access to the error handler is not protected from multithreading races,
       so it should be set before concurrent access is possible. */
    static void SetErrorHandler(TWriteErrorHandler handler) noexcept;

    explicit TStdoutStderrLogWriter(bool enabled) noexcept
        : Enabled(enabled) {
    }

    bool IsEnabled() const noexcept {
      return Enabled;
    }

    /* Write 'entry' to stdout or stderr, depending on the log level (severity
       of at least LOG_ERR goes to stderr).  A trailing newline will be
       appended. */
    void WriteEntry(TLogEntryAccessApi &entry,
        bool no_stdout_stderr) const noexcept override;

    /* The parameters represent the results from a call to backtrace().  Write
       a stack trace to the log. */
    void WriteStackTrace(TPri pri, void *const *buffer,
        size_t size, bool no_stdout_stderr) const noexcept override;

    private:
    static void NullErrorHandler(TLogWriteError error) noexcept;

    static TWriteErrorHandler ErrorHandler;

    const bool Enabled;
  };  // TStdoutStderrLogWriter

}  // Log
