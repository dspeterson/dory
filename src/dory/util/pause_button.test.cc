/* <dory/util/pause_button.test.cc>

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

   Unit test for <dory/util/pause_button.h>.
 */

#include <dory/util/pause_button.h>

#include <base/tmp_file.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace ::TestUtil;

namespace {

  /* The fixture for testing class TPauseButton. */
  class TPauseButtonTest : public ::testing::Test {
    protected:
    TPauseButtonTest() = default;

    ~TPauseButtonTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TPauseButtonTest

  TEST_F(TPauseButtonTest, Test1) {
    TPauseButton pause_button;
    const Base::TFd &fd = pause_button.GetFd();
    ASSERT_FALSE(fd.IsReadable());
    pause_button.Push();
    ASSERT_TRUE(fd.IsReadable());
    pause_button.Push();
    ASSERT_TRUE(fd.IsReadable());
    pause_button.Reset();
    ASSERT_FALSE(fd.IsReadable());
    pause_button.Reset();
    ASSERT_FALSE(fd.IsReadable());
    pause_button.Push();
    ASSERT_TRUE(fd.IsReadable());
    pause_button.Reset();
    ASSERT_FALSE(fd.IsReadable());
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
