/* <log/log_writer.h>

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

   Global log writer access.
 */

#pragma once

#include <memory>
#include <string>

#include <sys/stat.h>

#include <log/file_log_writer.h>
#include <log/log_writer_base.h>

namespace Log {

  /* Create a log writer configured as specified and install it as the global
     log writer.  An empty file path disables file logging.  If nonempty, the
     file path must be absolute (i.e. it must start with '/'). */
  void SetLogWriter(bool enable_stdout_stderr, bool enable_syslog,
      const std::string &file_path,
      mode_t file_mode = TFileLogWriter::default_file_mode);

  /* Get a reference to the current global log writer. */
  std::shared_ptr<TLogWriterBase> GetLogWriter();

}
