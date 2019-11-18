/* <dory/conf/input_config_conf.h>

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

   Class representing input config section from Dory's config file.
 */

#pragma once

#include <cstddef>

namespace Dory {

  namespace Conf {

    struct TInputConfigConf final {
      size_t MaxBuffer = 16 * 1024 * 1024;

      size_t MaxDatagramMsgSize = 64 * 1024;

      bool AllowLargeUnixDatagrams = false;

      size_t MaxStreamMsgSize = 2 * 1024 * 1024;
    };  // TInputConfigConf

  };  // Conf

}  // Dory
