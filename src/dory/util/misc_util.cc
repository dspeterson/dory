/* <dory/util/misc_util.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Implements <dory/util/misc_util.h>.
 */

#include <dory/util/misc_util.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <base/basename.h>
#include <base/error_utils.h>
#include <base/fd.h>
#include <base/no_default_case.h>
#include <log/file_log_writer.h>
#include <log/log.h>
#include <log/log_entry.h>
#include <log/log_writer.h>
#include <log/stdout_stderr_log_writer.h>
#include <log/syslog_log_writer.h>
#include <server/counter.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace Log;

SERVER_COUNTER(LogPrefixWriteFailed);
SERVER_COUNTER(LogToFileFailed);
SERVER_COUNTER(LogToStdoutStderrFailed);

const char *GetProgName(const char *prog_name = nullptr) {
  static const std::string name = (prog_name == nullptr) ? "" : prog_name;
  return name.c_str();
}

/* Assign a prefix to the given log entry.  The prefix will look something like
   "2019-07-14 19:43:34.001 UTC dory[84828] (WARNING): ", assuming that the
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

void Dory::Util::InitLogging(const char *prog_name, TPri max_level,
    bool log_echo, const char *logfile_path) {
  /* The call to GetProgName() remembers the program name so WriteLogPrefix()
     can use it.  Also, openlog() retains the passed in program name pointer so
     we must provide something that we will not free. */
  TSyslogLogWriter::Init(GetProgName(Basename(prog_name).c_str()), LOG_PID,
      LOG_USER);

  TStdoutStderrLogWriter::SetErrorHandler(HandleStdoutStderrLogWriteFailure);
  TFileLogWriter::SetErrorHandler(HandleFileLogWriteFailure);
  SetPrefixWriter(WriteLogPrefix);
  SetLogMask(UpTo(max_level));

  /* TODO: Add config options for specifying log destination(s). */
  SetLogWriter(log_echo /* enable_stdout_stderr */, true /* enable_syslog */,
      std::string(logfile_path) /* file_path */);

  LOG(TPri::NOTICE) << "Log started";
}

static bool RunUnixDgSocketTest(std::vector<uint8_t> &buf,
    const TFd fd_pair[]) {
  std::fill(buf.begin(), buf.end(), 0xff);

  for (; ; ) {
    ssize_t ret = send(fd_pair[0], &buf[0], buf.size(), 0);

    if (ret < 0) {
      switch (errno) {
        case EINTR:
          continue;
        case EMSGSIZE:
          return false;
        default:
          IfLt0(ret);  // this will throw
      }
    }

    break;
  }

  std::fill(buf.begin(), buf.end(), 0);
  ssize_t ret = 0;

  for (; ; ) {
    ret = recv(fd_pair[1], &buf[0], buf.size(), 0);

    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }

      IfLt0(ret);  // this will throw
    }

    break;
  }

  if (static_cast<size_t>(ret) != buf.size()) {
    return false;
  }

  for (uint8_t value : buf) {
    if (value != 0xff) {
      return false;
    }
  }

  return true;
}

TUnixDgSizeTestResult Dory::Util::TestUnixDgSize(size_t size) {
  if (size > (16 * 1024 * 1024)) {
    /* Reject unreasonably large values. */
    return TUnixDgSizeTestResult::Fail;
  }

  std::vector<uint8_t> buf(size);
  TFd fd_pair[2];

  {
    int tmp_fd_pair[2];
    int err = socketpair(AF_LOCAL, SOCK_DGRAM, 0, tmp_fd_pair);
    IfLt0(err);
    fd_pair[0] = tmp_fd_pair[0];
    fd_pair[1] = tmp_fd_pair[1];
  }

  if (RunUnixDgSocketTest(buf, fd_pair)) {
    return TUnixDgSizeTestResult::Pass;
  }

  auto opt = static_cast<int>(size);
  int ret = setsockopt(fd_pair[0], SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));

  if (ret < 0) {
    if (errno == EINVAL) {
      return TUnixDgSizeTestResult::Fail;
    }

    IfLt0(ret);  // this will throw
  }

  return RunUnixDgSocketTest(buf, fd_pair) ?
      TUnixDgSizeTestResult::PassWithLargeSendbuf :
      TUnixDgSizeTestResult::Fail;
}
