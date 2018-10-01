/* <dory/kafka_proto/metadata/v0/metadata_proto.cc>

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

   Implements <dory/kafka_proto/metadata/v0/metadata_proto.h>.
 */

#include <dory/kafka_proto/metadata/v0/metadata_proto.h>

#include <cassert>
#include <cstring>
#include <string>

#include <syslog.h>

#include <dory/kafka_proto/kafka_error_code.h>
#include <dory/kafka_proto/metadata/v0/metadata_request_writer.h>
#include <dory/kafka_proto/metadata/v0/metadata_response_reader.h>
#include <dory/metadata.h>
#include <dory/util/time_util.h>
#include <server/counter.h>

using namespace Dory;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Metadata;
using namespace Dory::KafkaProto::Metadata::V0;
using namespace Dory::Util;

SERVER_COUNTER(TopicAutocreateGotErrorResponse);
SERVER_COUNTER(TopicAutocreateNoTopicInResponse);
SERVER_COUNTER(TopicAutocreateSuccess);
SERVER_COUNTER(TopicAutocreateUnexpectedTopicInResponse);

void TMetadataProto::WriteAllTopicsMetadataRequest(
    std::vector<uint8_t> &result, int32_t correlation_id) const {
  assert(this);
  TMetadataRequestWriter().WriteAllTopicsRequest(result, correlation_id);
}

void TMetadataProto::WriteSingleTopicMetadataRequest(
    std::vector<uint8_t> &result, const char *topic,
    int32_t correlation_id) const {
  assert(this);
  TMetadataRequestWriter().WriteSingleTopicRequest(result, topic,
      topic + std::strlen(topic), correlation_id);
}

static inline bool CanSendToPartition(int16_t error_code) {
  /* Note: If a replica is not available, it is still OK to send to the leader.
   */
  return (error_code == TKafkaErrorCode::None) ||
      (error_code == TKafkaErrorCode::ReplicaNotAvailable);
}

std::unique_ptr<TMetadata> TMetadataProto::BuildMetadataFromResponse(
    const void *response_buf, size_t response_buf_size) const {
  assert(this);
  TMetadata::TBuilder builder;
  TMetadataResponseReader reader(response_buf, response_buf_size);
  std::string name;
  builder.OpenBrokerList();

  while (reader.NextBroker()) {
    name.assign(reader.GetCurrentBrokerHostBegin(),
        reader.GetCurrentBrokerHostEnd());
    builder.AddBroker(reader.GetCurrentBrokerNodeId(), std::move(name),
        reader.GetCurrentBrokerPort());
  }

  builder.CloseBrokerList();

  while (reader.NextTopic()) {
    if (reader.GetCurrentTopicErrorCode()) {
      continue;
    }

    name.assign(reader.GetCurrentTopicNameBegin(),
        reader.GetCurrentTopicNameEnd());

    /* If OpenTopic() returns false, we got a duplicate topic.  In this case,
       the builder logs a warning and we ignore the topic. */
    if (builder.OpenTopic(name)) {
      while (reader.NextPartitionInTopic()) {
        int16_t error_code = reader.GetCurrentPartitionErrorCode();
        builder.AddPartitionToTopic(reader.GetCurrentPartitionId(),
            reader.GetCurrentPartitionLeaderId(),
            CanSendToPartition(error_code), error_code);
      }

      builder.CloseTopic();
    }
  }

  return builder.Build();
}

bool TMetadataProto::TopicAutocreateWasSuccessful(const char *topic,
    const void *response_buf, size_t response_buf_size) const {
  assert(this);
  TMetadataResponseReader reader(response_buf, response_buf_size);

  if (!reader.NextTopic()) {
    TopicAutocreateNoTopicInResponse.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(30));

    if (lim.Test()) {
      syslog(LOG_ERR,
          "Autocreate for topic [%s] failed: no topic in metadata response",
          topic);
    }

    return false;
  }

  std::string response_topic(reader.GetCurrentTopicNameBegin(),
      reader.GetCurrentTopicNameEnd());

  if (response_topic != topic) {
    TopicAutocreateUnexpectedTopicInResponse.Increment();
    syslog(LOG_ERR,
        "Autocreate for topic [%s] failed: unexpected topic [%s] in metadata response",
        topic, response_topic.c_str());
    return false;
  }

  int16_t error_code = reader.GetCurrentTopicErrorCode();

  /* We expect to see "leader not available" when the topic was successfully
     created.  An error code of "none" probably means that the topic was
     already created by some other Kafka client (perhaps a Dory instance
     running on another host) since we last updated our metadata. */
  if ((error_code != TKafkaErrorCode::None) &&
      (error_code != TKafkaErrorCode::LeaderNotAvailable)) {
    TopicAutocreateGotErrorResponse.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(30));

    if (lim.Test()) {
      syslog(LOG_ERR, "Autocreate for topic [%s] failed: got error code %d",
          topic, static_cast<int>(error_code));
    }

    return false;
  }

  TopicAutocreateSuccess.Increment();
  return true;
}
