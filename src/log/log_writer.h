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
     file path must be absolute (i.e. it must start with '/').  If 'file_path'
     is nonempty and identical to the value specified on the previous call to
     this function, the existing file descriptor for the logfile will be
     reused, rather than reopening the file.  If opening the logfile fails,
     std::system_error will be thrown and the call will otherwise have no
     effect (i.e. any previous log writer will be preserved). */
  void SetLogWriter(bool enable_stdout_stderr, bool enable_syslog,
      const std::string &file_path,
      mode_t file_mode = TFileLogWriter::DEFAULT_FILE_MODE);

  /* This is intended to be called only by unit tests.  It destroys any
     existing log writer. */
  void DropLogWriter();

  /* If a log writer has already been set (via a call to SetLogWriter() above),
     and it was configured to write to a file, this will attempt to reopen the
     logfile, and true will be returned, assuming that no exception is thrown.
     Otherwise, this is a no-op and false will be returned.

     If an attempted reopen fails, std::system_error will be thrown and the
     call will otherwise have no effect (i.e. the previous logfile descriptor
     will remain in use).  A successful call that causes the logfile to be
     reopened will replace the internally held log writer object, so that
     subsequent calls to GetLogWriter() will return the new log writer.  Any
     prior callers of GetLogWriter() that still hold references to the old log
     writer will continue to use it (and therefore the old file descriptor)
     until they drop their references. */
  bool HandleLogfileReopenRequest();

  /* Get a reference to the current global log writer.  If SetLogWriter() has
     not yet been called, returns a default log writer that logs only to
     stdout/stderr. */
  std::shared_ptr<TLogWriterBase> GetLogWriter() noexcept;

}
