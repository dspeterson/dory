/* <log/log_entry.cc>

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

   Implements <log/log_entry.h>.
 */

#include <log/log_entry.h>

using namespace Log;

static void NullPrefixWriter(TLogPrefixAssignApi &) noexcept {
}

TPrefixWriteFn PrefixWriter{NullPrefixWriter};

void Log::SetPrefixWriter(TPrefixWriteFn writer) noexcept {
  PrefixWriter = writer;
}

void Log::WritePrefix(TLogPrefixAssignApi &entry) noexcept {
  PrefixWriter(entry);
}
