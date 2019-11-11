/* <log/file_log_writer.h>

   ----------------------------------------------------------------------------
   Copyright 2017-2019 Dave Peterson <dave@dspeterson.com>

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

   A log writer that writes to a file.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>

#include <base/fd.h>
#include <log/error_handler.h>
#include <log/log_entry_access_api.h>
#include <log/log_writer_base.h>
#include <log/pri.h>

namespace Log {

  class TFileLogWriter final : public TLogWriterBase {
    public:
    class TError : public std::runtime_error {
      public:
      const std::string &GetPath() const noexcept {
        assert(this);
        return Path;
      }

      protected:
      TError(const std::string &what_arg, const std::string &path)
        : std::runtime_error(what_arg),
          Path(path) {
      }

      private:
      std::string Path;
    };  // TError

    class TInvalidPath : public TError {
      public:
      explicit TInvalidPath(const std::string &path);
    };

    class TInvalidMode : public TError {
      public:
      TInvalidMode(const std::string &path, mode_t mode);

      mode_t GetMode() const noexcept {
        assert(this);
        return Mode;
      }

      private:
      mode_t Mode;
    };

    /* Access to the error handler is not protected from multithreading races,
       so it should be set before concurrent access is possible. */
    static void SetErrorHandler(TWriteErrorHandler handler) noexcept;

    static const mode_t DEFAULT_FILE_MODE;

    /* Throws std::system_error on error opening file.  An empty path disables
       the writer.  If nonempty, the path must be absolute (i.e. it must start
       with '/'). */
    TFileLogWriter(const std::string &path,
        mode_t open_mode = DEFAULT_FILE_MODE);

    TFileLogWriter(const TFileLogWriter &) = default;

    bool IsEnabled() const noexcept {
      assert(this);
      const bool is_open = FdRef->IsOpen();
      assert(Path.empty() == !is_open);
      return is_open;
    }

    /* Returns empty string if no logfile is open. */
    const std::string &GetPath() const noexcept {
      assert(this);
      assert(Path.empty() == !IsEnabled());
      return Path;
    }

    mode_t GetOpenMode() const noexcept {
      assert(this);
      return OpenMode;
    }

    /* Write 'entry' to file.  A trailing newline will be appended. */
    void WriteEntry(TLogEntryAccessApi &entry,
        bool no_stdout_stderr) const noexcept override;

    /* The parameters represent the results from a call to backtrace().  Write
       a stack trace to the log. */
    void WriteStackTrace(TPri pri, void *const *buffer,
        size_t size, bool no_stdout_stderr) const noexcept override;

    private:
    static void NullErrorHandler(TLogWriteError error) noexcept;

    static TWriteErrorHandler ErrorHandler;

    const std::string Path;

    const mode_t OpenMode;

    /* Holding the file descriptor by shared_ptr facilitates copy construction.
     */
    const std::shared_ptr<Base::TFd> FdRef;
  };  // TFileLogWriter

}  // Log
