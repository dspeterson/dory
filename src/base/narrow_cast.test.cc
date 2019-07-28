/* <base/narrow_cast.test.cc>

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

   Unit test for <base/narrow_cast.h>.
 */

#include <base/narrow_cast.h>

#include <cstdint>

#include <gtest/gtest.h>

using namespace Base;

namespace {

  /* Test fixture. */
  class TNarrowCastTest : public ::testing::Test {
    protected:
    TNarrowCastTest() = default;

    ~TNarrowCastTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TNarrowCastTest

  TEST_F(TNarrowCastTest, Simple) {
    uint16_t x = 255;
    uint8_t y = 0;
    bool threw = false;

    try {
      y = narrow_cast<uint8_t>(x);
    } catch (const std::range_error &) {
      threw = true;
    }

    ASSERT_FALSE(threw);
    ASSERT_EQ(y, 255U);
    ASSERT_TRUE(CanNarrowCast<uint8_t>(x));
    x = 256;

    try {
      y = narrow_cast<uint8_t>(x);
    } catch (const std::range_error &) {
      threw = true;
    }

    ASSERT_TRUE(threw);
    ASSERT_FALSE(CanNarrowCast<uint8_t>(x));
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
