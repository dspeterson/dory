/* <dory/conf/common_logging_conf.h>

   ----------------------------------------------------------------------------
   Copyright 2020 Dave Peterson <dave@dspeterson.com>

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

   Logging configuration to be shared between Dory and mock Kafka server.
 */

#pragma once

#include <optional>
#include <string>

#include <sys/types.h>

#include <log/pri.h>

namespace Dory {

  namespace Conf {

    struct TCommonLoggingConf final {
      Log::TPri Pri = Log::TPri::NOTICE;

      bool EnableStdoutStderr = false;

      bool EnableSyslog = true;

      /* Must be empty string or absolute pathname.  Empty string indicates
         that file logging is disabled. */
      std::string FilePath;

      std::optional<mode_t> FileMode;

      void SetFileConf(const std::string &path, std::optional<mode_t> mode);
    };  // TCommonLoggingConf

  }  // Conf

}  // Dory