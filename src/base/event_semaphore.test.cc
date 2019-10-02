/* <base/event_semaphore.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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
 
   Unit test for <base/event_semaphore.h>.
 */

#include <base/event_semaphore.h>
  
#include <fcntl.h>
#include <unistd.h>
 
#include <base/error_util.h>
#include <base/wr/fd_util.h>
  
#include <gtest/gtest.h>
  
using namespace Base;

static void SetCloseOnExec(int fd) noexcept {
  const int flags = Wr::fcntl(fd, F_GETFD, 0);
  assert(flags >= 0);
  const int ret = Wr::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  assert(ret >= 0);
}

namespace {

  /* The fixture for testing class TEventSemaphore. */
  class TEventSemaphoreTest : public ::testing::Test {
    protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    TEventSemaphoreTest() = default;

    ~TEventSemaphoreTest() override = default;

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    void SetUp() override {
      // Code here will be called immediately after the constructor (right
      // before each test).
    }

    void TearDown() override {
      // Code here will be called immediately after each test (right
      // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Foo.
  };  // TEventSemaphoreTest

  TEST_F(TEventSemaphoreTest, Typical) {
    static const uint64_t actual_count = 3;
    TEventSemaphore sem;
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    sem.Push(actual_count);

    for (uint64_t i = 0; i < actual_count; ++i) {
      ASSERT_TRUE(sem.GetFd().IsReadableIntr());
      sem.Pop();
    }

    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
  }
  
  TEST_F(TEventSemaphoreTest, Nonblocking) {
    static const uint64_t actual_count = 3;
    TEventSemaphore sem(0, true);
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    ASSERT_FALSE(sem.Pop());
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    sem.Push(actual_count);

    for (uint64_t i = 0; i < actual_count; ++i) {
      ASSERT_TRUE(sem.GetFd().IsReadableIntr());
      ASSERT_TRUE(sem.Pop());
    }

    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    ASSERT_FALSE(sem.Pop());
  }
  
  TEST_F(TEventSemaphoreTest, Reset) {
    TEventSemaphore sem;
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    int old_fd = sem.GetFd();
    sem.Reset(1);
    int new_fd = sem.GetFd();
    ASSERT_EQ(new_fd, old_fd);
    ASSERT_TRUE(sem.GetFd().IsReadableIntr());
    int flags = fcntl(sem.GetFd(), F_GETFD, 0);
    ASSERT_EQ(flags & FD_CLOEXEC, 0);
    SetCloseOnExec(sem.GetFd());
    flags = fcntl(sem.GetFd(), F_GETFD, 0);
    ASSERT_NE(flags & FD_CLOEXEC, 0);
    old_fd = sem.GetFd();
    sem.Reset(0);
    new_fd = sem.GetFd();
    ASSERT_EQ(new_fd, old_fd);
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    flags = fcntl(sem.GetFd(), F_GETFD, 0);
    ASSERT_NE(flags & FD_CLOEXEC, 0);
  }
  
  TEST_F(TEventSemaphoreTest, ResetNonblocking) {
    TEventSemaphore sem(0, true);
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    ASSERT_FALSE(sem.Pop());
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    int old_fd = sem.GetFd();
    sem.Reset(1);
    int new_fd = sem.GetFd();
    ASSERT_EQ(new_fd, old_fd);
    ASSERT_TRUE(sem.GetFd().IsReadableIntr());
    int flags = fcntl(sem.GetFd(), F_GETFD, 0);
    ASSERT_EQ(flags & FD_CLOEXEC, 0);
    SetCloseOnExec(sem.GetFd());
    flags = fcntl(sem.GetFd(), F_GETFD, 0);
    ASSERT_NE(flags & FD_CLOEXEC, 0);
    old_fd = sem.GetFd();
    sem.Reset(0);
    new_fd = sem.GetFd();
    ASSERT_EQ(new_fd, old_fd);
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    ASSERT_FALSE(sem.Pop());
    ASSERT_FALSE(sem.GetFd().IsReadableIntr());
    flags = fcntl(sem.GetFd(), F_GETFD, 0);
    ASSERT_NE(flags & FD_CLOEXEC, 0);
    sem.Push();
    ASSERT_TRUE(sem.Pop());
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
