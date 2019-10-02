/* <base/sig_set.test.cc>
 
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
 
   Unit test for <base/sig_set.h>.
 */

#include <base/sig_set.h>

#include <base/tmp_file.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace TestUtil;

namespace {

  /* The fixture for testing signal sets. */
  class TSigSetTest : public ::testing::Test {
    protected:
    TSigSetTest() = default;

    ~TSigSetTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TSigSetTest

  TEST_F(TSigSetTest, Empty) {
    TSigSet a;
    ASSERT_FALSE(a[SIGINT]);
    a += SIGINT;
    ASSERT_TRUE(a[SIGINT]);
    a -= SIGINT;
    ASSERT_FALSE(a[SIGINT]);
  }

  TEST_F(TSigSetTest, Full) {
    TSigSet a(TSigSet::TListInit::Exclude, {});
    ASSERT_TRUE(a[SIGINT]);
    a -= SIGINT;
    ASSERT_FALSE(a[SIGINT]);
    a += SIGINT;
    ASSERT_TRUE(a[SIGINT]);
  }

  TEST_F(TSigSetTest, Copy) {
    TSigSet a(TSigSet::TListInit::Include, {SIGINT });
    ASSERT_TRUE(a[SIGINT]);
    TSigSet b(a);
    ASSERT_TRUE(a[SIGINT]);
    ASSERT_TRUE(b[SIGINT]);
  }

  TEST_F(TSigSetTest, Assign) {
    TSigSet a(TSigSet::TListInit::Include, {SIGINT });
    ASSERT_TRUE(a[SIGINT]);
    TSigSet b;
    ASSERT_FALSE(b[SIGINT]);
    b = a;
    ASSERT_TRUE(a[SIGINT]);
    ASSERT_TRUE(b[SIGINT]);
  }

  TEST_F(TSigSetTest, Exclude) {
    TSigSet a(TSigSet::TListInit::Exclude, {SIGINT });
    ASSERT_TRUE(a[SIGPIPE]);
    ASSERT_FALSE(a[SIGINT]);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
