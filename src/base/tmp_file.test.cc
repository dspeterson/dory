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
 
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <utility>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtest/gtest.h>
  
using namespace Base;

namespace {

  bool FileExists(const std::string &name) {
    struct stat buf;
    int ret = stat(name.c_str(), &buf);
    const bool exists = (ret == 0);
    assert(exists || (errno == ENOENT));
    assert(!exists || S_ISREG(buf.st_mode));
    return exists;
  }

  ino_t GetInodeNumber(int fd) {
    struct stat buf;
    int ret = fstat(fd, &buf);
    assert(ret == 0);
    return buf.st_ino;
  }

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

  TEST_F(TTmpFileTest, Reset) {
    TTmpFile f1("/tmp/tmp_file_test.XXXXXX", true);
    const std::string f1_name = f1.GetName();
    ASSERT_TRUE(FileExists(f1_name));
    f1.Reset();
    ASSERT_TRUE(f1.GetName().empty());
    ASSERT_TRUE(f1.GetDeleteOnDestroy());
    ASSERT_FALSE(f1.GetFd().IsOpen());
    ASSERT_FALSE(FileExists(f1_name));
  }

  TEST_F(TTmpFileTest, MoveConstruct) {
    TTmpFile f1("/tmp/tmp_file_test.XXXXXX", true);
    const std::string f1_name = f1.GetName();
    ASSERT_TRUE(FileExists(f1_name));

    {
      TTmpFile f2(std::move(f1));
      ASSERT_TRUE(FileExists(f1_name));
      ASSERT_TRUE(f1.GetName().empty());
      ASSERT_TRUE(f1.GetDeleteOnDestroy());
      ASSERT_FALSE(f1.GetFd().IsOpen());
      ASSERT_EQ(f2.GetName(), f1_name);
      ASSERT_TRUE(f2.GetDeleteOnDestroy());
      ASSERT_TRUE(f2.GetFd().IsOpen());
    }

    ASSERT_FALSE(FileExists(f1_name));
  }

  TEST_F(TTmpFileTest, MoveAssign) {
    TTmpFile f1("/tmp/tmp_file_test.XXXXXX", true);
    const std::string f1_name = f1.GetName();
    ASSERT_TRUE(FileExists(f1_name));
    const int f1_fd = f1.GetFd();
    const ino_t f1_inode = GetInodeNumber(f1.GetFd());

    {
      TTmpFile f2("/tmp/tmp_file_test.XXXXXX", true);
      const std::string f2_name = f2.GetName();
      ASSERT_TRUE(FileExists(f2_name));

      f2 = std::move(f1);
      ASSERT_FALSE(FileExists(f2_name));
      ASSERT_TRUE(FileExists(f1_name));
      ASSERT_TRUE(f1.GetName().empty());
      ASSERT_EQ(f2.GetName(), f1_name);
      ASSERT_FALSE(f1.GetFd().IsOpen());
      ASSERT_TRUE(f2.GetFd().IsOpen());
      ASSERT_EQ(f2.GetFd(), f1_fd);
      ASSERT_EQ(GetInodeNumber(f2.GetFd()), f1_inode);
    }

    ASSERT_FALSE(FileExists(f1_name));
  }

  TEST_F(TTmpFileTest, Swap) {
    std::string f1_name;
    std::string f2_name;

    {
      TTmpFile f1("/tmp/tmp_file_test.XXXXXX", true);
      f1_name = f1.GetName();
      ASSERT_TRUE(FileExists(f1_name));
      const int f1_fd = f1.GetFd();
      const ino_t f1_inode = GetInodeNumber(f1.GetFd());

      {
        TTmpFile f2("/tmp/tmp_file_test.XXXXXX", true);
        f2_name = f2.GetName();
        ASSERT_TRUE(FileExists(f2_name));
        const int f2_fd = f2.GetFd();
        const ino_t f2_inode = GetInodeNumber(f2.GetFd());

        f2.Swap(f1);
        ASSERT_TRUE(FileExists(f1_name));
        ASSERT_TRUE(FileExists(f2_name));
        ASSERT_EQ(f1.GetName(), f2_name);
        ASSERT_EQ(f2.GetName(), f1_name);
        ASSERT_TRUE(f1.GetFd().IsOpen());
        ASSERT_TRUE(f2.GetFd().IsOpen());
        ASSERT_EQ(f1.GetFd(), f2_fd);
        ASSERT_EQ(f2.GetFd(), f1_fd);
        ASSERT_EQ(GetInodeNumber(f1.GetFd()), f2_inode);
        ASSERT_EQ(GetInodeNumber(f2.GetFd()), f1_inode);
      }

      ASSERT_FALSE(FileExists(f1_name));
      ASSERT_TRUE(FileExists(f2_name));
    }

    ASSERT_FALSE(FileExists(f2_name));
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
