/* <log/pri.h>

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

   Logging priorities and masks.  Loosely inspired by priorities and macros
   LOG_MASK() and LOG_UPTO() defined in <syslog.h>.
 */

#pragma once

#include <string>

namespace Log {

  enum class TPri : unsigned int {
    EMERG = 0,
    ALERT = 1,
    CRIT = 2,
    ERR = 3,
    WARNING = 4,
    NOTICE = 5,
    INFO = 6,
    DEBUG = 7
  };

  inline constexpr unsigned int Mask(TPri p) noexcept {
    return 1u << static_cast<unsigned int>(p);
  }

  inline constexpr unsigned int UpTo(TPri p) noexcept {
    return (1u << (static_cast<unsigned int>(p) + 1u)) - 1u;
  }

  unsigned int GetLogMask() noexcept;

  void SetLogMask(unsigned int mask) noexcept;

  inline bool IsEnabled(TPri p) noexcept {
    return ((GetLogMask() & Mask(p)) != 0);
  }

  const char *ToString(TPri p) noexcept;

  TPri ToPri(const char *pri_string);

  inline TPri ToPri(const std::string &pri_string) {
    return ToPri(pri_string.c_str());
  }

}
