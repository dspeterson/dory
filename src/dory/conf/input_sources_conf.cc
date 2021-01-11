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

#include <stdexcept>

using namespace Dory;
using namespace Dory::Conf;

void TInputSourcesConf::SetUnixDgConf(const std::string &path,
    std::optional<mode_t> mode) {
  if (!path.empty() && (path[0] != '/')) {
    throw std::logic_error("UNIX datagram path must be absolute");
  }

  if (mode && (*mode > 0777)) {
    throw std::logic_error("Invalid UNIX datagram file mode");
  }

  UnixDgPath = path;
  UnixDgMode = mode;
}

void TInputSourcesConf::SetUnixStreamConf(const std::string &path,
    std::optional<mode_t> mode) {
  if (!path.empty() && (path[0] != '/')) {
    throw std::logic_error("UNIX stream path must be absolute");
  }

  if (mode && (*mode > 0777)) {
    throw std::logic_error("Invalid UNIX stream file mode");
  }

  UnixStreamPath = path;
  UnixStreamMode = mode;
}

void TInputSourcesConf::SetTcpConf(std::optional<in_port_t> port,
    bool allow_input_bind_ephemeral) {
  if (!allow_input_bind_ephemeral && port && (*port == 0)) {
    throw TInvalidTcpInputPort();
  }

  LocalTcpPort = port;
}
