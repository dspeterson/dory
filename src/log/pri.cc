/* <log/pri.cc>

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

   Implements <log/pri.h>.
 */

#include <log/pri.h>

#include <atomic>
#include <cstring>
#include <stdexcept>

#include <base/no_default_case.h>

using namespace Log;

static std::atomic<unsigned int> LogMask(UpTo(TPri::INFO));

unsigned int Log::GetLogMask() noexcept {
  return LogMask.load(std::memory_order_relaxed);
}

void Log::SetLogMask(unsigned int mask) noexcept {
  LogMask.store(mask, std::memory_order_relaxed);
}

const char *Log::ToString(Log::TPri p) {
  const char *result = "";

  switch (p) {
    case TPri::EMERG:
      result = "EMERG";
      break;
    case TPri::ALERT:
      result = "ALERT";
      break;
    case TPri::CRIT:
      result = "CRIT";
      break;
    case TPri::ERR:
      result = "ERR";
      break;
    case TPri::WARNING:
      result = "WARNING";
      break;
    case TPri::NOTICE:
      result = "NOTICE";
      break;
    case TPri::INFO:
      result = "INFO";
      break;
    case TPri::DEBUG:
      result = "DEBUG";
      break;
    NO_DEFAULT_CASE;
  }

  return result;
}

TPri Log::ToPri(const char *pri_string) {
  if (!std::strcmp(pri_string, "EMERG")) {
    return TPri::EMERG;
  }

  if (!std::strcmp(pri_string, "ALERT")) {
    return TPri::ALERT;
  }

  if (!std::strcmp(pri_string, "CRIT")) {
    return TPri::CRIT;
  }

  if (!std::strcmp(pri_string, "ERR")) {
    return TPri::ERR;
  }

  if (!std::strcmp(pri_string, "WARNING")) {
    return TPri::WARNING;
  }

  if (!std::strcmp(pri_string, "NOTICE")) {
    return TPri::NOTICE;
  }

  if (!std::strcmp(pri_string, "INFO")) {
    return TPri::INFO;
  }

  if (!std::strcmp(pri_string, "DEBUG")) {
    return TPri::DEBUG;
  }

  std::string msg("Invalid log level: ");
  msg += pri_string;
  throw std::range_error(msg);
}
