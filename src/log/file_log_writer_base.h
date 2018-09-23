/* <log/file_log_writer_base.h>

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

   Base class for log writer that writes to file.
 */

#pragma once

#include <base/no_copy_semantics.h>
#include <log/log_writer_api.h>

namespace Log {

  class TFileLogWriterBase : public TLogWriterApi {
    NO_COPY_SEMANTICS(TFileLogWriterBase);

    public:
    TFileLogWriterBase() = default;

    ~TFileLogWriterBase() override = default;

    protected:
    /* Write 'entry' to file descriptor 'fd'.  A trailing newline will be
       appended. */
    virtual void DoWriteEntry(int fd, TLogEntryAccessApi &entry);
  };  // TFileLogWriterBase

}  // Log
