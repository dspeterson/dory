/* <log/file_log_writer.cc>

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

   Implements <log/file_log_writer.h>.
 */

#include <log/file_log_writer.h>

#include <cassert>

#include <execinfo.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/error_util.h>
#include <log/write_to_fd.h>

using namespace Base;
using namespace Log;

static std::string MakeInvalidPathMsg(const std::string &path) {
  std::string msg("Logfile path must be absolute: [");
  msg += path;
  msg += "]";
  return std::move(msg);
}

static std::string MakeInvalidFileModeMsg(const std::string &path) {
  std::string msg("Invalid mode for logfile: [");
  msg += path;
  msg += "]";
  return std::move(msg);
}

static std::string ValidateFilePathAndMode(const std::string &path,
    mode_t mode) {
  if (!path.empty() && (path[0] != '/')) {
    throw TFileLogWriter::TInvalidPath(path);
  }

  if (mode & ~(S_IRWXU | S_IRWXG | S_IRWXO)) {
    throw TFileLogWriter::TInvalidMode(path, mode);
  }

  return path;
}

static TFd OpenLogPath(const char *path, mode_t mode) {
  return TFd(IfLt0(open(path, O_CREAT | O_APPEND | O_WRONLY, mode)));
}

TFileLogWriter::TInvalidPath::TInvalidPath(const std::string &path)
    : TError(MakeInvalidPathMsg(path), path) {
}

TFileLogWriter::TInvalidMode::TInvalidMode(const std::string &path,
    mode_t mode)
    : TError(MakeInvalidFileModeMsg(path), path),
      Mode(mode) {
}

void TFileLogWriter::SetErrorHandler(TErrorHandler handler) noexcept {
  ErrorHandler = handler;
}

TFileLogWriter::TFileLogWriter(const std::string &path, mode_t open_mode)
    : TLogWriterBase(),
      Path(ValidateFilePathAndMode(path, open_mode)),
      OpenMode(open_mode),
      FdRef(path.empty() ?
          new TFd() : new TFd(OpenLogPath(path.c_str(), open_mode))) {
}

void TFileLogWriter::WriteEntry(TLogEntryAccessApi &entry,
    bool /* no_stdout_stderr */) const noexcept {
  assert(this);

  if (IsEnabled()) {
    try {
      WriteToFd(*FdRef, entry);
    } catch (...) {
      ErrorHandler();
    }
  }
}

void TFileLogWriter::WriteStackTrace(TPri /* pri */, void *const *buffer,
    size_t size, bool /* no_stdout_stderr */) const noexcept {
  assert(this);

  if (IsEnabled()) {
    backtrace_symbols_fd(buffer, static_cast<int>(size), *FdRef);
  }
}

void TFileLogWriter::NullErrorHandler() noexcept {
}

TErrorHandler TFileLogWriter::ErrorHandler{TFileLogWriter::NullErrorHandler};
