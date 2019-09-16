/* <log_util/init_logging.cc>

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

   Implements <log_util/init_logging.h>.
 */

#include <log_util/init_logging.h>

#include <cstring>
#include <ctime>

#include <syslog.h>
#include <unistd.h>

#include <base/basename.h>
#include <base/error_util.h>
#include <log/die_handler.h>
#include <log/log.h>
#include <log/log_entry.h>
#include <log/log_prefix_assign_api.h>
#include <log/log_writer.h>
#include <log/stdout_stderr_log_writer.h>
#include <log/syslog_log_writer.h>
#include <server/counter.h>

using namespace Base;
using namespace Log;
using namespace LogUtil;
using namespace Server;

SERVER_COUNTER(LogPrefixWriteFailed);
SERVER_COUNTER(LogToFileFailed);
SERVER_COUNTER(LogToStdoutStderrFailed);

static const char *GetProgName(const char *prog_name = nullptr) {
  static const std::string name = (prog_name == nullptr) ? "" : prog_name;
  return name.c_str();
}

/* Assign a prefix to the given log entry.  The prefix will look something like
   "2019-07-14 19:43:34.001 UTC dory[84828] WARNING: ", assuming that the
   value returned by entry.GetLevel() is TPri::WARNING. */
static void WriteLogPrefix(TLogPrefixAssignApi &entry) {
  struct timespec now;

  if (clock_gettime(CLOCK_REALTIME, &now)) {
    LogPrefixWriteFailed.Increment();
    return;
  }

  std::tm t;

  if (gmtime_r(&now.tv_sec, &t) == nullptr) {
    LogPrefixWriteFailed.Increment();
    return;
  }

  char buf[64];
  size_t bytes_written = std::strftime(buf, sizeof(buf), "%Y-%m-%d %T", &t);

  if ((bytes_written == 0) || (bytes_written >= sizeof(buf))) {
    LogPrefixWriteFailed.Increment();
    return;
  }

  char *pos = buf + bytes_written;
  size_t space_left = sizeof(buf) - bytes_written;
  size_t n_bytes = static_cast<size_t>(
      std::snprintf(pos, space_left, ".%03d UTC %s[%d] ",
          static_cast<int>(now.tv_nsec / 1000000), GetProgName(),
          static_cast<int>(getpid())));

  if (n_bytes >= space_left) {
    LogPrefixWriteFailed.Increment();
    return;
  }

  pos += n_bytes;
  space_left -= n_bytes;
  const char *level_string = ToString(entry.GetLevel());
  size_t level_len = std::strlen(level_string);

  if (level_len >= space_left) {
    LogPrefixWriteFailed.Increment();
    return;
  }

  std::strncpy(pos, level_string, space_left);
  pos += level_len;
  space_left -= level_len;

  if (space_left < 3) {
    LogPrefixWriteFailed.Increment();
    return;
  }

  *pos = ':';
  ++pos;
  *pos = ' ';
  ++pos;
  *pos = '\0';  // C string terminator
  entry.AssignPrefix(buf, static_cast<size_t>(pos - buf));
}

static void HandleStdoutStderrLogWriteFailure() {
  LogToStdoutStderrFailed.Increment();
}

static void HandleFileLogWriteFailure() {
  LogToFileFailed.Increment();
}

void LogUtil::InitLogging(const char *prog_name, Log::TPri max_level,
    bool enable_stdout_stderr, bool enable_syslog,
    const std::string &file_path, mode_t file_mode) {
  /* Initialize syslog() logging even if enable_syslog is false.  syslog()
     logging can be enabled at any time, and it requires that initialization
     has been completed, so do the initialization now.

     The call to GetProgName() remembers the program name so WriteLogPrefix()
     can use it.  Also, openlog() retains the passed in program name pointer so
     we must provide something that we will not free. */
  TSyslogLogWriter::Init(GetProgName(Basename(prog_name).c_str()), LOG_PID,
      LOG_USER);

  TStdoutStderrLogWriter::SetErrorHandler(HandleStdoutStderrLogWriteFailure);
  TFileLogWriter::SetErrorHandler(HandleFileLogWriteFailure);
  SetPrefixWriter(WriteLogPrefix);
  SetLogMask(UpTo(max_level));
  SetLogWriter(enable_stdout_stderr, enable_syslog, file_path, file_mode);
  SetDieHandler(DieHandler<TPri::ERR>);
  LOG(TPri::NOTICE) << "Log started";
}

void LogUtil::InitTestLogging(const char *prog_name, const std::string &file_path) {
  InitLogging(prog_name, TPri::DEBUG, false /* enable_stdout_stderr */,
      true /* enable_syslog */, file_path);
}
