/* <thread/gate.test.cc>

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

   Unit test for <thread/gate.h>
 */

#include <thread/gate.h>

#include <string>

#include <base/tmp_file.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace TestUtil;
using namespace Thread;

namespace {

  /* The fixture for testing class TGate. */
  class TGateTest : public ::testing::Test {
    protected:
    TGateTest() = default;

    ~TGateTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TGateTest

  TEST_F(TGateTest, Test1) {
    TGate<std::string> g;
    const Base::TFd &fd = g.GetMsgAvailableFd();
    std::list<std::string> list_1;
    ASSERT_FALSE(fd.IsReadableIntr());
    g.Put(std::move(list_1));
    ASSERT_FALSE(fd.IsReadableIntr());
    list_1 = g.NonblockingGet();
    ASSERT_TRUE(list_1.empty());
    ASSERT_FALSE(fd.IsReadableIntr());

    list_1.emplace_back("msg1");
    list_1.emplace_back("msg2");
    std::list<std::string> list_2(list_1);
    g.Put(std::move(list_1));
    ASSERT_TRUE(list_1.empty());
    ASSERT_TRUE(fd.IsReadableIntr());
    list_1.emplace_back("msg3");
    list_1.emplace_back("msg4");
    list_2.emplace_back("msg3");
    list_2.emplace_back("msg4");
    g.Put(std::move(list_1));
    ASSERT_TRUE(list_1.empty());
    ASSERT_TRUE(fd.IsReadableIntr());
    list_1 = g.Get();
    ASSERT_TRUE(list_1 == list_2);
    ASSERT_FALSE(fd.IsReadableIntr());

    list_1.clear();
    list_1.emplace_back("msg5");
    list_1.emplace_back("msg6");
    list_2 = list_1;
    g.Put(std::move(list_1));
    ASSERT_TRUE(list_1.empty());
    ASSERT_TRUE(fd.IsReadableIntr());
    list_1 = g.NonblockingGet();
    ASSERT_TRUE(fd.IsReadableIntr());
    ASSERT_TRUE(list_1 == list_2);
    list_1 = g.Get();
    ASSERT_FALSE(fd.IsReadableIntr());
    ASSERT_TRUE(list_1.empty());

    std::string s("msg7");
    list_1.push_back(s);
    g.Put(std::move(s));
    ASSERT_TRUE(s.empty());
    ASSERT_TRUE(fd.IsReadableIntr());
    s = "msg8";
    list_1.push_back(s);
    g.Put(std::move(s));
    ASSERT_TRUE(s.empty());
    ASSERT_TRUE(fd.IsReadableIntr());
    list_2 = g.Get();
    ASSERT_FALSE(fd.IsReadableIntr());
    ASSERT_TRUE(list_2 == list_1);
    list_2 = g.NonblockingGet();
    ASSERT_TRUE(list_2.empty());
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
