/* <dory/conf/http_interface_conf.h>

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

   Class representing HTTP interface config section from Dory's config file.
 */

#pragma once

#include <cstddef>

#include <netinet/in.h>

#include <dory/conf/conf_error.h>

namespace Dory {

  namespace Conf {

    class THttpInterfaceInvalidPort final : public TConfError {
      public:
      THttpInterfaceInvalidPort()
          : TConfError("Invalid HTTP interface port") {
      }
    };  // THttpInterfaceInvalidPort

    class THttpInterfaceInvalidDiscardReportInterval final
        : public TConfError {
      public:
      THttpInterfaceInvalidDiscardReportInterval()
          : TConfError("Invalid HTTP interface discard report interval") {
      }
    };  // THttpInterfaceInvalidDiscardReportInterval

    struct THttpInterfaceConf final {
      in_port_t Port = 9090;

      bool LoopbackOnly = false;

      size_t DiscardReportInterval = 600;

      size_t BadMsgPrefixSize = 256;

      void SetPort(in_port_t port);

      void SetDiscardReportInterval(size_t value);
    };  // THttpInterfaceConf

  };  // Conf

}  // Dory
