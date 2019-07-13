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

#include <syslog.h>

#include <dory/compress/compression_init.h>
#include <dory/mock_kafka_server/config.h>
#include <dory/mock_kafka_server/server.h>
#include <dory/util/arg_parse_error.h>
#include <dory/util/misc_util.h>
#include <log/pri.h>

using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::MockKafkaServer;
using namespace Dory::Util;
using namespace Log;

int mock_kafka_server_main(int argc, char **argv) {
  std::unique_ptr<TConfig> cfg;

  try {
    cfg.reset(new TConfig(argc, argv));
  } catch (const TArgParseError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  }

  InitLogging(argv[0], TPri::DEBUG, cfg->LogEcho);

  /* Force all supported compression libraries to load.  We want to fail early
     if a library fails to load. */
  CompressionInit();

  return TServer(*cfg, false, false).Run();
}

int main(int argc, char **argv) {
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
