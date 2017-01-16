/* <dory/kafka_proto/metadata/metadata_protocol.h>

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

   Base class that provides a uniform API for dealing with different versions
   of the Kafka metadata wire format.  Derived classes implement specific
   versions, and the core dory server code interacts with a base class
   instance to insulate itself from version-specific wire format details.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <base/no_copy_semantics.h>
#include <base/thrower.h>
#include <dory/metadata.h>

namespace Dory {

  namespace KafkaProto {

    namespace Metadata {

      class TMetadataProtocol {
        NO_COPY_SEMANTICS(TMetadataProtocol);

        public:
        class TBadMetadataResponse : public std::runtime_error {
          protected:
          explicit TBadMetadataResponse(const char *msg)
              : std::runtime_error(msg) {
          }

          public:
          virtual ~TBadMetadataResponse() noexcept { }
        };  // TBadMetadataResponse

        DEFINE_ERROR(TPartitionHasUnknownBroker, TBadMetadataResponse,
            "Partition in metadata respose references unknown broker");

        virtual ~TMetadataProtocol() noexcept { }

        /* Write an all topics metadata request to 'result'.  Resize 'result'
           to the size of the written request. */
        virtual void WriteAllTopicsMetadataRequest(
            std::vector<uint8_t> &result, int32_t correlation_id) const = 0;

        virtual void WriteSingleTopicMetadataRequest(
            std::vector<uint8_t> &result, const char *topic,
            int32_t correlation_id) const = 0;

        /* Throws a subclass of std::runtime_error on bad metadata response.
           Caller assumes responsibility for deleting returned object. */
        virtual TMetadata *BuildMetadataFromResponse(const void *response_buf,
            size_t response_buf_size) const = 0;

        virtual bool TopicAutocreateWasSuccessful(const char *topic,
            const void *response_buf, size_t response_buf_size) const = 0;

        protected:
        TMetadataProtocol() = default;
      };  // TMetadataProtocol

    }  // Metadata

  }  // KafkaProto

}  // Dory
