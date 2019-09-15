/* <log/syslog_log_writer.cc>

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

   Implements <log/syslog_log_writer.h>.
 */

#include <log/syslog_log_writer.h>

#include <atomic>
#include <stdexcept>

#include <execinfo.h>
#include <syslog.h>

#include <base/error_utils.h>

using namespace Base;
using namespace Log;

/* true indicates that syslog() has been initialized. */
static std::atomic<bool> Initialized(false);

void TSyslogLogWriter::Init(const char *ident, int option, int facility) {
  if (option & LOG_PERROR) {
    /* To keep things simple, we will not support echoing of log messages to
       stderr.  If log output to stdout/stderr is desired,
       TStdoutStderrLogWriter should be used. */
    throw std::logic_error("Cannot initialize syslog subsystem because "
        "LOG_PERROR is not supported");
  }

  openlog(ident, option, facility);

  /* Allow logging at all levels.  We do our own level-based filtering, which
     is applied uniformly for syslog, stdout/stderr, and logfiles. */
  setlogmask(LOG_UPTO(LOG_DEBUG));

  Initialized = true;
}

TSyslogLogWriter::TSyslogLogWriter(bool enabled)
    : TLogWriterBase()
    , Enabled(enabled) {
  /* Copy constructor doesn't need to perform this check, since this
     constructor will create the very first object. */
  if (enabled && !Initialized) {
    throw std::logic_error("Must call TSyslogLogWriter::Init() before "
        "creating any enabled TSyslogLogWriter objects");
  }
}

void TSyslogLogWriter::WriteEntry(TLogEntryAccessApi &entry) const noexcept {
  assert(this);

  if (Enabled) {
    /* Pass log entry via "%s" rather than directly, since it may contain
       formatting characters.  This avoids injection vulnerabilities. */
    syslog(static_cast<int>(entry.GetLevel()), "%s",
        entry.Get(false /* with_prefix */,
            false /* with_trailing_newline */).first);

  }
}

void TSyslogLogWriter::WriteStackTrace(TPri pri, void *const *buffer,
    size_t size) const noexcept {
  assert(this);

  if (Enabled && Log::IsEnabled(pri)) {
    TBacktraceSymbols symbols(buffer, size);

    for (size_t i = 0; i < symbols.Size(); ++i) {
      /* Pass log entry via "%s" rather than directly, in case it contains
         formatting characters. */
      syslog(static_cast<int>(pri), "%s", symbols[i]);
    }
  }
}
