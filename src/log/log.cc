/* <log/log.cc>

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

   Implements <log/log.h>.
 */

#include <log/log.h>

#include <execinfo.h>

#include <log/log_writer.h>

using namespace Log;

void Log::LogStackTrace(TPri pri) noexcept {
  static const int STACK_TRACE_SIZE = 128;
  void *trace_buf[STACK_TRACE_SIZE];
  const int trace_size = backtrace(trace_buf, STACK_TRACE_SIZE);
  assert(trace_size <= STACK_TRACE_SIZE);
  GetLogWriter()->WriteStackTrace(pri, trace_buf, STACK_TRACE_SIZE,
      false /* no_stdout_stderr */);
}
