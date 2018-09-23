/* <dory/kafka_proto/produce/v0/produce_response_reader.h>

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

   Class for reading the contents of a produce response from a Kafka broker.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <base/thrower.h>
#include <dory/kafka_proto/produce/produce_response_reader_api.h>
#include <dory/kafka_proto/produce/v0/produce_response_constants.h>
#include <dory/kafka_proto/request_response.h>

namespace Dory {

  namespace KafkaProto {

    namespace Produce {

      namespace V0 {

        class TProduceResponseReader final : public TProduceResponseReaderApi {
          public:
          DEFINE_ERROR(TShortResponse, TBadProduceResponse,
              "Kafka produce response is too short");

          DEFINE_ERROR(TResponseTruncated, TBadProduceResponse,
              "Kafka produce response is truncated");

          DEFINE_ERROR(TBadTopicCount, TBadProduceResponse,
              "Invalid topic count in Kafka produce response");

          DEFINE_ERROR(TBadTopicNameLength, TBadProduceResponse,
              "Bad topic name length in Kafka produce response");

          DEFINE_ERROR(TBadPartitionCount, TBadProduceResponse,
              "Invalid partition count in Kafka produce response");

          static size_t MinSize() {
            return REQUEST_OR_RESPONSE_SIZE_SIZE + PRC::CORRELATION_ID_SIZE +
                PRC::TOPIC_COUNT_SIZE;
          }

          TProduceResponseReader();

          ~TProduceResponseReader() override = default;

          void Clear() noexcept override;

          void SetResponse(const void *response,
              size_t response_size) override;

          int32_t GetCorrelationId() const override;

          size_t GetNumTopics() const override;

          bool FirstTopic() override;

          bool NextTopic() override;

          const char *GetCurrentTopicNameBegin() const override;

          const char *GetCurrentTopicNameEnd() const override;

          size_t GetNumPartitionsInCurrentTopic() const override;

          bool FirstPartitionInTopic() override;

          bool NextPartitionInTopic() override;

          int32_t GetCurrentPartitionNumber() const override;

          int16_t GetCurrentPartitionErrorCode() const override;

          int64_t GetCurrentPartitionOffset() const override;

          private:
          using PRC = TProduceResponseConstants;

          const uint8_t *GetPartitionStart(int32_t index) const;

          void InitCurrentTopic();

          void InitCurrentPartition();

          const uint8_t *Begin;

          const uint8_t *End;

          int32_t NumTopics;

          int32_t CurrentTopicIndex;

          const uint8_t *CurrentTopicBegin;

          const uint8_t *CurrentTopicNameEnd;

          int32_t NumPartitionsInTopic;

          int32_t CurrentPartitionIndexInTopic;
        };  // TProduceResponseReader

      }  // V0

    }  // Produce

  }  // KafkaProto

}  // Dory
