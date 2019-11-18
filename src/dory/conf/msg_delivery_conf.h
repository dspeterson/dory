/* <dory/conf/msg_delivery_conf.h>

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

   Class representing message delivery section from Dory's config file.
 */

#pragma once

#include <cstddef>

namespace Dory {

  namespace Conf {

    struct TMsgDeliveryConf final {
      bool TopicAutocreate = false;

      size_t MaxFailedDeliveryAttempts = 5;

      size_t ShutdownMaxDelay = 30000;

      size_t DispatcherRestartMaxDelay = 5000;

      size_t MetadataRefreshInterval = 15;

      bool CompareMetadataOnRefresh = true;

      size_t KafkaSocketTimeout = 60;

      size_t PauseRateLimitInitial = 5000;

      size_t PauseRateLimitMaxDouble = 4;

      size_t MinPauseDelay = 5000;
    };  // TMsgDeliveryConf

  };  // Conf

}  // Dory
