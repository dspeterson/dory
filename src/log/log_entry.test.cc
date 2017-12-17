/* <log/log_entry.test.cc>
 
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
 
   Unit test for <log/log_entry.h>.
 */

#include <log/log_entry.h>

#include <cassert>
#include <cstddef>
#include <string>

#include <syslog.h>

#include <gtest/gtest.h>

using namespace Log;
 
namespace {

  class TTestLogWriter : public TLogWriterApi {
    NO_COPY_SEMANTICS(TTestLogWriter);

    public:
    TTestLogWriter()
        : WrittenCount(0) {
    }

    virtual ~TTestLogWriter() noexcept = default;

    virtual void WriteEntry(TLogEntryAccessApi &entry) {
      assert(this);
      Entry = entry.GetWithoutTrailingNewline().first;
      EntryWithNewline = entry.GetWithTrailingNewline().first;
      ++WrittenCount;
    }

    size_t GetWrittenCount() const noexcept {
      assert(this);
      return WrittenCount;
    }

    const std::string &GetEntry() const noexcept {
      assert(this);
      return Entry;
    }

    const std::string &GetEntryWithNewline() const noexcept {
      assert(this);
      return EntryWithNewline;
    }

    private:
    size_t WrittenCount;

    std::string Entry;

    std::string EntryWithNewline;
  };  // TTestLogWriter

  /* The fixture for testing class TIndent. */
  class TLogEntryTest : public ::testing::Test {
    protected:
    TLogEntryTest() {
    }

    virtual ~TLogEntryTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TLogEntryTest

  TEST_F(TLogEntryTest, Test1) {
    TTestLogWriter writer;
    std::string hello("hello world");

    {
      TLogEntry entry(writer, LOG_INFO);
      ASSERT_EQ(entry.GetLevel(), LOG_INFO);
      entry.SetLevel(LOG_WARNING);
      ASSERT_EQ(entry.GetLevel(), LOG_WARNING);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer.GetWrittenCount(), 0U);
      ASSERT_EQ(writer.GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
      entry.Write();
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer.GetWrittenCount(), 1U);
      ASSERT_EQ(writer.GetEntry(), hello);
      entry.Write();
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer.GetWrittenCount(), 1U);
      ASSERT_EQ(writer.GetEntry(), hello);
      ASSERT_EQ(writer.GetEntryWithNewline(), hello + "\n");
    }

    ASSERT_EQ(writer.GetWrittenCount(), 1U);
    ASSERT_EQ(writer.GetEntry(), hello);
    ASSERT_EQ(writer.GetEntryWithNewline(), hello + "\n");
  }

  TEST_F(TLogEntryTest, Test2) {
    TTestLogWriter writer;
    std::string hello("hello world");

    {
      TLogEntry entry(writer, LOG_INFO);
      ASSERT_EQ(entry.GetLevel(), LOG_INFO);
      entry.SetLevel(LOG_WARNING);
      ASSERT_EQ(entry.GetLevel(), LOG_WARNING);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer.GetWrittenCount(), 0U);
      ASSERT_EQ(writer.GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
    }

    ASSERT_EQ(writer.GetWrittenCount(), 1U);
    ASSERT_EQ(writer.GetEntry(), hello);
    ASSERT_EQ(writer.GetEntryWithNewline(), hello + "\n");
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
