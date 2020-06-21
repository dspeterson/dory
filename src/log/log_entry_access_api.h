/* <log/log_entry_access_api.h>

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

   API that a log writer uses to access a completed log entry.
 */

#pragma once

#include <cstddef>
#include <utility>

#include <log/log_prefix_assign_api.h>

namespace Log {

  class TLogEntryAccessApi : public TLogPrefixAssignApi {
    public:
    /* Return size in bytes of log prefix if one exists.  A return value of 0
       indicates no prefix. */
    virtual size_t PrefixSize() const noexcept = 0;

    bool HasPrefix() const noexcept {
      return (PrefixSize() > 0);
    }

    /* Return a pair of pointers, where the first pointer indicates the first
       byte to be written to the log, and the second pointer points one byte
       past the last byte to be written.  The log entry will have a C string
       terminator at the location pointed to by the second pointer in the
       returned pair.  If 'with_prefix' is true, the log entry will start with
       any prefix assigned to it via TLogPrefixAssignApi::AssignPrefix().  If
       'with_trailing_newline' is true, the log entry will have a newline
       appended.

       Warning: Saving the return value of this method, writing more data to
       the log entry, and then passing the first pointer in the pair to a
       function that expects a C string is an error, since the string
       terminator will be missing.  In the above scenario, this method should
       be called again so that the string terminator is written again. */
    virtual std::pair<const char *, const char *>
    Get(bool with_prefix, bool with_trailing_newline) noexcept = 0;
  };  // TLogEntryAccessApi

}  // Log
