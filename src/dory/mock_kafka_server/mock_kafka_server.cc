/* <dory/mock_kafka_server/mock_kafka_server.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Mock kafka server that receives messages from dory daemon.
 */

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>

#include <dory/compress/compression_init.h>
#include <dory/mock_kafka_server/cmd_line_args.h>
#include <dory/mock_kafka_server/server.h>
#include <dory/util/invalid_arg_error.h>
#include <log/log.h>
#include <log/pri.h>
#include <log_util/init_logging.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::MockKafkaServer;
using namespace Dory::Util;
using namespace Log;
using namespace LogUtil;

int mock_kafka_server_main(int argc, const char *const *argv) {
  std::unique_ptr<TCmdLineArgs> args;

  try {
    args.reset(new TCmdLineArgs(argc, argv));
  } catch (const TInvalidArgError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  }

  InitLogging(argv[0], TPri::DEBUG, args->LogEcho /* enable_stdout_stderr */,
      true /* enable_syslog */, std::string() /* file_path */);
  LOG(TPri::NOTICE) << "Log started";

  /* Force all supported compression libraries to load.  We want to fail early
     if a library fails to load. */
  CompressionInit();

  return TServer(*args, false, false).Run();
}

int main(int argc, const char *const *argv) {
  int ret = EXIT_SUCCESS;

  try {
    ret = mock_kafka_server_main(argc, argv);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    ret = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "error: unexpected unknown exception" << std::endl;
    ret = EXIT_FAILURE;
  }

  return ret;
}
