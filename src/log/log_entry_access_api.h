/* <log/log_entry_access_api.h>

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

   API that a log writer uses to access a completed log entry.
 */

#pragma once

#include <utility>

namespace Log {

  class TLogEntryAccessApi {
    public:
    virtual ~TLogEntryAccessApi() noexcept = default;

    /* Return a pair of pointers, where the first pointer indicates the start
       of the log entry, and the second pointer points one byte past the last
       byte to be written to the log.  The log entry will _not_ have a newline
       appended, but it will have a C string terminator.

       Warning: Saving the return value of this method, writing more data to
       the log entry, and then passing the first pointer in the pair to a
       function that expects a C string is an error, since the string
       terminator will be missing.  In the above scenario, this method should
       be called again so that the string terminator is written again. */
    virtual std::pair<const char *, const char *>
    GetWithoutTrailingNewline() noexcept = 0;

    /* Return a pair of pointers, where the first pointer indicates the start
       of the log entry, and the second pointer points one byte past the last
       byte to be written to the log (which will be an appended newline).  The
       log entry will have a newline appended, and followed by a C string
       terminator.

       Warning: Saving the return value of this method, writing more data to
       the log entry, and then passing the first pointer in the pair to a
       function that expects a C string is an error, since the string
       terminator will be missing.  In the above scenario, this method should
       be called again so that the string terminator is written again. */
    virtual std::pair<const char *, const char *>
    GetWithTrailingNewline() noexcept = 0;

    /* Return the log level, as defined by syslog(). */
    virtual int GetLevel() const noexcept = 0;
  };  // TLogEntryAccessApi

}  // Log
