/* <base/bits.test.cc>

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

   Unit test for <base/bits.h>.
 */

#include <base/bits.h>

#include <cstdint>

#include <gtest/gtest.h>

using namespace Base;

namespace {

  /* The fixture for testing class TBits. */
  class TBitsTest : public ::testing::Test {
    protected:
    TBitsTest() = default;

    ~TBitsTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TBitsTest

  TEST_F(TBitsTest, Simple) {
    ASSERT_EQ(TBits<int8_t>::Value(), 8U);
    ASSERT_EQ(TBits<uint8_t>::Value(), 8U);
    ASSERT_EQ(TBits<int16_t>::Value(), 16U);
    ASSERT_EQ(TBits<uint16_t>::Value(), 16U);
    ASSERT_EQ(TBits<int32_t>::Value(), 32U);
    ASSERT_EQ(TBits<uint32_t>::Value(), 32U);
    ASSERT_EQ(TBits<int64_t>::Value(), 64U);
    ASSERT_EQ(TBits<uint64_t>::Value(), 64U);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
