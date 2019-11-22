/* <dory/mock_kafka_server/config.h>

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

   Configuration options for mock Kafka server.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <netinet/in.h>

namespace Dory {

  namespace MockKafkaServer {

    struct TConfig {
      /* Throws TInvalidArgError on error parsing args. */
      TConfig(int argc, const char *const argv[]);

      bool LogEcho = false;

      size_t ProduceApiVersion = 0;

      size_t MetadataApiVersion = 0;

      size_t QuietLevel = 0;

      std::string SetupFile;

      std::string OutputDir;

      in_port_t CmdPort = 9080;

      bool SingleOutputFile = false;
    };  // TCmdLineArgs

  }  // MockKafkaServer

}  // Dory
