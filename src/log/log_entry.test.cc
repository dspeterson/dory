/* <log/log_entry.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2017-2019 Dave Peterson <dave@dspeterson.com>

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

#include <cstddef>
#include <memory>
#include <string>

#include <base/error_util.h>
#include <log/log_entry_access_api.h>
#include <log/log_writer_base.h>
#include <log/pri.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Log;

namespace {

  std::string TestPrefix;

  void TestPrefixWriter(TLogPrefixAssignApi &entry) noexcept {
    entry.AssignPrefix(TestPrefix.c_str(), TestPrefix.size());
  }

  class TTestLogWriter : public TLogWriterBase {
    public:
    TTestLogWriter() = default;

    size_t GetWrittenCount() const noexcept {
      return WrittenCount;
    }

    const std::string &GetEntry() const noexcept {
      return Entry;
    }

    const std::string &GetEntryWithNewline() const noexcept {
      return EntryWithNewline;
    }

    const std::string &GetEntryWithPrefix() const noexcept {
      return EntryWithPrefix;
    };

    const std::string &GetEntryWithPrefixAndNewline() const noexcept {
      return EntryWithPrefixAndNewline;
    }

    void WriteEntry(TLogEntryAccessApi &entry,
        bool /* no_stdout_stderr */) const noexcept override {
      Entry = entry.Get(false /* with_prefix */,
          false /* with_trailing_newline */).first;
      EntryWithNewline = entry.Get(false /* with_prefix */,
          true /* with_trailing_newline */).first;
      EntryWithPrefix = entry.Get(true /* with_prefix */,
          false /* with_newline */).first;
      EntryWithPrefixAndNewline = entry.Get(true /* with_prefix */,
          true /* with_newline */).first;
      ++WrittenCount;
    }

    void WriteStackTrace(TPri /* pri */, void *const * /* buffer */,
        size_t /* size */,
        bool /* no_stdout_stderr */) const noexcept override {
      ASSERT_TRUE(false);
    }

    private:
    mutable size_t WrittenCount = 0;

    mutable std::string Entry;

    mutable std::string EntryWithNewline;

    mutable std::string EntryWithPrefix;

    mutable std::string EntryWithPrefixAndNewline;
  };  // TTestLogWriter

  /* The fixture for testing class TLogEntry. */
  class TLogEntryTest : public ::testing::Test {
    protected:
    TLogEntryTest() {
    }

    virtual ~TLogEntryTest() = default;

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TLogEntryTest

  TEST_F(TLogEntryTest, BasicTest) {
    TestPrefix = "prefix";
    SetPrefixWriter(TestPrefixWriter);
    std::shared_ptr<TTestLogWriter> writer =
        std::make_shared<TTestLogWriter>();
    std::string hello("hello world");

    {
      TLogEntry<512, 0> entry(std::shared_ptr<TLogWriterBase>(writer),
          TPri::INFO);
      ASSERT_EQ(entry.GetLevel(), TPri::INFO);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 0U);
      ASSERT_EQ(writer->GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
      entry.Write();
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), hello);
      entry.Write();
      ASSERT_EQ(entry.PrefixSize(), 0U);
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), hello);
      ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
      ASSERT_EQ(writer->GetEntryWithPrefix(), hello);
      ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(), hello + "\n");
    }

    ASSERT_EQ(writer->GetWrittenCount(), 1U);
    ASSERT_EQ(writer->GetEntry(), hello);
    ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
    ASSERT_EQ(writer->GetEntryWithPrefix(), hello);
    ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(), hello + "\n");
  }

  TEST_F(TLogEntryTest, WriteOnDestroy) {
    TestPrefix = "prefix";
    SetPrefixWriter(TestPrefixWriter);
    std::shared_ptr<TTestLogWriter> writer =
        std::make_shared<TTestLogWriter>();
    std::string hello("hello world");

    {
      TLogEntry<512, 0> entry(std::shared_ptr<TLogWriterBase>(writer),
          TPri::INFO);
      ASSERT_EQ(entry.GetLevel(), TPri::INFO);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 0U);
      ASSERT_EQ(writer->GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
    }

    ASSERT_EQ(writer->GetWrittenCount(), 1U);
    ASSERT_EQ(writer->GetEntry(), hello);
    ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
    ASSERT_EQ(writer->GetEntryWithPrefix(), hello);
    ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(), hello + "\n");
  }

  TEST_F(TLogEntryTest, PrefixTest) {
    TestPrefix = "prefix";
    SetPrefixWriter(TestPrefixWriter);
    std::shared_ptr<TTestLogWriter> writer =
        std::make_shared<TTestLogWriter>();
    std::string hello("hello world");

    {
      TLogEntry<512, 16> entry(std::shared_ptr<TLogWriterBase>(writer),
          TPri::INFO);
      ASSERT_EQ(entry.GetLevel(), TPri::INFO);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 0U);
      ASSERT_EQ(writer->GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
      entry.Write();
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), hello);
      entry.Write();
      ASSERT_EQ(entry.PrefixSize(), TestPrefix.size());
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), hello);
      ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
      ASSERT_EQ(writer->GetEntryWithPrefix(), TestPrefix + hello);
      ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(),
          TestPrefix + hello + "\n");
    }

    ASSERT_EQ(writer->GetWrittenCount(), 1U);
    ASSERT_EQ(writer->GetEntry(), hello);
    ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
    ASSERT_EQ(writer->GetEntryWithPrefix(), TestPrefix + hello);
    ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(),
        TestPrefix + hello + "\n");
  }

  TEST_F(TLogEntryTest, LongPrefixTest) {
    TestPrefix = "prefix";
    std::string truncated_prefix("pref");
    SetPrefixWriter(TestPrefixWriter);
    std::shared_ptr<TTestLogWriter> writer =
        std::make_shared<TTestLogWriter>();
    std::string hello("hello world");

    {
      TLogEntry<512, 4> entry(std::shared_ptr<TLogWriterBase>(writer),
          TPri::INFO);
      ASSERT_EQ(entry.GetLevel(), TPri::INFO);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 0U);
      ASSERT_EQ(writer->GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
      entry.Write();
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), hello);
      entry.Write();
      ASSERT_EQ(entry.PrefixSize(), truncated_prefix.size());
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), hello);
      ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
      ASSERT_EQ(writer->GetEntryWithPrefix(), truncated_prefix + hello);
      ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(),
          truncated_prefix + hello + "\n");
    }

    ASSERT_EQ(writer->GetWrittenCount(), 1U);
    ASSERT_EQ(writer->GetEntry(), hello);
    ASSERT_EQ(writer->GetEntryWithNewline(), hello + "\n");
    ASSERT_EQ(writer->GetEntryWithPrefix(), truncated_prefix + hello);
    ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(),
        truncated_prefix + hello + "\n");
  }

  TEST_F(TLogEntryTest, BufferFullTest) {
    TestPrefix = "prefix";
    std::string truncated_prefix("pref");
    SetPrefixWriter(TestPrefixWriter);
    std::shared_ptr<TTestLogWriter> writer =
        std::make_shared<TTestLogWriter>();
    std::string hello("hello world");
    std::string truncated_hello("hello worl");

    {
      TLogEntry<16, 4> entry(std::shared_ptr<TLogWriterBase>(writer),
          TPri::INFO);
      ASSERT_EQ(entry.GetLevel(), TPri::INFO);
      ASSERT_FALSE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 0U);
      ASSERT_EQ(writer->GetEntry(), "");
      entry << hello;
      entry << 5;
      hello += "5";
      entry.Write();
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), truncated_hello);
      entry.Write();
      ASSERT_EQ(entry.PrefixSize(), truncated_prefix.size());
      ASSERT_TRUE(entry.IsWritten());
      ASSERT_EQ(writer->GetWrittenCount(), 1U);
      ASSERT_EQ(writer->GetEntry(), truncated_hello);
      ASSERT_EQ(writer->GetEntryWithNewline(), truncated_hello + "\n");
      ASSERT_EQ(writer->GetEntryWithPrefix(),
          truncated_prefix + truncated_hello);
      ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(),
          truncated_prefix + truncated_hello + "\n");
    }

    ASSERT_EQ(writer->GetWrittenCount(), 1U);
    ASSERT_EQ(writer->GetEntry(), truncated_hello);
    ASSERT_EQ(writer->GetEntryWithNewline(), truncated_hello + "\n");
    ASSERT_EQ(writer->GetEntryWithPrefix(),
        truncated_prefix + truncated_hello);
    ASSERT_EQ(writer->GetEntryWithPrefixAndNewline(),
        truncated_prefix + truncated_hello + "\n");
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
