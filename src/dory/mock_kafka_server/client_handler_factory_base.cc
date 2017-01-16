/* <dory/mock_kafka_server/client_handler_factory_base.cc>

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

   Implements <dory/mock_kafka_server/client_handler_factory_base.h>.
 */

#include <dory/mock_kafka_server/client_handler_factory_base.h>

#include <dory/mock_kafka_server/v0_client_handler_factory.h>

using namespace Dory;
using namespace Dory::MockKafkaServer;

TClientHandlerFactoryBase *
TClientHandlerFactoryBase::CreateFactory(const TConfig &config,
    const TSetup::TInfo &setup) {
  /* TODO: clean up API version logic */
  return ((config.ProduceApiVersion == 0) &&
      (config.MetadataApiVersion == 0)) ?
      new TV0ClientHandlerFactory(config, setup) : nullptr;
}
