/* <log/die_handler.h>

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

   Fatal error handler to pass to Base::SetDieHandler().
 */

#pragma once

#include <cstddef>

#include <log/pri.h>

namespace Log {

  /* Log fatal error message and stack trace.  Parameter 'pri' specifies the
     log level at which the output is logged, which will be passed to syslog()
     if syslog logging is enabled.  The output is written regardless of what
     IsEnabled(pri) would return, since a fatal error is always interesting
     enough to log. */
  void DieHandler(TPri pri, const char *msg, void *const *stack_trace_buffer,
                  size_t stack_trace_size) noexcept;

  template <TPri pri>
  void DieHandler(const char *msg, void *const *stack_trace_buffer,
      size_t stack_trace_size) noexcept {
    DieHandler(pri, msg, stack_trace_buffer, stack_trace_size);
  }

}  // Log
