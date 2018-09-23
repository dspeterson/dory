/* <dory/mock_kafka_server/v0_client_handler_factory.h>

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

   Factory to create client handlers for Kafka protocol version 0.
 */

#pragma once

#include <cstddef>
#include <memory>

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <dory/mock_kafka_server/client_handler_factory_base.h>

namespace Dory {

  namespace MockKafkaServer {

    class TV0ClientHandlerFactory final : public TClientHandlerFactoryBase {
      NO_COPY_SEMANTICS(TV0ClientHandlerFactory);

      public:
      TV0ClientHandlerFactory(const TConfig &config,
                              const TSetup::TInfo &setup)
          : TClientHandlerFactoryBase(config, setup) {
      }

      ~TV0ClientHandlerFactory() override = default;

      TSingleClientHandlerBase *CreateClientHandler(
          const std::shared_ptr<TPortMap> &port_map, size_t port_offset,
          TSharedState &ss, Base::TFd &&client_socket) override;
    };  // TV0ClientHandlerFactory

  }  // MockKafkaServer

}  // Dory
