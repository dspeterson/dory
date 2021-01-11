/* <dory/conf/common_logging_conf.cc>

   ----------------------------------------------------------------------------
   Copyright 2020 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/conf/coommon_logging_conf.h>.
 */

#include <dory/conf/common_logging_conf.h>

#include <stdexcept>

using namespace Dory;
using namespace Dory::Conf;

void TCommonLoggingConf::SetFileConf(const std::string &path,
    std::optional<mode_t> mode) {
  if (!path.empty() && (path[0] != '/')) {
    throw std::logic_error("Path must be absolute");
  }

  if (mode && (*mode > 0777)) {
    throw std::logic_error("Invalid file mode");
  }

  FilePath = path;
  FileMode = mode;
}
