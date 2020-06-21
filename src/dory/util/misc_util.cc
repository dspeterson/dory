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

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <vector>

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <base/counter.h>
#include <base/error_util.h>
#include <base/event_semaphore.h>
#include <base/fd.h>
#include <base/wr/net_util.h>
#include <log/log.h>
#include <server/signal_handler_thread.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace Log;
using namespace Server;

DEFINE_COUNTER(GotLogfileReopenRequest);
DEFINE_COUNTER(GotShutdownRequest);

static TEventSemaphore &GetShutdownRequestSem() {
  static TEventSemaphore sem;
  return sem;
}

static std::atomic_flag shutdown_flag = ATOMIC_FLAG_INIT;

void Dory::Util::RequestShutdown() noexcept {
  GotShutdownRequest.Increment();

  if (shutdown_flag.test_and_set()) {
    LOG(TPri::NOTICE) << "Got duplicate shutdown request";
  } else {
    LOG(TPri::NOTICE) << "Got shutdown request";

    try {
      GetShutdownRequestSem().Push();
    } catch (const std::exception &x) {
      LOG(TPri::ERR)
          << "Got exception when pushing shutdown request semaphore: "
          << x.what();
    } catch (...) {
      LOG(TPri::ERR)
          << "Got unknown exception when pushing shutdown request semaphore";
    }
  }
}

static void RequestLogfileReopen() noexcept {
  GotLogfileReopenRequest.Increment();
  LOG(TPri::NOTICE) << "Got SIGUSR1: request to reopen logfile";
  bool reopened = false;

  try {
    reopened = HandleLogfileReopenRequest();
  } catch (const std::exception &x) {
    LOG(TPri::ERR) << "Got exception while attempting to reopen logfile: "
        << x.what();
    return;
  } catch (...) {
    LOG(TPri::ERR)
        << "Got unknown exception while attempting to reopen logfile";
    return;
  }

  if (reopened) {
    LOG(TPri::NOTICE) << "Logfile was reopened";
  } else {
    LOG(TPri::NOTICE)
        << "Logfile reopen request ignored because file logging is disabled";
  }
}

void Dory::Util::ClearShutdownRequest() {
  GetShutdownRequestSem().Reset();
  shutdown_flag.clear();
}

const TFd &Dory::Util::GetShutdownRequestedFd() noexcept {
  return GetShutdownRequestSem().GetFd();
}

/* Callback called by signal handler thread on receipt of signal.  Called from
   normal thread context (not from actual signal handler). */
static void SignalCallback(int signum, const siginfo_t & /* info */) noexcept {
  switch (signum) {
    case SIGINT:
    case SIGTERM: {
      RequestShutdown();
      break;
    }
    case SIGUSR1: {
      RequestLogfileReopen();
      break;
    }
    default: {
      LOG(TPri::ERR) << "Got unknown signal " << signum;
      break;
    }
  }
}

void Dory::Util::StartSignalHandlerThread() {
  TSignalHandlerThread &handler_thread = TSignalHandlerThread::The();
  handler_thread.Init(SignalCallback, {SIGINT, SIGTERM, SIGUSR1});
  handler_thread.Start();
}

void Dory::Util::StopSignalHandlerThread() noexcept {
  try {
    TSignalHandlerThread &handler_thread = TSignalHandlerThread::The();

    if (handler_thread.IsStarted()) {
      handler_thread.RequestShutdown();
      handler_thread.Join();
    }
  } catch (const std::exception &x) {
    LOG(TPri::ERR) << "Exception during signal handler thread shutdown: "
        << x.what();
  } catch (...) {
    LOG(TPri::ERR) << "Unknown exception during signal handler thread shutdown";
  }
}

static bool RunUnixDgSocketTest(std::vector<uint8_t> &buf,
    const TFd fd_pair[]) {
  std::fill(buf.begin(), buf.end(), 0xff);

  for (; ; ) {
    ssize_t ret = Wr::send(Wr::TDisp::Nonfatal, {EINTR, EMSGSIZE}, fd_pair[0],
        &buf[0], buf.size(), 0);

    if (ret >= 0) {
      break;
    }

    if (errno == EMSGSIZE) {
      return false;
    }

    assert(errno == EINTR);
  }

  std::fill(buf.begin(), buf.end(), 0);
  ssize_t ret = 0;

  for (; ; ) {
    ret = Wr::recv(Wr::TDisp::Nonfatal, {EINTR}, fd_pair[1], &buf[0],
        buf.size(), 0);

    if (ret >= 0) {
      break;
    }

    assert(errno == EINTR);
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
    Wr::socketpair(AF_LOCAL, SOCK_DGRAM, 0, tmp_fd_pair);
    fd_pair[0] = tmp_fd_pair[0];
    fd_pair[1] = tmp_fd_pair[1];
  }

  if (RunUnixDgSocketTest(buf, fd_pair)) {
    return TUnixDgSizeTestResult::Pass;
  }

  auto opt = static_cast<int>(size);

  if (Wr::setsockopt(Wr::TDisp::Nonfatal, {EINVAL}, fd_pair[0],
      SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt)) < 0) {
    assert(errno == EINVAL);
    return TUnixDgSizeTestResult::Fail;
  }

  return RunUnixDgSocketTest(buf, fd_pair) ?
      TUnixDgSizeTestResult::PassWithLargeSendbuf :
      TUnixDgSizeTestResult::Fail;
}
