/* <thread/fd_managed_thread.test.cc>

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

   Unit test for <thread/fd_managed_thread.h>.
 */

#include <thread/fd_managed_thread.h>

#include <cstring>
#include <exception>
#include <stdexcept>

#include <unistd.h>

#include <base/tmp_file.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace TestUtil;
using namespace Thread;

namespace {

  /* The fixture for testing class TFdManagedThread. */
  class TFdManagedThreadTest : public ::testing::Test {
    protected:
    TFdManagedThreadTest() = default;

    ~TFdManagedThreadTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TFdManagedThreadTest

  class TTestWorker1 final : public TFdManagedThread {
    public:
    explicit TTestWorker1(bool &flag)
        : Flag(flag) {
    }

    ~TTestWorker1() override {
      ShutdownOnDestroy();
    }

    void Run() override;

    private:
    bool &Flag;
  };  // TTestWorker1

  void TTestWorker1::Run() {
    Flag = true;
  }

  class TTestWorker2 final : public TFdManagedThread {
    public:
    explicit TTestWorker2(bool &flag)
        : Flag(flag) {
    }

    ~TTestWorker2() override {
      ShutdownOnDestroy();
    }

    void Run() override;

    private:
    bool &Flag;
  };  // TTestWorker2

  void TTestWorker2::Run() {
    const TFd &fd = GetShutdownRequestFd();

    while (!fd.IsReadable()) {
      sleep(1);
    }

    Flag = true;
  }

  class TTestWorker3 final : public TFdManagedThread {
    public:
    TTestWorker3(bool &flag, bool throw_std_exception)
        : Flag(flag),
          ThrowStdException(throw_std_exception) {
    }

    ~TTestWorker3() override {
      ShutdownOnDestroy();
    }

    void Run() override;

    private:
    bool &Flag;

    bool ThrowStdException;
  };  // TTestWorker3

  const char BLURB_1[] = "blah";

  const char BLURB_2[] = "random junk";

  void TTestWorker3::Run() {
    const TFd &fd = GetShutdownRequestFd();

    while (!fd.IsReadable()) {
      sleep(1);
    }

    Flag = true;

    if (ThrowStdException) {
      throw std::length_error(BLURB_1);
    }

    throw BLURB_2;
  }

  TEST_F(TFdManagedThreadTest, Test1) {
    bool thread_executed = false;
    TTestWorker1 worker(thread_executed);
    ASSERT_FALSE(thread_executed);
    worker.Start();

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    worker.Join();
    ASSERT_TRUE(thread_executed);
    thread_executed = false;
    worker.Start();

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    worker.Join();
    ASSERT_TRUE(thread_executed);
  }

  TEST_F(TFdManagedThreadTest, Test2) {
    bool flag = false;
    TTestWorker2 worker(flag);
    worker.Start();
    sleep(2);
    ASSERT_FALSE(flag);
    worker.RequestShutdown();

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    worker.Join();
    ASSERT_TRUE(flag);

    flag = false;
    worker.Start();
    sleep(2);
    ASSERT_FALSE(flag);
    worker.RequestShutdown();

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    worker.Join();
    ASSERT_TRUE(flag);
  }

  TEST_F(TFdManagedThreadTest, Test3) {
    bool flag = false;
    TTestWorker3 worker(flag, true);
    worker.Start();
    ASSERT_FALSE(flag);
    worker.RequestShutdown();
    bool threw = false;

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    try {
      worker.Join();
    } catch (const TFdManagedThread::TWorkerError &) {
      threw = true;
    }

    ASSERT_TRUE(flag);
    ASSERT_TRUE(threw);

    flag = false;
    worker.Start();
    ASSERT_FALSE(flag);
    worker.RequestShutdown();
    threw = false;

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    try {
      worker.Join();
    } catch (const TFdManagedThread::TWorkerError &x) {
      try {
        std::rethrow_exception(x.ThrownException);
      } catch (const std::length_error &y) {
        threw = true;
        ASSERT_TRUE(std::strstr(y.what(), BLURB_1) != nullptr);
      }
    }

    ASSERT_TRUE(flag);
    ASSERT_TRUE(threw);
  }

  TEST_F(TFdManagedThreadTest, Test4) {
    bool flag = false;
    TTestWorker3 worker(flag, false);
    worker.Start();
    ASSERT_FALSE(flag);
    worker.RequestShutdown();
    bool threw = false;

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    try {
      worker.Join();
    } catch (const TFdManagedThread::TWorkerError &x) {
      try {
        std::rethrow_exception(x.ThrownException);
      } catch (const char *y) {
        threw = true;
        ASSERT_EQ(std::strcmp(y, BLURB_2), 0);
      }
    }

    ASSERT_TRUE(flag);
    ASSERT_TRUE(threw);

    flag = false;
    worker.Start();
    ASSERT_FALSE(flag);
    worker.RequestShutdown();
    threw = false;

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    try {
      worker.Join();
    } catch (const TFdManagedThread::TWorkerError &x) {
      try {
        std::rethrow_exception(x.ThrownException);
      } catch (const char *y) {
        threw = true;
        ASSERT_EQ(std::strcmp(y, BLURB_2), 0);
      }
    }

    ASSERT_TRUE(flag);
    ASSERT_TRUE(threw);
  }

  TEST_F(TFdManagedThreadTest, Test5) {
    bool flag = false;
    TTestWorker1 worker(flag);
    ASSERT_FALSE(flag);
    worker.Start();
    bool threw = false;

    try {
      worker.Start();
    } catch (const std::logic_error &) {
      threw = true;  // worker already started
    }

    ASSERT_TRUE(threw);
    threw = false;

    if (!worker.GetShutdownWaitFd().IsReadable(30000)) {
      ASSERT_TRUE(false);
      return;
    }

    worker.Join();

    try {
      worker.Join();
    } catch (const std::logic_error &) {
      threw = true;  // cannot join nonexistent thread
    }

    ASSERT_TRUE(threw);
    threw = false;

    try {
      worker.RequestShutdown();
    } catch (const std::logic_error &) {
      threw = true;  // worker already shut down
    }

    ASSERT_TRUE(threw);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
