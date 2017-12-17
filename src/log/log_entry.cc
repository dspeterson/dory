/* <log/log_entry.cc>

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

   Implements <log/log_entry.h>.
 */

#include <log/log_entry.h>

#include <stdexcept>

#include <syslog.h>

using namespace Log;

static int CheckLevel(int level) {
  if ((level < LOG_EMERG) || (level > LOG_DEBUG)) {
    throw std::logic_error("Invalid log level");
  }

  return level;
}

TLogEntry::TLogEntry(TLogWriterApi &log_writer, int level)
      /* Reserve 2 bytes at the end of our buffer, so we can append a
         newline and C string terminator. */
    : TArrayOstreamBase<BUF_SIZE>(2),
      LogWriter(log_writer), 
      Level(CheckLevel(level)),
      Written(false) {
}

TLogEntry::~TLogEntry() noexcept {
  try {
    Write();
  } catch (...) {
  }
}

std::pair<const char *, const char *>
TLogEntry::GetWithoutTrailingNewline() noexcept {
  assert(this);
  char *end_pos = GetPos();
  *end_pos = '\0';  // C string terminator
  char *start_pos = GetBuf();
  assert(end_pos >= start_pos);
  return std::make_pair(start_pos, end_pos);
}

std::pair<const char *, const char *>
TLogEntry::GetWithTrailingNewline() noexcept {
  assert(this);
  char *end_pos = GetPos();
  *end_pos = '\n';  // append trailing newline
  ++end_pos;
  *end_pos = '\0';  // C string terminator
  char *start_pos = GetBuf();
  assert(end_pos > start_pos);
  return std::make_pair(start_pos, end_pos);
}

int TLogEntry::GetLevel() const noexcept {
  assert(this);
  return Level;
}

void TLogEntry::SetLevel(int level) noexcept {
  assert(this);
  Level = CheckLevel(level);
}

void TLogEntry::Write() {
  assert(this);

  if (!Written) {
    /* Mark entry as written, regardless of whether write operation throws. */
    Written = true;

    if (!IsEmpty()) {
      LogWriter.WriteEntry(*this);
    }
  }
}
