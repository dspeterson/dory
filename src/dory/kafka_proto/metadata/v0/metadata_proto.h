/* <dory/kafka_proto/metadata/v0/metadata_proto.h>

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

   Kafka metadata protocol version 0 implementation class.
 */

#pragma once

#include <dory/kafka_proto/metadata/metadata_protocol.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <base/no_copy_semantics.h>

namespace Dory {

  namespace KafkaProto {

    namespace Metadata {

      namespace V0 {

        class TMetadataProto final : public TMetadataProtocol {
          NO_COPY_SEMANTICS(TMetadataProto);

          public:
          TMetadataProto() = default;

          ~TMetadataProto() override = default;

          /* Request metadata for all topics. */
          void WriteAllTopicsMetadataRequest(std::vector<uint8_t> &result,
              int32_t correlation_id) const override;

          void WriteSingleTopicMetadataRequest(
              std::vector<uint8_t> &result, const char *topic,
              int32_t correlation_id) const override;

          TMetadata *BuildMetadataFromResponse(
              const void *response_buf,
              size_t response_buf_size) const override;

          bool TopicAutocreateWasSuccessful(const char *topic,
              const void *response_buf,
              size_t response_buf_size) const override;
        };  // TMetadataProto

      }  // V0

    }  // Metadata

  }  // KafkaProto

}  // Dory
