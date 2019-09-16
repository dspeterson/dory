/* <socket/address.test.cc>
 
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
 
   Unit test for <socket/address.h>.
 */

#include <socket/address.h>

#include <sstream>

#include <log_util/init_logging.h>

#include <gtest/gtest.h>

using namespace LogUtil;
using namespace Socket;

namespace {

  /* The fixture for testing class TAddress. */
  class TAddressTest : public ::testing::Test {
    protected:
    TAddressTest() = default;

    ~TAddressTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TAddressTest

  TEST_F(TAddressTest, Specials) {
    ASSERT_EQ(TAddress(TAddress::IPv4Any),
        TAddress(std::istringstream("0.0.0.0")));
    ASSERT_EQ(TAddress(TAddress::IPv4Loopback),
        TAddress(std::istringstream("127.0.0.1")));
    ASSERT_EQ(TAddress(TAddress::IPv6Any),
        TAddress(std::istringstream("[::]")));
    ASSERT_EQ(TAddress(TAddress::IPv6Loopback),
        TAddress(std::istringstream("[::1]")));
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
