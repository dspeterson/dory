/* <log/log_prefix_assign_api.h>

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

   API for assigning a prefix to a log entry.
 */

#pragma once

#include <cstddef>

#include <log/pri.h>

namespace Log {

  class TLogPrefixAssignApi {
    public:
    virtual ~TLogPrefixAssignApi() = default;

    /* Return the log level.  Levels correspond to those defined by syslog().
       Method appears here because log level probably of interest when
       assigning a prefix. */
    virtual TPri GetLevel() const noexcept = 0;

    /* 'start' and 'len' specifies a character sequence to be assigned as a log
       entry prefix, which doesn't need a C string terminator.  A prefix is
       intended to contain information such as the current date/time, the
       program name, and the log level.  The resulting log entry will be
       available either with or without its prefix.

       The motivation is to support writing a log entry to multiple
       destinations where only some destinations need a prefix.  Consider the
       case of logging to both syslog and a file.  Since syslog provides its
       own prefixes, we write a log entry without its prefix to syslog.
       Writing to a file, if we want a prefix, we must provide it ourselves, so
       we write an entry with its prefix.

       A log entry reserves a fixed amount of prefix space, which should be
       plenty for typical usage.  If the provided prefix is longer than that,
       it will be truncated. */
    virtual void AssignPrefix(const char *start, size_t len) noexcept = 0;
  };

}
