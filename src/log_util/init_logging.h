/* <log_util/init_logging.h>

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

   Logging subsystem initialization function.
 */

#pragma once

#include <string>

#include <sys/types.h>

#include <base/error_util.h>
#include <log/file_log_writer.h>
#include <log/pri.h>

namespace LogUtil {

  /* Initialize logging subsystem.  Parameter file_path must be either empty or
     an absolute pathname (starting with '/').  If file_path is empty, file
     logging will be disabled.  Note that all parameter values except
     'prog_name' can be changed at runtime via logging subsystem API.
   */
  void InitLoggingCommon(const char *prog_name, Log::TPri max_level,
      bool enable_stdout_stderr, bool enable_syslog,
      const std::string &file_path, mode_t file_mode,
      Base::TDieHandler die_handler);

  /* Same as above, but uses an internally defined TDieHandler suitable for
     non-test code. */
  void InitLogging(const char *prog_name, Log::TPri max_level,
      bool enable_stdout_stderr, bool enable_syslog,
      const std::string &file_path,
      mode_t file_mode = Log::TFileLogWriter::default_file_mode);

}  // LogUtil
