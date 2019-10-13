/* <log/syslog_log_writer.h>

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

   A log writer that calls syslog().
 */

#pragma once

#include <cassert>
#include <cstddef>

#include <log/log_entry_access_api.h>
#include <log/log_writer_base.h>
#include <log/pri.h>

namespace Log {

  class TSyslogLogWriter final : public TLogWriterBase {
    public:
    /* Initialize syslog facility.  Must be called before constructing any
       TSyslogLogWriter objects.  Parameters 'ident', 'option', and 'facility'
       are passed directly to openlog(), although LOG_PERROR must _not_ be
       specified.

       WARNING: The memory pointed to by paramater 'ident' must not be freed
       for as long as the program writes to syslog.  This is because openlog()
       internally retains the pointer. */
    static void Init(const char *ident, int option, int facility) noexcept;

    explicit TSyslogLogWriter(bool enabled) noexcept;

    bool IsEnabled() const noexcept {
      assert(this);
      return Enabled;
    }

    /* Write 'entry'. */
    void WriteEntry(TLogEntryAccessApi &entry,
        bool no_stdout_stderr) const noexcept override;

    /* The parameters represent the results from a call to backtrace().  Write
       a stack trace to the log. */
    void WriteStackTrace(TPri pri, void *const *buffer,
        size_t size, bool no_stdout_stderr) const noexcept override;

    private:
    const bool Enabled;
  };  // TSyslogLogWriter

}  // Log
