/* <base/stl_util.test.cc>
 
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
 
   Unit test for <base/stl_util.h>.
 */

#include <base/stl_util.h>
  
#include <unordered_map>
#include <unordered_set>
  
#include <gtest/gtest.h>
  
using namespace std;
using namespace Base;

namespace {

  /* The fixture for testing STL utilities. */
  class TStlUtilsTest : public ::testing::Test {
    protected:
    TStlUtilsTest() = default;

    ~TStlUtilsTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TStlUtilsTest

  TEST_F(TStlUtilsTest, Contains) {
    unordered_set<int> container;
    container.insert(101);
    ASSERT_TRUE(Contains(container, 101));
    ASSERT_FALSE(Contains(container, 202));
  }

  TEST_F(TStlUtilsTest, RotatedLeft) {
    ASSERT_EQ(RotatedLeft<unsigned short>(0x1234, 4), 0x2341);
  }

  TEST_F(TStlUtilsTest, RotatedRight) {
    ASSERT_EQ(RotatedRight<unsigned short>(0x1234, 4), 0x4123);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
