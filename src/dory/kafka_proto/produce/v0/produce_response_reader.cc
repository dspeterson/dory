/* <dory/kafka_proto/produce/v0/produce_response_reader.cc>

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

   Implements <dory/kafka_proto/produce/v0/produce_response_reader.h>.
 */

#include <dory/kafka_proto/produce/v0/produce_response_reader.h>

#include <cassert>

#include <base/counter.h>
#include <base/field_access.h>

using namespace Dory;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Produce::V0;

DEFINE_COUNTER(ProduceResponseBadPartitionCount);
DEFINE_COUNTER(ProduceResponseBadTopicCount);
DEFINE_COUNTER(ProduceResponseBadTopicNameLength);
DEFINE_COUNTER(ProduceResponseTruncated1);
DEFINE_COUNTER(ProduceResponseTruncated2);
DEFINE_COUNTER(ProduceResponseTruncated3);
DEFINE_COUNTER(ProduceResponseTruncated4);
DEFINE_COUNTER(ProduceResponseTruncated5);

TProduceResponseReader::TProduceResponseReader() {
  Clear();
}

void TProduceResponseReader::Clear() noexcept {
  Begin = nullptr;
  End = nullptr;
  NumTopics = 0;
  CurrentTopicIndex = -1;
  CurrentTopicBegin = nullptr;
  CurrentTopicNameEnd = nullptr;
  NumPartitionsInTopic = 0;
  CurrentPartitionIndexInTopic = -1;
}

void TProduceResponseReader::SetResponse(const void *response,
    size_t response_size) {
  assert(response);
  Clear();

  if (response_size < MinSize()) {
    ProduceResponseTruncated1.Increment();
    THROW_ERROR(TShortResponse);
  }

  Begin = reinterpret_cast<const uint8_t *>(response);
  End = Begin + GetRequestOrResponseSize(Begin);

  if ((Begin + response_size) < End) {
    ProduceResponseTruncated2.Increment();
    THROW_ERROR(TResponseTruncated);
  }

  NumTopics = ReadInt32FromHeader(Begin + REQUEST_OR_RESPONSE_SIZE_SIZE +
      PRC::CORRELATION_ID_SIZE);

  if (NumTopics < 0) {
    ProduceResponseBadTopicCount.Increment();
    THROW_ERROR(TBadTopicCount);
  }
}

int32_t TProduceResponseReader::GetCorrelationId() const {
  assert(Begin);
  assert(End);
  assert(NumTopics >= 0);
  return ReadInt32FromHeader(Begin + REQUEST_OR_RESPONSE_SIZE_SIZE);
}

size_t TProduceResponseReader::GetNumTopics() const {
  return static_cast<size_t>(NumTopics);
}

bool TProduceResponseReader::FirstTopic() {
  assert(NumTopics >= 0);

  if (NumTopics < 1) {
    return false;
  }

  CurrentTopicIndex = 0;
  CurrentTopicBegin = Begin + REQUEST_OR_RESPONSE_SIZE_SIZE +
      PRC::CORRELATION_ID_SIZE + PRC::TOPIC_COUNT_SIZE;
  InitCurrentTopic();
  return true;
}

bool TProduceResponseReader::NextTopic() {
  assert(NumTopics >= 0);

  if (CurrentTopicIndex < 0) {
    return FirstTopic();
  }

  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);

  if (CurrentTopicIndex >= NumTopics) {
    throw std::range_error(
        "Invalid topic index while iterating over Kafka produce response");
  }

  if (++CurrentTopicIndex < NumTopics) {
    CurrentTopicBegin = CurrentTopicNameEnd +
        int32_t(PRC::PARTITION_COUNT_SIZE) +
        (NumPartitionsInTopic *
            (int32_t(PRC::PARTITION_SIZE) + int32_t(PRC::ERROR_CODE_SIZE) +
            int32_t(PRC::OFFSET_SIZE)));
    InitCurrentTopic();
    return true;
  }

  CurrentTopicBegin = nullptr;
  CurrentTopicNameEnd = nullptr;
  NumPartitionsInTopic = 0;
  CurrentPartitionIndexInTopic = 0;
  return false;
}

const char *TProduceResponseReader::GetCurrentTopicNameBegin() const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd > CurrentTopicBegin);
  return reinterpret_cast<const char *>(
      CurrentTopicBegin + PRC::TOPIC_NAME_LEN_SIZE);
}

const char *TProduceResponseReader::GetCurrentTopicNameEnd() const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd > CurrentTopicBegin);
  return reinterpret_cast<const char *>(CurrentTopicNameEnd);
}

size_t TProduceResponseReader::GetNumPartitionsInCurrentTopic() const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd > CurrentTopicBegin);
  return static_cast<size_t>(NumPartitionsInTopic);
}

bool TProduceResponseReader::FirstPartitionInTopic() {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);
  assert(NumPartitionsInTopic >= 0);

  if (NumPartitionsInTopic < 1) {
    return false;
  }

  CurrentPartitionIndexInTopic = 0;
  InitCurrentPartition();
  return true;
}

bool TProduceResponseReader::NextPartitionInTopic() {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);
  assert(NumPartitionsInTopic >= 0);

  if (CurrentPartitionIndexInTopic < 0) {
    return FirstPartitionInTopic();
  }

  if (CurrentPartitionIndexInTopic >= NumPartitionsInTopic) {
    throw std::range_error(
        "Invalid partition index while iterating over Kafka produce response");
  }

  if (++CurrentPartitionIndexInTopic < NumPartitionsInTopic) {
    InitCurrentPartition();
    return true;
  }

  return false;
}

int32_t TProduceResponseReader::GetCurrentPartitionNumber() const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);
  assert(NumPartitionsInTopic >= 0);
  assert((CurrentPartitionIndexInTopic >= 0) &&
      (CurrentPartitionIndexInTopic < NumPartitionsInTopic));
  const uint8_t *pos = GetPartitionStart(CurrentPartitionIndexInTopic);
  return ReadInt32FromHeader(pos);
}

int16_t TProduceResponseReader::GetCurrentPartitionErrorCode() const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);
  assert(NumPartitionsInTopic >= 0);
  assert((CurrentPartitionIndexInTopic >= 0) &&
      (CurrentPartitionIndexInTopic < NumPartitionsInTopic));
  const uint8_t *pos = GetPartitionStart(CurrentPartitionIndexInTopic);
  return ReadInt16FromHeader(pos + PRC::PARTITION_SIZE);
}

int64_t TProduceResponseReader::GetCurrentPartitionOffset() const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);
  assert(NumPartitionsInTopic >= 0);
  assert((CurrentPartitionIndexInTopic >= 0) &&
      (CurrentPartitionIndexInTopic < NumPartitionsInTopic));
  const uint8_t *pos = GetPartitionStart(CurrentPartitionIndexInTopic);
  return ReadInt64FromHeader(pos + PRC::PARTITION_SIZE + PRC::ERROR_CODE_SIZE);
}

const uint8_t *TProduceResponseReader::GetPartitionStart(int32_t index) const {
  assert(NumTopics >= 0);
  assert(CurrentTopicBegin);
  assert(CurrentTopicNameEnd);
  assert(NumPartitionsInTopic >= 0);

  return CurrentTopicNameEnd + int32_t(PRC::PARTITION_COUNT_SIZE) +
      (index * (int32_t(PRC::PARTITION_SIZE) + int32_t(PRC::ERROR_CODE_SIZE) +
          int32_t(PRC::OFFSET_SIZE)));
}

void TProduceResponseReader::InitCurrentTopic() {
  if ((CurrentTopicBegin + PRC::TOPIC_NAME_LEN_SIZE) > End) {
    ProduceResponseTruncated3.Increment();
    THROW_ERROR(TResponseTruncated);
  }

  int16_t topic_name_len = ReadInt16FromHeader(CurrentTopicBegin);

  if (topic_name_len == -1) {
    topic_name_len = 0;
  }

  if (topic_name_len < 0) {
    ProduceResponseBadTopicNameLength.Increment();
    THROW_ERROR(TBadTopicNameLength);
  }

  CurrentTopicNameEnd = CurrentTopicBegin + PRC::TOPIC_NAME_LEN_SIZE +
      topic_name_len;

  if ((CurrentTopicNameEnd + PRC::PARTITION_COUNT_SIZE) > End) {
    ProduceResponseTruncated4.Increment();
    THROW_ERROR(TResponseTruncated);
  }

  NumPartitionsInTopic = ReadInt32FromHeader(CurrentTopicNameEnd);

  if (NumPartitionsInTopic < 0) {
    ProduceResponseBadPartitionCount.Increment();
    THROW_ERROR(TBadPartitionCount);
  }

  CurrentPartitionIndexInTopic = -1;
}

void TProduceResponseReader::InitCurrentPartition() {
  const uint8_t *partition_end =
      GetPartitionStart(CurrentPartitionIndexInTopic + 1);

  if (partition_end > End) {
    ProduceResponseTruncated5.Increment();
    THROW_ERROR(TResponseTruncated);
  }
}
