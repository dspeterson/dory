/* <dory/kafka_proto/produce/v0/produce_proto.cc>

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

   Implements <dory/kafka_proto/produce/v0/produce_proto.h>.
 */

#include <dory/kafka_proto/produce/v0/produce_proto.h>

#include <cassert>

#include <syslog.h>

#include <base/no_default_case.h>
#include <dory/kafka_proto/kafka_error_code.h>
#include <dory/kafka_proto/produce/v0/msg_set_writer.h>
#include <dory/kafka_proto/produce/v0/produce_request_constants.h>
#include <dory/kafka_proto/produce/v0/produce_request_writer.h>
#include <dory/kafka_proto/produce/v0/produce_response_reader.h>
#include <dory/util/time_util.h>
#include <server/counter.h>

using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::KafkaProto::Produce::V0;
using namespace Dory::Util;

SERVER_COUNTER(AckErrorBrokerNotAvailable);
SERVER_COUNTER(AckErrorClusterAuthorizationFailed);
SERVER_COUNTER(AckErrorCorruptMessage);
SERVER_COUNTER(AckErrorGroupAuthorizationFailed);
SERVER_COUNTER(AckErrorGroupCoordinatorNotAvailable);
SERVER_COUNTER(AckErrorGroupLoadInProgress);
SERVER_COUNTER(AckErrorIllegalGeneration);
SERVER_COUNTER(AckErrorIllegalSaslState);
SERVER_COUNTER(AckErrorInconsistentGroupProtocol);
SERVER_COUNTER(AckErrorInvalidCommitOffsetSize);
SERVER_COUNTER(AckErrorInvalidConfig);
SERVER_COUNTER(AckErrorInvalidFetchSize);
SERVER_COUNTER(AckErrorInvalidGroupId);
SERVER_COUNTER(AckErrorInvalidPartitions);
SERVER_COUNTER(AckErrorInvalidReplicaAssignment);
SERVER_COUNTER(AckErrorInvalidReplicationFactor);
SERVER_COUNTER(AckErrorInvalidRequest);
SERVER_COUNTER(AckErrorInvalidRequiredAcks);
SERVER_COUNTER(AckErrorInvalidSessionTimeout);
SERVER_COUNTER(AckErrorInvalidTimestamp);
SERVER_COUNTER(AckErrorInvalidTopicException);
SERVER_COUNTER(AckErrorLeaderNotAvailable);
SERVER_COUNTER(AckErrorMessageTooLarge);
SERVER_COUNTER(AckErrorNetworkException);
SERVER_COUNTER(AckErrorNotController);
SERVER_COUNTER(AckErrorNotCoordinatorForGroup);
SERVER_COUNTER(AckErrorNotEnoughReplicas);
SERVER_COUNTER(AckErrorNotEnoughReplicasAfterAppend);
SERVER_COUNTER(AckErrorNotLeaderForPartition);
SERVER_COUNTER(AckErrorOffsetMetadataTooLarge);
SERVER_COUNTER(AckErrorOffsetOutOfRange);
SERVER_COUNTER(AckErrorRebalanceInProgress);
SERVER_COUNTER(AckErrorRecordListTooLarge);
SERVER_COUNTER(AckErrorReplicaNotAvailable);
SERVER_COUNTER(AckErrorRequestTimedOut);
SERVER_COUNTER(AckErrorStaleControllerEpoch);
SERVER_COUNTER(AckErrorTopicAlreadyExists);
SERVER_COUNTER(AckErrorTopicAuthorizationFailed);
SERVER_COUNTER(AckErrorUndocumented);
SERVER_COUNTER(AckErrorUnknown);
SERVER_COUNTER(AckErrorUnknownMemberId);
SERVER_COUNTER(AckErrorUnknownTopicOrPartition);
SERVER_COUNTER(AckErrorUnsupportedForMessageFormat);
SERVER_COUNTER(AckErrorUnsupportedSaslMechanism);
SERVER_COUNTER(AckErrorUnsupportedVersion);
SERVER_COUNTER(AckOk);

TProduceRequestWriterApi *
TProduceProto::CreateProduceRequestWriter() const {
  assert(this);
  return new TProduceRequestWriter;
}

TMsgSetWriterApi *
TProduceProto::CreateMsgSetWriter() const {
  assert(this);
  return new TMsgSetWriter;
}

TProduceResponseReaderApi *
TProduceProto::CreateProduceResponseReader() const {
  assert(this);
  return new TProduceResponseReader;
}

static void MaybeLogError(bool log_error, int16_t ack_value) {
  if (log_error) {
    const auto &error_info = LookupKafkaErrorCode(ack_value);
    syslog(LOG_ERR, "Kafka ACK returned error %d (%s): %s",
        static_cast<int>(ack_value), error_info.ErrorName,
        error_info.ErrorDescription);
  }
}

TProduceProtocol::TAckResultAction
TProduceProto::ProcessAck(int16_t ack_value) const {
  assert(this);

  /* See https://kafka.apache.org/protocol for documentation on the error codes
     below. */
  switch (static_cast<TKafkaErrorCode>(ack_value)) {
    case TKafkaErrorCode::Unknown: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUnknown.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::None: {
      AckOk.Increment();
      break;  // successful ACK
    }
    case TKafkaErrorCode::OffsetOutOfRange: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorOffsetOutOfRange.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::CorruptMessage: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorCorruptMessage.Increment();
      return TAckResultAction::Resend;
    }
    case TKafkaErrorCode::UnknownTopicOrPartition: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUnknownTopicOrPartition.Increment();

      /* This error may occur in cases where a reconfiguration of the Kafka
         cluster is being performed that involves moving partitions from one
         broker to another.  In this case, we want to reroute rather than
         discard so the messages are redirected to a valid destination broker.
         In the case where the topic no longer exists, the router thread will
         discard the messages during rerouting. */
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::InvalidFetchSize: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidFetchSize.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::LeaderNotAvailable: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorLeaderNotAvailable.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::NotLeaderForPartition: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorNotLeaderForPartition.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::RequestTimedOut: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorRequestTimedOut.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::BrokerNotAvailable: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorBrokerNotAvailable.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::ReplicaNotAvailable: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorReplicaNotAvailable.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::MessageTooLarge: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorMessageTooLarge.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::StaleControllerEpoch: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorStaleControllerEpoch.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::OffsetMetadataTooLarge: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorOffsetMetadataTooLarge.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NetworkException: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorNetworkException.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::GroupLoadInProgress: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorGroupLoadInProgress.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::GroupCoordinatorNotAvailable: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorGroupCoordinatorNotAvailable.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotCoordinatorForGroup: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorNotCoordinatorForGroup.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidTopicException: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidTopicException.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::RecordListTooLarge: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorRecordListTooLarge.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotEnoughReplicas: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorNotEnoughReplicas.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotEnoughReplicasAfterAppend: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorNotEnoughReplicasAfterAppend.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidRequiredAcks: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidRequiredAcks.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::IllegalGeneration: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorIllegalGeneration.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InconsistentGroupProtocol: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInconsistentGroupProtocol.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidGroupId: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidGroupId.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnknownMemberId: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUnknownMemberId.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidSessionTimeout: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidSessionTimeout.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::RebalanceInProgress: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorRebalanceInProgress.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidCommitOffsetSize: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidCommitOffsetSize.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::TopicAuthorizationFailed: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorTopicAuthorizationFailed.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::GroupAuthorizationFailed: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorGroupAuthorizationFailed.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::ClusterAuthorizationFailed: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorClusterAuthorizationFailed.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidTimestamp: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidTimestamp.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnsupportedSaslMechanism: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUnsupportedSaslMechanism.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::IllegalSaslState: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorIllegalSaslState.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnsupportedVersion: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUnsupportedVersion.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::TopicAlreadyExists: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorTopicAlreadyExists.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidPartitions: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidPartitions.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidReplicationFactor: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidReplicationFactor.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidReplicaAssignment: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidReplicaAssignment.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidConfig: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidConfig.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotController: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorNotController.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidRequest: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorInvalidRequest.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnsupportedForMessageFormat: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUnsupportedForMessageFormat.Increment();
      return TAckResultAction::Discard;
    }
    default: {
      static TLogRateLimiter lim(std::chrono::seconds(30));
      MaybeLogError(lim.Test(), ack_value);
      AckErrorUndocumented.Increment();
      return TAckResultAction::Discard;
    }
  }

  return TAckResultAction::Ok;
}

TProduceProtocol::TConstants TProduceProto::ComputeConstants() {
  using PRC = TProduceRequestConstants;
  TConstants constants;
  constants.SingleMsgOverhead = PRC::MSG_OFFSET_SIZE + PRC::MSG_SIZE_SIZE +
      PRC::CRC_SIZE + PRC::MAGIC_BYTE_SIZE + PRC::ATTRIBUTES_SIZE +
      PRC::KEY_LEN_SIZE + PRC::VALUE_LEN_SIZE;
  return constants;
}
