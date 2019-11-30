/* <test_util/test_logging.cc>

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

   Implements <test_util/test_logging.h>.
 */

#include <test_util/test_logging.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include <base/basename.h>
#include <base/opt.h>
#include <log/log.h>
#include <log_util/init_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Log;
using namespace LogUtil;
using namespace TestUtil;

static Base::TTmpFile MakeTestLogfile(const std::string &prog_basename) {
  std::string name_template("/tmp/");
  name_template += prog_basename;
  name_template += ".XXXXXX";
  return Base::TTmpFile(name_template, true /* delete_on_destroy */);
}

namespace {

  class TestFailureEventListener : public testing::EmptyTestEventListener {
    public:
    void OnTestProgramEnd(const testing::UnitTest& unit_test) override {
      if (unit_test.Failed()) {
        static const char msg[] = "Aborting on test failure";
        std::cerr << msg << std::endl;
        LOG(TPri::ERR) << msg;  // write to logfile in /tmp

        /* Cause immediate program termination before the destructor for the
           TTmpFile representing the logfile has a chance to execute.  This
           will leave the logfile behind for analysis.

           Unless we are running with libasan, a core dump should be created.
           libasan disables core dumps by default, since they may be very
           large. */
        std::abort();
      }
    }
  };

};

Base::TTmpFile TestUtil::InitTestLogging(const char *prog_name) {
  const std::string prog_basename = Base::Basename(prog_name);
  Base::TTmpFile tmp_logfile = MakeTestLogfile(prog_basename);

  /* For unit tests, write normal log messages only to the caller-specified
     logfile, which will be a temporary file.  Fatal error output goes to both
     stderr and the logfile. */
  InitLogging(prog_name, TPri::DEBUG, false /* enable_stdout_stderr */,
      false /* enable_syslog */, tmp_logfile.GetName(), TOpt<mode_t>());

  testing::UnitTest::GetInstance()->listeners().Append(
      new TestFailureEventListener);

  std::cout << "Logfile [" << tmp_logfile.GetName()
      << "]: delete when all tests pass" << std::endl;

  /* Write test name at start of logfile so it's obvious which test produced
     it. */
  LOG(TPri::NOTICE) << "Log started for test [" << prog_basename << "]";

  return tmp_logfile;
}
