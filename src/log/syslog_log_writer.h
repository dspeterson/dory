/* <log/syslog_log_writer.h>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

#include <base/no_copy_semantics.h>
#include <log/log_writer_api.h>

namespace Log {

  class TSyslogLogWriter : public TLogWriterApi {
    NO_COPY_SEMANTICS(TSyslogLogWriter);

    public:
    ~TSyslogLogWriter() override = default;

    /* Write 'entry'. */
    void WriteEntry(TLogEntryAccessApi &entry) override;
  };  // TSyslogLogWriter

}  // Log
