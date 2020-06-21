/* <dory/util/arg_parse_error.h>

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

   Exception class for reporting errors parsing command line arguments.
 */

#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace Dory {

  namespace Util {

    class TInvalidArgError : public std::runtime_error {
      public:
      explicit TInvalidArgError(std::string &&msg)
          : std::runtime_error(MakeWhatArg(msg)),
            Msg(std::move(msg)) {
      }

      TInvalidArgError(std::string &&msg, std::string &&arg_id)
          : std::runtime_error(MakeWhatArg(msg, arg_id)),
            Msg(std::move(msg)),
            ArgId(std::move(arg_id)) {
      }

      const std::string &GetMsg() const {
        return Msg;
      }

      const std::string &GetArgId() const {
        return ArgId;
      }

      private:
      std::string MakeWhatArg(const std::string &msg) {
        std::string what_arg("Error: ");
        what_arg += msg;
        return what_arg;
      }

      std::string MakeWhatArg(const std::string &msg,
          const std::string &arg_id) {
        std::string what_arg("Error: ");
        what_arg += msg;
        what_arg += " for arg ";
        what_arg += arg_id;
        return what_arg;
      }

      std::string Msg;

      std::string ArgId;
    };  // TInvalidArgError

  }  // Util

}  // Dory
