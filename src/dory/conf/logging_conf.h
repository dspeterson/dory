/* <dory/conf/logging_conf.h>

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

   Class representing logging configuration obtained from Dory's config file.
 */

#pragma once

#include <string>

#include <sys/types.h>

#include <base/opt.h>
#include <dory/conf/conf_error.h>
#include <log/file_log_writer.h>
#include <log/pri.h>

namespace Dory {

  namespace Conf {

    class TLoggingRelativePath final : public TConfError {
      public:
      TLoggingRelativePath()
          : TConfError("Logfile path must be absolute") {
      }
    };  // TLoggingRelativePath

    class TLoggingInvalidFileMode final : public TConfError {
      public:
      TLoggingInvalidFileMode()
          : TConfError("Invalid logfile mode") {
      }
    };  // TLoggingInvalidFileMode

    struct TLoggingConf final {
      Log::TPri Pri = Log::TPri::NOTICE;

      bool EnableStdoutStderr = false;

      bool EnableSyslog = true;

      /* Must be empty string or absolute pathname.  Empty string indicates
         that file logging is disabled. */
      std::string FilePath;

      Base::TOpt<mode_t> FileMode;

      bool LogDiscards = true;

      void SetFileConf(const std::string &path,
          const Base::TOpt<mode_t> &mode);
    };  // TLoggingConf

  }  // Conf

}  // Dory
