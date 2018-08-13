/* <base/event_semaphore_notifier.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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
 
   Unit test for <base/event_semaphore_notifier.h>.
 */

#include <base/event_semaphore_notifier.h>

#include <cstdlib>
#include <iostream>
  
#include <base/event_semaphore.h>
  
#include <gtest/gtest.h>
  
using namespace Base;

namespace {

  /* The fixture for testing class TEventSemaphoreNotifier. */
  class TEventSemaphoreNotifierTest : public ::testing::Test {
    protected:
    TEventSemaphoreNotifierTest() = default;

    ~TEventSemaphoreNotifierTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TEventSemaphoreNotifierTest

  TEST_F(TEventSemaphoreNotifierTest, Test1) {
    TEventSemaphore sem;
    ASSERT_FALSE(sem.GetFd().IsReadable());

    {
      TEventSemaphoreNotifier n(sem,
          [](const std::system_error &x) {
            std::cerr << "Event semaphore push error: " << x.what()
                << std::endl;
            exit(1);
          }
      );
    }

    ASSERT_TRUE(sem.GetFd().IsReadable());
    sem.Pop();
    ASSERT_FALSE(sem.GetFd().IsReadable());
  }

  TEST_F(TEventSemaphoreNotifierTest, Test2) {
    TEventSemaphore sem;
    ASSERT_FALSE(sem.GetFd().IsReadable());

    {
      TEventSemaphoreNotifier n(sem,
          [](const std::system_error &x) {
            std::cerr << "Event semaphore push error: " << x.what()
                << std::endl;
            exit(1);
          }
      );

      n.Notify();
    }

    ASSERT_TRUE(sem.GetFd().IsReadable());
    sem.Pop();
    ASSERT_FALSE(sem.GetFd().IsReadable());
  }
  
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
