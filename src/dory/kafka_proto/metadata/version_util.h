/* <dory/kafka_proto/metadata/version_util.h>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

   Factory function and related utilities for choosing Kafka metadata protocol.
 */

#pragma once

#include <cstddef>
#include <vector>

#include <dory/kafka_proto/metadata/metadata_protocol.h>

namespace Dory {

  namespace KafkaProto {

    namespace Metadata {

      TMetadataProtocol *ChooseMetadataProto(size_t api_version);

      /* Return a vector of all metadata API versions that Dory supports.  The
         result will be sorted in ascending order. */
      const std::vector<size_t> &GetSupportedMetadataApiVersions();

      bool IsMetadataApiVersionSupported(size_t api_version);

    }  // Metadata

  }  // KafkaProto

}  // Dory
