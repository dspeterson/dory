/* <log/log.test.cc>

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

   Unit test for general logging functionality.
 */

#include <log/log.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h>

#include <base/error_utils.h>
#include <base/file_reader.h>
#include <base/tmp_file.h>

#include <log/log_writer.h>
#include <log/pri.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Log;

namespace {
  const char name_template[] = "/tmp/log_test.XXXXXX";

  int Foo() {
    static int value = 0;
    return ++value;
  }

  /* The fixture for testing general logging functionality. */
  class TLogTest : public ::testing::Test {
    protected:
    TLogTest() {
    }

    virtual ~TLogTest() = default;

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TLogTest

  TEST_F(TLogTest, BasicLoggingTest) {
    TTmpFile stdout_file(name_template, true /* delete_on_destroy */);
    TTmpFile stderr_file(name_template, true /* delete_on_destroy */);
    TTmpFile tmp_file(name_template, true /* delete_on_destroy */);
    int saved_stdout = IfLt0(dup(1));
    int saved_stderr = IfLt0(dup(2));
    IfLt0(dup2(stdout_file.GetFd(), 1));
    IfLt0(dup2(stderr_file.GetFd(), 2));
    SetLogMask(UpTo(TPri::NOTICE));

    SetLogWriter(false /* enable_stdout_stderr */, false /* enable_syslog */,
        tmp_file.GetName());
    std::string msg1("first message ");
    std::string msg2("second message ");
    std::string msg3("third message ");

    /* This message should get logged with Foo() returning 1. */
    LOG(TPri::NOTICE) << msg1 << Foo();

    /* Due to the log level and log mask, this message should not get logged.
       Foo() should not be called. */
    LOG(TPri::INFO) << msg2 << Foo();

    /* This message should get logged with Foo() returning 2, since Foo() was
       called only for the initial message. */
    LOG(TPri::WARNING) << msg3 << Foo();

    IfLt0(dup2(saved_stdout, 1));
    IfLt0(dup2(saved_stderr, 2));
    IfLt0(close(saved_stdout));
    IfLt0(close(saved_stderr));
    std::string stdout_contents = ReadFileIntoString(stdout_file.GetName());
    std::string stderr_contents = ReadFileIntoString(stderr_file.GetName());
    std::string file_contents = ReadFileIntoString(tmp_file.GetName());
    std::ostringstream os;
    ASSERT_EQ(stdout_contents, std::string());
    ASSERT_EQ(stderr_contents, std::string());
    os << msg1 << 1 << std::endl << msg3 << 2 << std::endl;
    ASSERT_EQ(file_contents, os.str());
  }

  TEST_F(TLogTest, StdoutStderrTest) {
    TTmpFile stdout_file(name_template, true /* delete_on_destroy */);
    TTmpFile stderr_file(name_template, true /* delete_on_destroy */);
    int saved_stdout = IfLt0(dup(1));
    int saved_stderr = IfLt0(dup(2));
    IfLt0(dup2(stdout_file.GetFd(), 1));
    IfLt0(dup2(stderr_file.GetFd(), 2));
    SetLogMask(UpTo(TPri::NOTICE));
    SetLogWriter(true /* enable_stdout_stderr */, false /* enable_syslog */,
        std::string());
    std::string msg1("should go to stdout");
    std::string msg2("should go to stderr");
    LOG(TPri::NOTICE) << msg1;
    LOG(TPri::WARNING) << msg2;
    TTmpFile tmp_file(name_template, true /* delete_on_destroy */);
    SetLogWriter(true /* enable_stdout_stderr */, false /* enable_syslog */,
        tmp_file.GetName());
    std::string msg3("should go to stdout and file");
    std::string msg4("should go to stderr and file");
    LOG(TPri::NOTICE) << msg3;
    LOG(TPri::WARNING) << msg4;
    IfLt0(dup2(saved_stdout, 1));
    IfLt0(dup2(saved_stderr, 2));
    IfLt0(close(saved_stdout));
    IfLt0(close(saved_stderr));
    std::string stdout_contents = ReadFileIntoString(stdout_file.GetName());
    std::string stderr_contents = ReadFileIntoString(stderr_file.GetName());
    std::ostringstream os1;
    os1 << msg1 << std::endl << msg3 << std::endl;
    std::ostringstream os2;
    os2 << msg2 << std::endl << msg4 << std::endl;
    ASSERT_EQ(stdout_contents, os1.str());
    ASSERT_EQ(stderr_contents, os2.str());
  }

  TEST_F(TLogTest, NoLoggingTest) {
    TTmpFile stdout_file(name_template, true /* delete_on_destroy */);
    TTmpFile stderr_file(name_template, true /* delete_on_destroy */);
    int saved_stdout = IfLt0(dup(1));
    int saved_stderr = IfLt0(dup(2));
    IfLt0(dup2(stdout_file.GetFd(), 1));
    IfLt0(dup2(stderr_file.GetFd(), 2));
    SetLogMask(UpTo(TPri::NOTICE));
    SetLogWriter(false /* enable_stdout_stderr */, false /* enable_syslog */,
        std::string());
    std::string msg1("first message to ignore");
    std::string msg2("second message to ignore");
    LOG(TPri::NOTICE) << msg1;
    LOG(TPri::WARNING) << msg2;
    IfLt0(dup2(saved_stdout, 1));
    IfLt0(dup2(saved_stderr, 2));
    IfLt0(close(saved_stdout));
    IfLt0(close(saved_stderr));
    std::string stdout_contents = ReadFileIntoString(stdout_file.GetName());
    std::string stderr_contents = ReadFileIntoString(stderr_file.GetName());
    ASSERT_EQ(stdout_contents, std::string());
    ASSERT_EQ(stderr_contents, std::string());
  }

  TEST_F(TLogTest, RateLimitTest) {
    TTmpFile tmp_file(name_template, true /* delete_on_destroy */);
    SetLogMask(UpTo(TPri::INFO));
    SetLogWriter(false /* enable_stdout_stderr */, false /* enable_syslog */,
        tmp_file.GetName());
    std::cout << "This test should take about 10 seconds" << std::endl;
    auto start = std::chrono::steady_clock::now();
    decltype(start - start) limit(std::chrono::seconds(10));

    while ((std::chrono::steady_clock::now() - start) < limit) {
      LOG_R(TPri::INFO, std::chrono::seconds(1)) << "message";
    }

    std::string file_contents = ReadFileIntoString(tmp_file.GetName());
    size_t num_lines = std::count(file_contents.begin(), file_contents.end(),
        '\n');

    // In practice, num_lines should almost always be 10, but allow lots of
    // room for timing errors to avoid false test failures.  We just want to
    // verify that log messages are being rate limited.  If rate limiting was
    // broken, num_lines would be a huge value.
    ASSERT_GE(num_lines, 5U);
    ASSERT_LE(num_lines, 15U);
  }

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
