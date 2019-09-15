/* <log/die_handler.cc>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Implements <log/die_handler.h>.
 */

#include <log/die_handler.h>

#include <log/log.h>

using namespace Log;

void Log::DieHandler(TPri pri, const char *msg,
    void *const *stack_trace_buffer, size_t stack_trace_size) noexcept {
  /* Write the fatal error message and stack trace regardless of what
     IsEnabled(pri) would return.  A fatal error is always interesting enough
     to log.  Parameter 'pri' will be passed to syslog() if syslog() logging is
     enabled. */

  std::shared_ptr<TLogWriterBase> writer = GetLogWriter();

  {
    /* TLogEntryType Destructor writes 'msg'.  Scope causes 'msg' to be written
       before stack trace is written. */
    TLogEntryType(std::shared_ptr<TLogWriterBase>(writer), pri) << msg;
  }

  writer->WriteStackTrace(pri, stack_trace_buffer, stack_trace_size);
}
