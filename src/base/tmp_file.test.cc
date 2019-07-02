/* <base/tmp_file.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2013 if(we)

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
 
   Unit test for <base/tmp_file.h>.
 */
  
  
#include <base/tmp_file.h>
 
#include <cstddef>
#include <cstring>
 
#include <gtest/gtest.h>
  
using namespace Base;

namespace {

  /* The fixture for testing class TTmpFile. */
  class TTmpFileTest : public ::testing::Test {
    protected:
    TTmpFileTest() = default;

    ~TTmpFileTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TTmpFileTest

  TEST_F(TTmpFileTest, Test1) {
    std::string tmpl("/tmp/tmp_file_test.XXXXXX");
    TTmpFile tmp_file(tmpl.c_str(), true);
    ASSERT_EQ(tmpl.size(), tmp_file.GetName().size());
    ASSERT_NE(tmpl, tmp_file.GetName());
    const char tmpl_prefix[] = "/tmp/tmp_file_test.";
    size_t tmpl_prefix_len = std::strlen(tmpl_prefix);
    ASSERT_EQ(tmp_file.GetName().substr(0, tmpl_prefix_len),
        std::string(tmpl_prefix));
    ASSERT_NE(tmp_file.GetName().substr(tmpl_prefix_len, 6),
        std::string("XXXXXX"));
  }
  
  TEST_F(TTmpFileTest, Test2) {
    std::string tmpl("XXXXXXburp");
    TTmpFile tmp_file(tmpl.c_str(), true);
    ASSERT_EQ(tmpl.size(), tmp_file.GetName().size());
    ASSERT_NE(tmpl, tmp_file.GetName());
    ASSERT_NE(tmp_file.GetName().substr(0, 6), std::string("XXXXXX"));
    ASSERT_EQ(tmp_file.GetName().substr(6, 4), std::string("burp"));
  }
  
  TEST_F(TTmpFileTest, Test3) {
    std::string tmpl("burpXXXXXX");
    TTmpFile tmp_file(tmpl.c_str(), true);
    ASSERT_EQ(tmpl.size(), tmp_file.GetName().size());
    ASSERT_NE(tmpl, tmp_file.GetName());
    ASSERT_EQ(tmp_file.GetName().substr(0, 4), std::string("burp"));
    ASSERT_NE(tmp_file.GetName().substr(4, 6), std::string("XXXXXX"));
  }
  
  TEST_F(TTmpFileTest, Test4) {
    std::string tmpl("burpXXXXXXyXXXXXbarf");
    TTmpFile tmp_file(tmpl.c_str(), true);
    ASSERT_EQ(tmpl.size(), tmp_file.GetName().size());
    ASSERT_NE(tmpl, tmp_file.GetName());
    ASSERT_EQ(tmp_file.GetName().substr(0, 4), std::string("burp"));
    ASSERT_NE(tmp_file.GetName().substr(4, 6), std::string("XXXXXX"));
    ASSERT_EQ(tmp_file.GetName().substr(10, 10), std::string("yXXXXXbarf"));
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
