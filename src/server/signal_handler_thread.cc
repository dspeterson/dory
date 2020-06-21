/* <server/signal_handler_thread.cc>

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

   Implements <server/signal_handler_thread.h>.
 */

#include <server/signal_handler_thread.h>

#include <cassert>
#include <cerrno>

#include <poll.h>
#include <signal.h>

#include <base/error_util.h>
#include <base/wr/fd_util.h>
#include <base/wr/signal_util.h>

using namespace Base;
using namespace Server;

TSignalHandlerThread &TSignalHandlerThread::The() {
  static TSignalHandlerThread singleton;
  return singleton;
}

void TSignalHandlerThread::Init(
    Server::TSignalHandlerThread::THandler handler_callback,
    std::initializer_list<int> signals) {
  if (Initialized) {
    Die("TSignalHandlerThread already initialized");
  }

  assert(handler_callback);

  /* Set says "block all but client-specified signals". */
  BlockedSet = TSigSet(TSigSet::TListInit::Exclude, signals);

  HandlerCallback = handler_callback;

  /* Initialize map with all signals that we will handle.  Set all values to
     false, indicating that no signals have been received yet. */
  for (int sig : signals) {
    assert((sig > 0) && (sig < NSIG));
    CaughtSignals.insert(std::make_pair(sig, TSigInfo()));
  }

  Initialized = true;
}

void TSignalHandlerThread::Start() {
  if (!Initialized) {
    Die("TSignalHandlerThread must be initialized before starting");
  }

  /* Block all signals and then start the thread, which starts with all signals
     blocked since it inherits our signal mask.  On return, leave all signals
     blocked for the caller.  From here onward, thread assumes all signal
     handling responsibility.  No other threads should unblock any signals. */
  assert(HandlerCallback);
  Wr::sigprocmask(SIG_SETMASK,
      TSigSet(TSigSet::TListInit::Exclude, {}).Get(), nullptr);
  DoStart();
}

void TSignalHandlerThread::Run() {
  assert(Initialized);
  assert(HandlerCallback);

  /* Install handler for all client-specified signals.  At this point, all
     signals are blocked. */
  InstallHandler();

  struct pollfd poll_fd;
  poll_fd.fd = GetShutdownRequestFd();
  poll_fd.events = POLLIN;

  for (; ; ) {
    poll_fd.revents = 0;

    /* Sleep until either a signal is received, or the file descriptor
       indicating that it's time for us to shut down becomes readable.
       Client-specified signals are unblocked inside ppoll(), and all signals
       are blocked everywhere else. */
    int ret = Wr::ppoll(&poll_fd, 1, nullptr, BlockedSet.Get());

    /* If we were awakened by a signal, ret will be -1 and errno will be EINTR.
       Otherwise, ret will be 1, indicating that it's time to shut down. */
    if (ret != -1) {
      /* Shutdown notifier file descriptor became readable.  It's time to shut
         down. */
      assert(ret == 1);
      assert(poll_fd.revents);
      break;
    }

    assert(errno == EINTR);
    assert(poll_fd.revents == 0);

    for (auto &item : CaughtSignals) {
      if (item.second.Caught) {
        /* We caught the given signal.  Call client-provided callback. */
        assert(item.first == item.second.Info.si_signo);
        HandlerCallback(item.first, item.second.Info);
        item.second.Clear();
      }
    }
  }
}

/* This is the only code that executes from signal handler context.  Do the
   minimal amount of work here, recording receipt of the signal.  When we
   return, the awakened handler thread will see that a signal was received and
   call the corresponding client-provided callback. */
void TSignalHandlerThread::Handler(int signum, siginfo_t *info,
    void * /* ucontext */) noexcept {
  assert(info);
  TSignalHandlerThread &t = The();
  const auto iter = t.CaughtSignals.find(signum);

  if (iter == t.CaughtSignals.end()) {
    assert(false);
    return;
  }

  iter->second.Set(*info);
}

void TSignalHandlerThread::InstallHandler() const noexcept {
  TSigSet block_all(TSigSet::TListInit::Exclude, {});

  /* Install our handler for each client-specified signal. */
  for (const auto &item : CaughtSignals) {
    struct sigaction act;
    Zero(act);
    act.sa_sigaction = Handler;
    act.sa_flags = SA_SIGINFO;

    /* Specifies that all signals are blocked during handler execution. */
    std::memcpy(&act.sa_mask, block_all.Get(), sizeof(act.sa_mask));

    Wr::sigaction(item.first, &act, nullptr);
  }
}
