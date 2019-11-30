/* <dory/mock_kafka_server/cmd_line_args.cc>

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

   Implements <dory/mock_kafka_server/cmd_line_args.h>.
 */

#include <dory/mock_kafka_server/cmd_line_args.h>

#include <base/basename.h>
#include <dory/build_id.h>
#include <dory/util/invalid_arg_error.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::MockKafkaServer;
using namespace Dory::Util;

static void ParseArgs(int argc, const char *const argv[], TCmdLineArgs &args) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd("Mock Kafka server for testing Dory.", ' ', dory_build_id);
    SwitchArg arg_log_echo("", "log-echo",
        "Echo syslog messages to standard error.", cmd, args.LogEcho);
    ValueArg<decltype(args.ProduceApiVersion)> arg_produce_api_version("",
        "produce-api-version",
        "Version of Kafka produce API to use (currently only 0 is supported).",
        false, args.ProduceApiVersion, "VERSION");
    cmd.add(arg_produce_api_version);
    ValueArg<decltype(args.MetadataApiVersion)> arg_metadata_api_version("",
        "metadata-api-version",
        "Version of Kafka metadata API to use (currently only 0 is "
        "supported).",
        false, args.MetadataApiVersion, "VERSION");
    cmd.add(arg_metadata_api_version);
    ValueArg<decltype(args.QuietLevel)> arg_quiet_level("", "quiet-level",
        "Limit output verbosity.", false, args.QuietLevel, "LEVEL");
    cmd.add(arg_quiet_level);
    ValueArg<decltype(args.SetupFile)> arg_setup_file("", "setup-file",
        "Setup file.", true, args.SetupFile, "FILE");
    cmd.add(arg_setup_file);
    ValueArg<decltype(args.OutputDir)> arg_output_dir("", "output-dir",
        "Directory where server writes its output files.", true,
        args.OutputDir, "DIR");
    cmd.add(arg_output_dir);
    ValueArg<decltype(args.CmdPort)> arg_cmd_port("", "cmd-port",
        "Command port (for error injection, etc.).", false, args.CmdPort,
        "PORT");
    cmd.add(arg_cmd_port);
    SwitchArg arg_single_output_file("", "single-output-file",
        "Use single output file for all clients", cmd,
        args.SingleOutputFile);
    cmd.parse(argc, &arg_vec[0]);
    args.LogEcho = arg_log_echo.getValue();
    args.ProduceApiVersion = arg_produce_api_version.getValue();
    args.MetadataApiVersion = arg_metadata_api_version.getValue();
    args.QuietLevel = arg_quiet_level.getValue();
    args.SetupFile = arg_setup_file.getValue();
    args.OutputDir = arg_output_dir.getValue();
    args.CmdPort = arg_cmd_port.getValue();
    args.SingleOutputFile = arg_single_output_file.getValue();
  } catch (const ArgException &x) {
    throw TInvalidArgError(x.error(), x.argId());
  }
}

TCmdLineArgs::TCmdLineArgs(int argc, const char *const argv[]) {
  ParseArgs(argc, argv, *this);
}
