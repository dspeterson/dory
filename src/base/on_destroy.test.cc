/* <base/on_destroy.test.cc>
 
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
 
   Unit test for <base/on_destroy.h>.
 */

#include <base/on_destroy.h>

#include <base/error_util.h>

#include <gtest/gtest.h>
  
using namespace Base;

namespace {

  /* The fixture for testing class TOnDestroy. */
  class TOnDestroyTest : public ::testing::Test {
    protected:
    TOnDestroyTest() = default;

    ~TOnDestroyTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TOnDestroyTest

  TEST_F(TOnDestroyTest, Test1) {
    bool called = false;

    {
      TOnDestroy on_destroy(
          [&called]() noexcept {
            called = true;
          }
      );
    }

    ASSERT_TRUE(called);
  }

  TEST_F(TOnDestroyTest, Test2) {
    bool called = false;

    {
      TOnDestroy on_destroy(
          [&called]() noexcept {
            called = true;
          }
      );

      on_destroy.Cancel();
    }

    ASSERT_FALSE(called);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
