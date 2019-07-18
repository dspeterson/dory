/* <log/stdout_stderr_log_writer.cc>

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

   Implements <log/stdout_stderr_log_writer.h>.
 */

#include <log/stdout_stderr_log_writer.h>

#include <log/pri.h>
#include <log/write_to_fd.h>

using namespace Log;

void TStdoutStderrLogWriter::SetErrorHandler(
    std::function<void() noexcept> const &handler) {
  ErrorHandler = handler;
}

void TStdoutStderrLogWriter::DoWriteEntry(
    TLogEntryAccessApi &entry) const noexcept {
  assert(this);

  if (Enabled) {
    try {
      /* Anything at least as severe as LOG_WARNING goes to stderr, so it
         doesn't get lost in the noise. */
      WriteToFd((entry.GetLevel() <= TPri::WARNING) ? 2 : 1, entry);
    } catch (...) {
      ErrorHandler();
    }
  }
}

void TStdoutStderrLogWriter::NullErrorHandler() noexcept {
}

std::function<void() noexcept> TStdoutStderrLogWriter::ErrorHandler{
    TStdoutStderrLogWriter::NullErrorHandler};
