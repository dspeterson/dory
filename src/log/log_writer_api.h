/* <log/log_writer_api.h>

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

   API for writing a log message.
 */

#pragma once

#include <cassert>
#include <list>
#include <stdexcept>
#include <string>

#include <log/log_entry_access_api.h>

namespace Log {

  class TLogWriterApi {
    public:
    /* Thrown by WriteEntry() on error writing to log.
       Warning: The caller should _not_ attempt to log this type of exception,
       since that would likely cause another TLogWriteError exception to be
       thrown.  An alternative way to handle this type of error is to increment
       a counter whose value can be queried through a REST API. */
    class TLogWriteError : public std::runtime_error {
      public:
      TLogWriteError()
          : std::runtime_error("Error writing log output") {
      }
    };  // TLogWriteError

    virtual ~TLogWriterApi() noexcept = default;

    /* Write 'entry'.  Throws TLogWriteError on error writing to log. */
    virtual void WriteEntry(TLogEntryAccessApi &entry) = 0;
  };  // TLogWriterApi

}  // Log
