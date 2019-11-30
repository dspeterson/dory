/* <dory/cmd_line_args.cc>

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

   Implements <dory/cmd_line_args.h>.
 */

#include <dory/cmd_line_args.h>

#include <vector>

#include <base/basename.h>
#include <dory/build_id.h>
#include <dory/util/invalid_arg_error.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;

static void ParseArgs(int argc, const char *const argv[], TCmdLineArgs &args) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd("Producer daemon for Apache Kafka", ' ', dory_build_id);
    ValueArg<decltype(args.ConfigPath)> arg_config_path("", "config-path",
        "Pathname of config file.", true, args.ConfigPath, "PATH");
    cmd.add(arg_config_path);
    SwitchArg arg_daemon("", "daemon", "Run as daemon.", cmd, args.Daemon);
    cmd.parse(argc, &arg_vec[0]);
    args.ConfigPath = arg_config_path.getValue();
    args.Daemon = arg_daemon.getValue();
  } catch (const ArgException &x) {
    throw TInvalidArgError(x.error(), x.argId());
  }
}

TCmdLineArgs::TCmdLineArgs(int argc, const char *const argv[]) {
  ParseArgs(argc, argv, *this);
}
