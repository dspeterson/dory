/* <dory/conf/discard_logging_conf.h>

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

   Class representing discard logging section from Dory's config file.
 */

#pragma once

#include <cstddef>
#include <limits>
#include <string>

#include <dory/conf/conf_error.h>

namespace Dory {

  namespace Conf {

    class TDiscardLoggingRelativePath final : public TConfError {
      public:
      TDiscardLoggingRelativePath()
          : TConfError("Discard logging path must be absolute") {
      }
    };  // TDiscardLoggingRelativePath

    struct TDiscardLoggingConf final {
      std::string Path;

      size_t MaxFileSize = 1024 * 1024;

      size_t MaxArchiveSize = 32 * 1024 * 1024;

      size_t MaxMsgPrefixSize = std::numeric_limits<size_t>::max();

      void SetPath(const std::string &path);
    };  // TDiscardLoggingConf

  };  // Conf

}  // Dory
