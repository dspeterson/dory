/* <server/signal_handler_thread.test.cc>

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

   Unit tests for <server/signal_handler_thread.h>.
 */

#include <server/signal_handler_thread.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/on_destroy.h>
#include <base/time_util.h>
#include <base/zero.h>
#include <signal/set.h>
#include <thread/fd_managed_thread.h>

using namespace Base;
using namespace Server;
using namespace Signal;
using namespace Thread;

namespace {

  /* The fixture for testing class TFdManagedThread. */
  class TSignalHandlerThreadTest : public ::testing::Test {
    protected:
    TSignalHandlerThreadTest() = default;

    ~TSignalHandlerThreadTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TSignalHandlerThreadTest

  static std::atomic<bool> GotSIGUSR1(false);

  static siginfo_t InfoSIGUSR1;

  static std::atomic<bool> GotSIGCHLD(false);

  static siginfo_t InfoSIGCHLD;

  static void SignalCallback(int signum, const siginfo_t &info) noexcept {
    ASSERT_TRUE((signum == SIGUSR1) || (signum == SIGCHLD));

    switch (signum) {
      case SIGUSR1: {
        /* Do memcpy() _before_ setting GotSIGUSR1 so other thread is
           guaranteed to see what we wrote to InfoSIGUSR1 when it sees
           GotSIGUSR1 become true. */
        std::memcpy(&InfoSIGUSR1, &info, sizeof(info));
        GotSIGUSR1.store(true);
        break;
      }
      case SIGCHLD: {
        /* Do memcpy() _before_ setting GotSIGCHLD so other thread is
           guaranteed to see what we wrote to InfoSIGCHLD when it sees
           GotSIGCHLD become true. */
        std::memcpy(&InfoSIGCHLD, &info, sizeof(info));
        GotSIGCHLD.store(true);
        break;
      }
      default: {
        ASSERT_TRUE(false);
        break;
      }
    }
  }

  TEST_F(TSignalHandlerThreadTest, BasicTest) {
    Zero(InfoSIGUSR1);
    GotSIGUSR1.store(false);
    Zero(InfoSIGCHLD);
    GotSIGCHLD.store(false);
    TSignalHandlerThread &handler_thread = TSignalHandlerThread::The();

    /* Make sure signal handler thread gets shut down, no matter what happens
       during test. */
    TOnDestroy thread_stop(
      [&handler_thread]() noexcept {
        if (handler_thread.IsStarted()) {
          try {
            handler_thread.RequestShutdown();
            handler_thread.Join();
          } catch (...) {
            ASSERT_TRUE(false);
          }
        }
      });

    handler_thread.Init(SignalCallback, {SIGUSR1, SIGCHLD});
    handler_thread.Start();
    const TSet mask = TSet::FromSigmask();

    /* For this thread, all signals should be blocked (except SIGKILL and
       SIGSTOP, which can't be blocked) after call to Start().  Here we don't
       bother checking blocked status for POSIX realtime signals. */
    for (int i = 1; i < 32 /* Linux-specific */; ++i) {
      ASSERT_TRUE((i == SIGKILL) || (i == SIGSTOP) || mask[i]);
    }

    ASSERT_FALSE(GotSIGUSR1.load());
    ASSERT_EQ(InfoSIGUSR1.si_signo, 0);
    ASSERT_FALSE(GotSIGCHLD.load());
    ASSERT_EQ(InfoSIGCHLD.si_signo, 0);

    /* Send SIGUSR1 to self.  Then make sure our callback got called for
       SIGUSR1. */
    int ret = kill(getpid(), SIGUSR1);
    ASSERT_EQ(ret, 0);

    for (size_t i = 0; i < 1000; ++i) {
      if (GotSIGUSR1.load()) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_TRUE(GotSIGUSR1.load());
    ASSERT_EQ(InfoSIGUSR1.si_signo, SIGUSR1);
    ASSERT_FALSE(GotSIGCHLD.load());
    ASSERT_EQ(InfoSIGCHLD.si_signo, 0);
    Zero(InfoSIGUSR1);
    GotSIGUSR1.store(false);

    /* fork() a child and cause the child to exit immediately.  This will cause
       us to get SIGCHLD.  Then make sure our callback got called for
       SIGCHLD. */
    const pid_t pid = fork();

    switch (pid) {
      case -1: {
        ASSERT_TRUE(false);  // fork() failed
        break;
      }
      case 0: {
        /* Child exits immediately.  Parent should then get SIGCHLD. */
        _exit(0);
        break;
      }
      default: {
        break;  // Parent continues executing.
      }
    }

    for (size_t i = 0; i < 1000; ++i) {
      if (GotSIGCHLD.load()) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_TRUE(GotSIGCHLD.load());
    ASSERT_EQ(InfoSIGCHLD.si_signo, SIGCHLD);
    ASSERT_EQ(InfoSIGCHLD.si_pid, pid);
    ASSERT_FALSE(GotSIGUSR1.load());
    ASSERT_EQ(InfoSIGUSR1.si_signo, 0);
    Zero(InfoSIGCHLD);
    GotSIGCHLD.store(false);

    /* Try SIGUSR1 again to verify that we still get notified for a second
       signal. */
    ret = kill(getpid(), SIGUSR1);
    ASSERT_EQ(ret, 0);

    for (size_t i = 0; i < 1000; ++i) {
      if (GotSIGUSR1.load()) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_TRUE(GotSIGUSR1.load());
    ASSERT_EQ(InfoSIGUSR1.si_signo, SIGUSR1);
    ASSERT_FALSE(GotSIGCHLD.load());
    ASSERT_EQ(InfoSIGCHLD.si_signo, 0);

    /* Tell signal handler thread to shut down.  Then verify that it shuts down
       properly. */
    ASSERT_TRUE(handler_thread.IsStarted());
    handler_thread.RequestShutdown();

    for (size_t i = 0; i < 1000; ++i) {
      if (handler_thread.GetShutdownWaitFd().IsReadable()) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_TRUE(handler_thread.GetShutdownWaitFd().IsReadable());

    /* Make sure handler didn't throw an exception. */
    try {
      handler_thread.Join();
    } catch (const TFdManagedThread::TWorkerError &) {
      ASSERT_TRUE(false);
    }
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
