/* <log/log_writer.test.cc>

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

   Unit test for <log/log_writer.h>.
 */

#include <log/log_writer.h>

#include <cstdio>
#include <iostream>
#include <string>

#include <unistd.h>

#include <base/error_util.h>
#include <base/on_destroy.h>
#include <base/file_reader.h>
#include <base/tmp_file.h>
#include <log/log_entry.h>
#include <log/pri.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Log;

namespace {

  /* The fixture for testing class TLogEntry. */
  class TLogWriterTest : public ::testing::Test {
    protected:
    TLogWriterTest() {
    }

    virtual ~TLogWriterTest() = default;

    virtual void SetUp() {
      /* Destroy any log writer left behind by a prior test. */
      DropLogWriter();
    }

    virtual void TearDown() {
    }
  };  // TLogWriterTest

  TEST_F(TLogWriterTest, LogRotate) {
    SetLogMask(UpTo(TPri::INFO));
    const std::string tmp_filename_1 = MakeTmpFilename(
        "/tmp/log_writer_test.XXXXXX");
    auto file_1_deleter = OnDestroy(
        [&tmp_filename_1]() noexcept {
          unlink(tmp_filename_1.c_str());
        });

    /* Tell logging subsystem to open logfile tmp_filename_1. */
    SetLogWriter(false /* enable_stdout_stderr */, false /* enable_syslog */,
        tmp_filename_1 /* file_path */, TOpt<mode_t>(0644) /* file_mode */);

    const std::string tmp_filename_2 = MakeTmpFilename(
        "/tmp/log_writer_test.XXXXXX");
    auto file_2_deleter = OnDestroy(
        [&tmp_filename_2]() noexcept {
          unlink(tmp_filename_2.c_str());
        });

    std::shared_ptr<TLogWriterBase> old_writer = GetLogWriter();
    const std::string line_1 = "line 1";
    const std::string line_2 = "line 2";
    const std::string line_3 = "line 3";

    /* Write line_1 into tmp_filename_1 (with trailing newline). */
    TLogEntry<64, 0>(std::shared_ptr<TLogWriterBase>(old_writer), TPri::INFO)
        << line_1;

    /* After the rename, writes to old_writer will go to tmp_filename_2, since
       old_writer keeps the file descriptor open. */
    const int ret = std::rename(tmp_filename_1.c_str(), tmp_filename_2.c_str());
    ASSERT_EQ(ret, 0);

    /* Tell logging subsystem to reopen tmp_filename_1.  This recreates the
       file that was renamed above. */
    const bool reopened = HandleLogfileReopenRequest();
    ASSERT_TRUE(reopened);

    /* Writes to new_writer will go to recreated tmp_filename_1. */
    std::shared_ptr<TLogWriterBase> new_writer = GetLogWriter();

    /* Write line_2 (with trailing newline) to tmp_filename_2. */
    TLogEntry<64, 0>(std::shared_ptr<TLogWriterBase>(old_writer), TPri::INFO)
        << line_2;

    /* Write line_3 (with trailing newline) to recreated tmp_filename_1. */
    TLogEntry<64, 0>(std::shared_ptr<TLogWriterBase>(new_writer), TPri::INFO)
        << line_3;

    const std::string file_1_contents = ReadFileIntoString(tmp_filename_1);
    const std::string file_2_contents = ReadFileIntoString(tmp_filename_2);
    ASSERT_EQ(file_1_contents, line_3 + "\n");
    ASSERT_EQ(file_2_contents, line_1 + "\n" + line_2 + "\n");
  }

  TEST_F(TLogWriterTest, NoLogRotate) {
    SetLogMask(UpTo(TPri::INFO));

    /* This should be a no-op and return false, since log writer has not yet
       been created. */
    bool reopened = HandleLogfileReopenRequest();
    ASSERT_FALSE(reopened);

    /* Create log writer with file logging disabled. */
    SetLogWriter(false /* enable_stdout_stderr */, false /* enable_syslog */,
        std::string() /* file_path */, TOpt<mode_t>() /* file_mode */);

    /* This should be a no-op and return false, since log writer has been
       created but file logging is disabled. */
    reopened = HandleLogfileReopenRequest();
    ASSERT_FALSE(reopened);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
