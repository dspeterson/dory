/* <dory/kafka_proto/kafka_error_code.cc>

   ----------------------------------------------------------------------------
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

   Implements <dory/kafka_proto/kafka_error_code.h>.
 */

#include <dory/kafka_proto/kafka_error_code.h>

#include <cstddef>

using namespace Dory;
using namespace Dory::KafkaProto;

const TKafkaErrorInfo Dory::KafkaProto::UndocumentedKafkaErrorInfo = {
  "undocumented error",
  "No information about this error is available.  See "
      "https://kafka.apache.org/protocol for the latest information on Kafka "
      "error codes."
};

static const TKafkaErrorInfo UnknownServerErrorInfo = {
  "unknown",
  "Kafka experienced an unexpected error when processing the request."
};

static const TKafkaErrorInfo KafkaErrorInfoTable[] = {
  {
    "none",
    "Success (no error occurred)."
  },
  {
    "offset out of range",
    "The requested offset is not within the range of offsets maintained by "
        "Kafka."
  },
  {
    "corrupt message",
    "This message has failed its CRC checksum, exceeds the valid size, or is "
        "otherwise corrupt."
  },
  {
    "unknown topic or partition",
    "This broker does not host this topic-partition."
  },
  {
    "invalid fetch size",
    "The requested fetch size is invalid."
  },
  {
    "leader not available",
    "There is no leader for this topic-partition as we are in the middle of a "
        "leadership election."
  },
  {
    "not leader for partition",
    "This broker is not the leader for that topic-partition."
  },
  {
    "request timed out",
    "The request timed out."
  },
  {
    "broker not available",
    "The broker is not available."
  },
  {
    "replica not available",
    "The replica is not available for the requested topic-partition."
  },
  {
    "message too large",
    "The request included a message larger than the max message size the "
        "broker will accept."
  },
  {
    "stale controller epoch",
    "The controller moved to another broker."
  },
  {
    "offset metadata too large",
    "The metadata field of the offset request was too large."
  },
  {
    "network exception",
    "The server disconnected before a response was received."
  },
  {
    "group load in progress",
    "The coordinator is loading and hence can't process requests for this "
        "group."
  },
  {
    "group coordinator not available",
    "The group coordinator is not available."
  },
  {
    "not coordinator for group",
    "This is not the correct coordinator for this group."
  },
  {
    "invalid topic exception",
    "The request attempted to perform an operation on an invalid topic."
  },
  {
    "record list too large",
    "The request included message batch larger than the configured segment "
        "size on the broker."
  },
  {
    "not enough replicas",
    "Messages are rejected since there are fewer in-sync replicas than "
        "required."
  },
  {
    "not enough replicas after append",
    "Messages are written to the log, but to fewer in-sync replicas than "
        "required."
  },
  {
    "invalid required ACKs",
    "Produce request specified an invalid value for required ACKs."
  },
  {
    "illegal generation",
    "Specified group generation ID is not valid."
  },
  {
    "inconsistent group protocol",
    "The group member's supported protocols are incompatible with those of "
        "existing members."
  },
  {
    "invalid group ID",
    "The configured groupId is invalid."
  },
  {
    "unknown member ID",
    "The coordinator is not aware of this member."
  },
  {
    "invalid session timeout",
    "The session timeout is not within the range allowed by the broker (as "
        "configured by group.min.session.timeout.ms and "
        "group.max.session.timeout.ms)."
  },
  {
    "rebalance in progress",
    "The group is rebalancing, so a rejoin is needed."
  },
  {
    "invalid commit offset size",
    "The committing offset data size is not valid."
  },
  {
    "topic authorization failed",
    "Not authorized to access topics."
  },
  {
    "group authorization failed",
    "Not authorized to access group."
  },
  {
    "cluster authorization failed",
    "Cluster authorization failed."
  },
  {
    "invalid timestamp",
    "The timestamp of the message is out of acceptable range."
  },
  {
    "unsupported SASL mechanism",
    "The broker does not support the requested SASL mechanism."
  },
  {
    "illegal SASL state",
    "Request is not valid given the current SASL state."
  },
  {
    "unsupported version",
    "The version of API is not supported."
  },
  {
    "topic already exists",
    "Topic with this name already exists."
  },
  {
    "invalid partitions",
    "Number of partitions is invalid."
  },
  {
    "invalid replication factor",
    "Replication factor is invalid."
  },
  {
    "invalid replica assignment",
    "Replica assignment is invalid."
  },
  {
    "invalid config",
    "Configuration is invalid."
  },
  {
    "not controller",
    "This is not the correct controller for this cluster."
  },
  {
    "invalid request",
    "This most likely occurs because of a request being malformed by the "
        "client library or the message was sent to an incompatible broker.  "
        "See the broker logs for more details."
  },
  {
    "unsupported for message format",
    "The message format version on the broker does not support the request."
  }
};

static const size_t KafkaErrorInfoTableSize = sizeof(KafkaErrorInfoTable) /
    sizeof(KafkaErrorInfoTable[0]);

const TKafkaErrorInfo &Dory::KafkaProto::LookupKafkaErrorCode(
    int16_t error_code) noexcept {
  if (error_code < 0) {
    return (error_code == TKafkaErrorCode::Unknown) ?
        UnknownServerErrorInfo : UndocumentedKafkaErrorInfo;
  }

  return ((error_code < 0) ||
      (static_cast<size_t>(error_code) >= KafkaErrorInfoTableSize)) ?
      UndocumentedKafkaErrorInfo : KafkaErrorInfoTable[error_code];
}
