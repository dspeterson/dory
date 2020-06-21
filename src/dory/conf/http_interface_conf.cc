/* <dory/conf/http_interface_conf.cc>

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

   Implements <dory/conf/http_interface_conf.h>.
 */

#include <dory/conf/http_interface_conf.h>

#include <cassert>

using namespace Dory;
using namespace Dory::Conf;

void THttpInterfaceConf::SetPort(in_port_t port) {
  if (port < 1) {
    throw THttpInterfaceInvalidPort();
  }

  Port = port;
}

void THttpInterfaceConf::SetDiscardReportInterval(size_t value) {
  if (value < 1) {
    throw THttpInterfaceInvalidDiscardReportInterval();
  }

  DiscardReportInterval = value;
}
