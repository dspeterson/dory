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
#include <cstring>
#include <stdexcept>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>

#include <base/basename.h>
#include <base/error_utils.h>
#include <base/fd.h>
#include <base/no_default_case.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;

const char *Dory::Util::LogLevelToString(int level) {
  switch (level) {
    case LOG_ERR:
      return "LOG_ERR";
    case LOG_WARNING:
      return "LOG_WARNING";
    case LOG_NOTICE:
      return "LOG_NOTICE";
    case LOG_INFO:
      return "LOG_INFO";
    case LOG_DEBUG:
      break;
    NO_DEFAULT_CASE;
  }

  return "LOG_DEBUG";
}

int Dory::Util::StringToLogLevel(const char *level_string) {
  if (!std::strcmp(level_string, "LOG_ERR")) {
    return LOG_ERR;
  }

  if (!std::strcmp(level_string, "LOG_WARNING")) {
    return LOG_WARNING;
  }

  if (!std::strcmp(level_string, "LOG_NOTICE")) {
    return LOG_NOTICE;
  }

  if (!std::strcmp(level_string, "LOG_INFO")) {
    return LOG_INFO;
  }

  if (!std::strcmp(level_string, "LOG_DEBUG")) {
    return LOG_DEBUG;
  }

  throw std::logic_error("Bad log level string");
}

void Dory::Util::InitSyslog(const char *prog_name, int max_level,
    bool log_echo) {
  /* This is static in case syslog retains the passed in string pointer rather
     than making a copy of the string. */
  static const std::string prog_basename = Basename(prog_name);

  openlog(prog_basename.c_str(), LOG_PID | (log_echo ? LOG_PERROR : 0),
          LOG_USER);
  setlogmask(LOG_UPTO(max_level));
  syslog(LOG_NOTICE, "Log started");
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

  for (size_t i = 0; i < buf.size(); ++i) {
    if (buf[i] != 0xff) {
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

  int opt = size;
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
