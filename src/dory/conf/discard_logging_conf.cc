/* <dory/conf/discard_logging_conf.cc>

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

   Implements <dory/conf/discard_logging_conf.h>.
 */

#include <dory/conf/discard_logging_conf.h>

#include <cassert>

using namespace Dory;
using namespace Dory::Conf;

void TDiscardLoggingConf::SetPath(const std::string &path) {
  if (!path.empty() && (path[0] != '/')) {
    throw TDiscardLoggingRelativePath();
  }

  Path = path;
}
