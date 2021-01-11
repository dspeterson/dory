/* <log/combined_log_writer.cc>

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

   Implements <log/combined_log_writer.h>.
 */

#include <log/combined_log_writer.h>

using namespace Base;
using namespace Log;

TCombinedLogWriter::TCombinedLogWriter(bool enable_stdout_stderr,
    bool enable_syslog, const std::string &file_path,
    std::optional<mode_t> file_mode)
    : TLogWriterBase(),
      StdoutStderrLogWriter(enable_stdout_stderr),
      FileLogWriter(file_path, file_mode),
      SyslogLogWriter(enable_syslog) {
}

TCombinedLogWriter::TCombinedLogWriter(const TCombinedLogWriter &old_writer,
    bool enable_stdout_stderr, bool enable_syslog,
    const std::string &file_path, std::optional<mode_t> file_mode)
    : TLogWriterBase(),
      StdoutStderrLogWriter(enable_stdout_stderr),
      FileLogWriter((file_path == old_writer.FileLogWriter.GetPath()) ?
          old_writer.FileLogWriter :
          TFileLogWriter(file_path, file_mode)),
      SyslogLogWriter(enable_syslog) {
}

void TCombinedLogWriter::WriteEntry(
    Log::TLogEntryAccessApi &entry, bool no_stdout_stderr) const noexcept {
  StdoutStderrLogWriter.WriteEntry(entry, no_stdout_stderr);
  FileLogWriter.WriteEntry(entry, no_stdout_stderr);
  SyslogLogWriter.WriteEntry(entry, no_stdout_stderr);
}

void TCombinedLogWriter::WriteStackTrace(Log::TPri pri, void *const *buffer,
    size_t size, bool no_stdout_stderr) const noexcept {
  StdoutStderrLogWriter.WriteStackTrace(pri, buffer, size, no_stdout_stderr);
  FileLogWriter.WriteStackTrace(pri, buffer, size, no_stdout_stderr);
  SyslogLogWriter.WriteStackTrace(pri, buffer, size, no_stdout_stderr);
}
