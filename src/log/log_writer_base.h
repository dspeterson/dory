/* <log/log_writer_base.h>

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

   Log writer base class.
 */

#pragma once

#include <cassert>
#include <cstddef>

#include <log/log_entry_access_api.h>
#include <log/pri.h>

namespace Log {

  /* Log writer base class. */
  class TLogWriterBase {
    public:
    virtual ~TLogWriterBase() = default;

    /* Write log entry. */
    virtual void WriteEntry(TLogEntryAccessApi &entry,
        bool no_stdout_stderr) const noexcept = 0;

    /* Log stack trace.

       The last two parameters represent the results from a call to
       backtrace().  If possible, the implementation should write the output
       directly to a file descriptor by calling backtrace_symbols_fd() for
       maximum reliability.  Output is not expected to include the usual log
       entry prefix.  The goal is to just write the stack trace in the
       simplest, least error-prone manner.
     */
    virtual void WriteStackTrace(TPri pri, void *const *buffer,
        size_t size, bool no_stdout_stderr) const noexcept = 0;

    protected:
    TLogWriterBase() = default;
  };  // TLogWriterBase

}  // Log
