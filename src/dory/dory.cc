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

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include <sys/types.h>

#include <base/error_util.h>
#include <base/file_reader.h>
#include <dory/cmd_line_args.h>
#include <dory/conf/conf.h>
#include <dory/dory_server.h>
#include <dory/util/dory_xml_init.h>
#include <dory/util/invalid_arg_error.h>
#include <dory/util/misc_util.h>
#include <log/log.h>
#include <log_util/init_logging.h>
#include <server/daemonize.h>
#include <xml/config/config_errors.h>

using namespace xercesc;

using namespace Base;
using namespace Dory;
using namespace Dory::Conf;
using namespace Dory::Util;
using namespace Log;
using namespace LogUtil;
using namespace Xml;
using namespace Xml::Config;

static int DoryMain(int argc, char *const argv[]) {
  TDoryXmlInit xml_init(false /* init_on_construction */);
  TCmdLineArgs args;
  TConf conf;
  bool large_sendbuf_required = false;

  try {
    /* Handle command line arg errors before all other kinds of errors (such as
       errors initializing XML library, for instance). */
    args = TCmdLineArgs(argc, argv);

    /* Init XML processing subsystem.  Needed for config file parsing. */
    xml_init.Init();

    /* TODO: Enable support for LZ4 compression.  To enable LZ4, dory's wire
       protocol implementation needs to be updated to support asking the
       brokers what version of Kafka they are running.  LZ4 support will be
       enabled only for broker versions >= 0.10.0.0.  Enabling LZ4 for earlier
       versions would require a messy workaround for a bug in Kafka.  See
       https://cwiki.apache.org/confluence/display/KAFKA/KIP-57+-+Interoperable+LZ4+Framing
       for details. */
    conf = Dory::Conf::TConf::TBuilder(
        false /* allow_input_bind_ephemeral */,
        false /* enable_lz4 */).Build(ReadFileIntoString(args.ConfigPath));

    /* may throw TInvalidArgError */
    large_sendbuf_required = TDoryServer::CheckUnixDgSize(conf);
  } catch (const TInvalidArgError &x) {
    /* Error processing command line arguments. */
    LOG(TPri::ERR) << x.what();
    return EXIT_FAILURE;
  } catch (const TXmlError &x) {
    LOG(TPri::ERR) << "Error in config file: " << x.what();
    return EXIT_FAILURE;
  }

  /* Note: An error may cause us to invoke the logging subsystem as
     follows:

         LOG(TPri::ERR) << "Something bad happened";

     before we initialize it here.  This is ok, since an attempt to log to
     an uninitialized logging subsystem will trigger initialization with a
     default implementation that logs to stdout/stderr. */
  InitLogging(argv[0], conf.LoggingConf.Common.Pri,
      conf.LoggingConf.Common.EnableStdoutStderr && !args.Daemon,
      conf.LoggingConf.Common.EnableSyslog,
      conf.LoggingConf.Common.FilePath, conf.LoggingConf.Common.FileMode);

  LOG(TPri::NOTICE) << "Log started";

  if (args.Daemon) {
    pid_t pid = Server::Daemonize();

    if (pid) {
      std::cout << pid << std::endl;
      return EXIT_SUCCESS;
    }
  }

  /* After this point, all signals will be blocked, and should remain blocked
     for all threads except the signal handler thread for the lifetime of the
     application.  Do this after we daemonize, since becoming a daemon involves
     calling fork().  If a multithreaded process calls fork(), only the calling
     thread exists in the child.  Therefore if we tried to start the signal
     handler thread before becoming a daemon, that thread would no longer exist
     after daemonization.  In general, one must exercise great care when
     calling fork() from a multithreaded process (see
     https://pubs.opengroup.org/onlinepubs/9699919799/functions/fork.html ). */
  TSignalHandlerThreadStarter signal_handler_starter;

  TDoryServer::PrepareForInit(conf);
  std::unique_ptr<TDoryServer> dory;

  try {
    dory.reset(new TDoryServer(std::move(args), std::move(conf),
        GetShutdownRequestedFd()));
  } catch (const std::bad_alloc &) {
    LOG(TPri::ERR)
        << "Failed to allocate memory during server initialization.  Try "
        << "specifying a smaller value for the --msg_buffer_max option.";
    return EXIT_FAILURE;
  }

  /* Fail if server is already running. */
  dory->BindStatusSocket(false);

  if (large_sendbuf_required) {
    LOG(TPri::WARNING)
        << "Clients sending maximum-sized UNIX domain datagrams need to set "
        << "SO_SNDBUF above the default value.";
  }

  return dory->Run();
}

int main(int argc, char *argv[]) {
  int ret = EXIT_SUCCESS;

  try {
    ret = DoryMain(argc, argv);
  } catch (const std::exception &x) {
    LOG(TPri::ERR) << "Fatal error in main thread: " << x.what();
    Die("Terminating on fatal error");
  } catch (...) {
    Die("Fatal unknown error in main thread");
  }

  return ret;
}
