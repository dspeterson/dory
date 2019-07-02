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

#include <log/log_entry_access_api.h>

namespace Log {

  /* Log writer base class. */
  class TLogWriterBase {
    public:
    virtual ~TLogWriterBase() = default;

    void WriteEntry(TLogEntryAccessApi &entry) const noexcept {
      assert(this);
      DoWriteEntry(entry);
    }

    protected:
    TLogWriterBase() = default;

    /* Subclasses must implement this to handle the details of writing the log
       entry. */
    virtual void DoWriteEntry(TLogEntryAccessApi &entry) const noexcept = 0;
  };  // TLogWriterBase

}  // Log
