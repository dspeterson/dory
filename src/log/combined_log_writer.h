/* <log/combined_log_writer.h>

   ----------------------------------------------------------------------------
   Copyright 2018-2019 Dave Peterson <dave@dspeterson.com>

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

   A log writer that writes to multiple destinations as appropriate.
 */

#pragma once

#include <optional>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>

#include <log/file_log_writer.h>
#include <log/log_writer_base.h>
#include <log/pri.h>
#include <log/stdout_stderr_log_writer.h>
#include <log/syslog_log_writer.h>

namespace Log {

  /* Log writer that writes to all appropriate destinations. */
  class TCombinedLogWriter final : public TLogWriterBase {
    public:
    /* For constructors below, an empty file path disables file logging.  If
       nonempty, the file path must be absolute (i.e. it must start with '/').
       */

    /* Create a new log writer from scratch, based on config. */
    TCombinedLogWriter(bool enable_stdout_stderr, bool enable_syslog,
        const std::string &file_path, std::optional<mode_t> file_mode);

    /* Create a new log writer, attempting to reuse any open file descriptor
       for logfile from 'old_writer'.  This avoids unnecessarily closing and
       reopening the logfile. */
    TCombinedLogWriter(const TCombinedLogWriter &old_writer,
        bool enable_stdout_stderr, bool enable_syslog,
        const std::string &file_path, std::optional<mode_t> file_mode);

    bool StdoutStderrLoggingIsEnabled() const noexcept {
      return StdoutStderrLogWriter.IsEnabled();
    }

    bool SyslogLoggingIsEnabled() const noexcept {
      return SyslogLogWriter.IsEnabled();
    }

    bool FileLoggingIsEnabled() const noexcept {
      return FileLogWriter.IsEnabled();
    }

    /* Returns empty string if no logfile is open. */
    const std::string &GetFilePath() const noexcept {
      return FileLogWriter.GetPath();
    }

    std::optional<mode_t> GetFileOpenMode() const noexcept {
      return FileLogWriter.GetOpenMode();
    }

    void WriteEntry(TLogEntryAccessApi &entry,
        bool no_stdout_stderr) const noexcept override;

    /* The parameters represent the results from a call to backtrace().  Write
       a stack trace to the log. */
    void WriteStackTrace(TPri pri, void *const *buffer,
        size_t size, bool no_stdout_stderr) const noexcept override;

    private:
    const TStdoutStderrLogWriter StdoutStderrLogWriter;

    const TFileLogWriter FileLogWriter;

    const TSyslogLogWriter SyslogLogWriter;
  };  // TCombinedLogWriter

}  // Log
