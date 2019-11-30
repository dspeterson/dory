/* <dory/conf/input_sources_conf.cc>

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

   Implements <dory/conf/input_sources_conf.h>.
 */

#include <dory/conf/input_sources_conf.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Conf;

void TInputSourcesConf::SetUnixDgConf(const std::string &path,
    const TOpt<mode_t> &mode) {
  assert(this);

  if (!path.empty() && (path[0] != '/')) {
    throw TInputSourcesRelativeUnixDgPath();
  }

  if (mode.IsKnown() && (*mode > 0777)) {
    throw TInputSourcesInvalidUnixDgFileMode();
  }

  UnixDgPath = path;
  UnixDgMode = mode;
}

void TInputSourcesConf::SetUnixStreamConf(const std::string &path,
    const TOpt<mode_t> &mode) {
  assert(this);

  if (!path.empty() && (path[0] != '/')) {
    throw TInputSourcesRelativeUnixStreamPath();
  }

  if (mode.IsKnown() && (*mode > 0777)) {
    throw TInputSourcesInvalidUnixStreamFileMode();
  }

  UnixStreamPath = path;
  UnixStreamMode = mode;
}

void TInputSourcesConf::SetTcpConf(const TOpt<in_port_t> &port,
    bool allow_input_bind_ephemeral) {
  assert(this);

  if (!allow_input_bind_ephemeral && port.IsKnown() && (*port == 0)) {
    throw TInvalidTcpInputPort();
  }

  LocalTcpPort = port;
}
