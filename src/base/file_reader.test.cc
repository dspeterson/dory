/* <base/file_reader.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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
 
   Unit test for <base/file_reader.h>.
 */

#include <base/file_reader.h>

#include <cstring>
#include <fstream>
#include <string>

#include <base/tmp_file.h> 
#include <gtest/gtest.h>

using namespace Base;

namespace {

  class TFileReaderSimpleTest : public ::testing::Test {
    protected:
    TFileReaderSimpleTest() {
    }

    virtual ~TFileReaderSimpleTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TFileReaderSimpleTest

  /* The fixture for testing class TFileReader. */
  class TFileReaderTest : public ::testing::Test {
    protected:
    TFileReaderTest()
        : FileContents("a bunch of junk"),
          TmpFile("/tmp/file_reader_test.XXXXXX",
              true /* delete_on_destroy */),
          Reader(TmpFile.GetName().c_str()) {
    }

    virtual ~TFileReaderTest() {
    }

    virtual void SetUp() {
      std::ofstream f(TmpFile.GetName().c_str(), std::ios_base::out);
      f << FileContents;
    }

    virtual void TearDown() {
    }

    std::string FileContents;

    TTmpFile TmpFile;

    TFileReader Reader;
  };  // TFileReaderTest

  TEST_F(TFileReaderSimpleTest, TestNoSuchFile) {
    TFileReader reader("/blah/this_file_should_not_exist");
    bool threw = false;

    try {
      reader.GetSize();
    } catch (const std::ios_base::failure &x) {
      threw = true;
    }

    ASSERT_TRUE(threw);
    threw = false;
    char buf[16];

    try {
      reader.ReadIntoBuf(buf, sizeof(buf));
    } catch (const std::ios_base::failure &x) {
      threw = true;
    }

    ASSERT_TRUE(threw);
    threw = false;
    std::string s;

    try {
      reader.ReadIntoString(s);
    } catch (const std::ios_base::failure &x) {
      threw = true;
    }

    ASSERT_TRUE(threw);
    threw = false;

    try {
      reader.ReadIntoString();
    } catch (const std::ios_base::failure &x) {
      threw = true;
    }

    ASSERT_TRUE(threw);
    threw = false;
    std::vector<char> v;

    try {
      reader.ReadIntoBuf(v);
    } catch (const std::ios_base::failure &x) {
      threw = true;
    }

    ASSERT_TRUE(threw);
    threw = false;

    try {
      reader.ReadIntoBuf<char>();
    } catch (const std::ios_base::failure &x) {
      threw = true;
    }

    ASSERT_TRUE(threw);
  }

  TEST_F(TFileReaderTest, TestGetSize) {
    ASSERT_EQ(Reader.GetSize(), FileContents.size());

    /* Make sure a second GetSize() operation with the same reader works. */
    ASSERT_EQ(Reader.GetSize(), FileContents.size());
  }

  TEST_F(TFileReaderTest, TestCallerSuppliedBuf) {
    char buf[2 * FileContents.size()];
    std::fill(buf, buf + sizeof(buf), 'x');
    size_t byte_count = Reader.ReadIntoBuf(buf, sizeof(buf));
    ASSERT_EQ(byte_count, FileContents.size());
    ASSERT_EQ(std::memcmp(buf, "a bunch of junkxxxxxxxxxxxxxxx", sizeof(buf)),
        0);
    std::fill(buf, buf + sizeof(buf), 'x');
    byte_count = Reader.ReadIntoBuf(buf, 7);
    ASSERT_EQ(byte_count, 7U);
    ASSERT_EQ(std::memcmp(buf, "a bunchxxxxxxxxxxxxxxxxxxxxxxx", sizeof(buf)),
        0);
  }

  TEST_F(TFileReaderTest, TestCallerSuppliedBuf2) {
    char buf[2 * FileContents.size()];
    std::fill(buf, buf + sizeof(buf), 'x');
    size_t byte_count = ReadFileIntoBuf(TmpFile.GetName(), buf, sizeof(buf));
    ASSERT_EQ(byte_count, FileContents.size());
    ASSERT_EQ(std::memcmp(buf, "a bunch of junkxxxxxxxxxxxxxxx", sizeof(buf)),
        0);
    std::fill(buf, buf + sizeof(buf), 'x');
    byte_count = ReadFileIntoBuf(TmpFile.GetName(), buf, 7);
    ASSERT_EQ(byte_count, 7U);
    ASSERT_EQ(std::memcmp(buf, "a bunchxxxxxxxxxxxxxxxxxxxxxxx", sizeof(buf)),
        0);
  }

  TEST_F(TFileReaderTest, TestReadIntoString) {
    std::string s;
    Reader.ReadIntoString(s);
    ASSERT_EQ(s, FileContents);
    ASSERT_EQ(Reader.ReadIntoString(), FileContents);
  }

  TEST_F(TFileReaderTest, TestReadIntoString2) {
    std::string s;
    ReadFileIntoString(TmpFile.GetName(), s);
    ASSERT_EQ(s, FileContents);
    ASSERT_EQ(ReadFileIntoString(TmpFile.GetName()), FileContents);
  }

  TEST_F(TFileReaderTest, TestReadIntoVector) {
    std::vector<char> v1;
    Reader.ReadIntoBuf(v1);
    ASSERT_EQ(v1.size(), FileContents.size());
    ASSERT_EQ(std::memcmp(&v1[0], FileContents.data(), v1.size()), 0);

    std::vector<char> v2 = Reader.ReadIntoBuf<char>();
    ASSERT_EQ(v2, v1);
  }

  TEST_F(TFileReaderTest, TestReadIntoVector2) {
    std::vector<char> v1;
    ReadFileIntoBuf(TmpFile.GetName(), v1);
    ASSERT_EQ(v1.size(), FileContents.size());
    ASSERT_EQ(std::memcmp(&v1[0], FileContents.data(), v1.size()), 0);

    std::vector<char> v2 = ReadFileIntoBuf<char>(TmpFile.GetName());
    ASSERT_EQ(v2, v1);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
