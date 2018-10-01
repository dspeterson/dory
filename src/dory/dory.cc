/* <dory/dory.cc>

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

   Kafka producer daemon.
 */

#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <signal.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <base/opt.h>
#include <dory/dory_server.h>
#include <dory/build_id.h>
#include <dory/config.h>
#include <dory/util/arg_parse_error.h>
#include <dory/util/handle_xml_errors.h>
#include <dory/util/misc_util.h>
#include <dory/util/dory_xml_init.h>
#include <server/daemonize.h>

using namespace xercesc;

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace Xml;

static int DoryMain(int argc, char *argv[]) {
  TDoryXmlInit xml_init;
  TOpt<TDoryServer::TServerConfig> dory_config;
  bool large_sendbuf_required = false;

  try {
    xml_init.Init();
    TOpt<std::string> opt_err_msg = HandleXmlErrors(
        [&]() -> void {
          /* TODO: Enable support for LZ4 compression.  To enable LZ4, dory's
             wire protocol implementation needs to be updated to support asking
             the brokers what version of Kafka they are running.  LZ4 support
             will be enabled only for broker versions >= 0.10.0.0.  Enabling
             LZ4 for earlier versions would require a messy workaround for a
             bug in Kafka.  See
             https://cwiki.apache.org/confluence/display/KAFKA/KIP-57+-+Interoperable+LZ4+Framing
             for details. */
          bool enable_lz4 = false;

          dory_config.MakeKnown(TDoryServer::CreateConfig(argc, argv,
              large_sendbuf_required, false, enable_lz4));
        }
    );

    if (opt_err_msg.IsKnown()) {
      std::cerr << *opt_err_msg << std::endl;
      return EXIT_FAILURE;
    }

    const TConfig &config = dory_config->GetCmdLineConfig();

    if (config.Daemon) {
      pid_t pid = Server::Daemonize();

      if (pid) {
        std::cout << pid << std::endl;
        return EXIT_SUCCESS;
      }
    }

    InitSyslog(argv[0], config.LogLevel, config.LogEcho);
  } catch (const TArgParseError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception &x) {
    std::cerr << "Error during server initialization: " << x.what()
        << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Unknown error during server initialization" << std::endl;
    return EXIT_FAILURE;
  }

  std::unique_ptr<TDoryServer> dory;

  try {
    dory.reset(new TDoryServer(std::move(*dory_config)));
  } catch (const std::bad_alloc &) {
    syslog(LOG_ERR,
        "Failed to allocate memory during server initialization.  Try specifying a smaller value for the --msg_buffer_max option.");
    return EXIT_FAILURE;
  }

  dory_config.Reset();

  /* Fail early if server is already running. */
  dory->BindStatusSocket(false);

  LogConfig(dory->GetConfig());

  if (large_sendbuf_required) {
    syslog(LOG_WARNING,
        "Clients sending maximum-sized UNIX domain datagrams need to set SO_SNDBUF above the default value.");
  }

  syslog(LOG_NOTICE, "Pool block size is %lu bytes",
         static_cast<unsigned long>(dory->GetPoolBlockSize()));
  TDoryServer::TSignalHandlerInstaller handler_installer;
  return dory->Run();
}

int main(int argc, char *argv[]) {
  int ret = EXIT_SUCCESS;

  try {
    ret = DoryMain(argc, argv);
  } catch (const std::exception &x) {
    syslog(LOG_ERR, "Fatal error in main thread: %s", x.what());
    _exit(EXIT_FAILURE);
  } catch (...) {
    syslog(LOG_ERR, "Fatal unknown error in main thread");
    _exit(EXIT_FAILURE);
  }

  return ret;
}
