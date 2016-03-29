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
    TTmpFileTest() {
    }

    virtual ~TTmpFileTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TTmpFileTest

  TEST_F(TTmpFileTest, Test1) {
    std::string tmpl("/tmp/dory_tmp.XXXXXX");
    TTmpFile tmp_file(tmpl.c_str(), true);
    std::string name(tmp_file.GetName());
    ASSERT_EQ(tmpl.size(), name.size());
    ASSERT_NE(tmpl, name);
    const char tmpl_prefix[] = "/tmp/dory_tmp.";
    size_t tmpl_prefix_len = std::strlen(tmpl_prefix);
    ASSERT_EQ(name.substr(0, tmpl_prefix_len), std::string(tmpl_prefix));
    ASSERT_NE(name.substr(tmpl_prefix_len, 6), std::string("XXXXXX"));
  }
  
  TEST_F(TTmpFileTest, Test2) {
    std::string tmpl("XXXXXXburp");
    TTmpFile tmp_file(tmpl.c_str(), true);
    std::string name(tmp_file.GetName());
    ASSERT_EQ(tmpl.size(), name.size());
    ASSERT_NE(tmpl, name);
    ASSERT_NE(name.substr(0, 6), std::string("XXXXXX"));
    ASSERT_EQ(name.substr(6, 4), std::string("burp"));
  }
  
  TEST_F(TTmpFileTest, Test3) {
    std::string tmpl("burpXXXXXX");
    TTmpFile tmp_file(tmpl.c_str(), true);
    std::string name(tmp_file.GetName());
    ASSERT_EQ(tmpl.size(), name.size());
    ASSERT_NE(tmpl, name);
    ASSERT_EQ(name.substr(0, 4), std::string("burp"));
    ASSERT_NE(name.substr(4, 6), std::string("XXXXXX"));
  }
  
  TEST_F(TTmpFileTest, Test4) {
    std::string tmpl("burpXXXXXXyXXXXXbarf");
    TTmpFile tmp_file(tmpl.c_str(), true);
    std::string name(tmp_file.GetName());
    ASSERT_EQ(tmpl.size(), name.size());
    ASSERT_NE(tmpl, name);
    ASSERT_EQ(name.substr(0, 4), std::string("burp"));
    ASSERT_NE(name.substr(4, 6), std::string("XXXXXX"));
    ASSERT_EQ(name.substr(10, 10), std::string("yXXXXXbarf"));
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
