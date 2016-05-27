/* <dory/client/path_too_long.h>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

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

   Exception class thrown by subclasses of TClientSenderBase to indicate that a
   pathname for a UNIX domain socket is too long.
 */

#pragma once

#include <stdexcept>
#include <string>

namespace Dory {

  namespace Client {

    class TPathTooLong : public std::runtime_error {
      public:
      explicit TPathTooLong(const char *path)
          : std::runtime_error(MakeWhatArg(path)) {
      }

      explicit TPathTooLong(const std::string &path)
          : TPathTooLong(path.c_str()) {
      }

      private:
      static std::string MakeWhatArg(const char *path);
    };  // TPathTooLong

  }  // Client

}  // Dory
