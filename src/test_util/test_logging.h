/* <test_util/test_logging.h>

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

   Logging setup for unit tests.
 */

#pragma once

#include <base/tmp_file.h>

namespace TestUtil {

  /* Initialize logging for unit tests.  This should be called from main(),
     after calling ::testing::InitGoogleTest() and before calling
     RUN_ALL_TESTS().  It returns a TTmpFile representing a file in /tmp that
     log output is written to.  If all tests run successfully, the TTmpFile
     destructor will delete the logfile.  On fatal error or test failure, the
     test program will terminate before the TTmpFile destructor has a chance to
     execute, leaving behind the logfile for analysis. */
  Base::TTmpFile InitTestLogging(const char *prog_name);

}  // TestUtil
