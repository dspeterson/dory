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

#include <base/counter.h>
#include <base/no_default_case.h>
#include <dory/kafka_proto/kafka_error_code.h>
#include <dory/kafka_proto/produce/v0/msg_set_writer.h>
#include <dory/kafka_proto/produce/v0/produce_request_constants.h>
#include <dory/kafka_proto/produce/v0/produce_request_writer.h>
#include <dory/kafka_proto/produce/v0/produce_response_reader.h>
#include <log/log.h>

using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::KafkaProto::Produce::V0;
using namespace Log;

DEFINE_COUNTER(AckErrorBrokerNotAvailable);
DEFINE_COUNTER(AckErrorClusterAuthorizationFailed);
DEFINE_COUNTER(AckErrorCorruptMessage);
DEFINE_COUNTER(AckErrorGroupAuthorizationFailed);
DEFINE_COUNTER(AckErrorGroupCoordinatorNotAvailable);
DEFINE_COUNTER(AckErrorGroupLoadInProgress);
DEFINE_COUNTER(AckErrorIllegalGeneration);
DEFINE_COUNTER(AckErrorIllegalSaslState);
DEFINE_COUNTER(AckErrorInconsistentGroupProtocol);
DEFINE_COUNTER(AckErrorInvalidCommitOffsetSize);
DEFINE_COUNTER(AckErrorInvalidConfig);
DEFINE_COUNTER(AckErrorInvalidFetchSize);
DEFINE_COUNTER(AckErrorInvalidGroupId);
DEFINE_COUNTER(AckErrorInvalidPartitions);
DEFINE_COUNTER(AckErrorInvalidReplicaAssignment);
DEFINE_COUNTER(AckErrorInvalidReplicationFactor);
DEFINE_COUNTER(AckErrorInvalidRequest);
DEFINE_COUNTER(AckErrorInvalidRequiredAcks);
DEFINE_COUNTER(AckErrorInvalidSessionTimeout);
DEFINE_COUNTER(AckErrorInvalidTimestamp);
DEFINE_COUNTER(AckErrorInvalidTopicException);
DEFINE_COUNTER(AckErrorLeaderNotAvailable);
DEFINE_COUNTER(AckErrorMessageTooLarge);
DEFINE_COUNTER(AckErrorNetworkException);
DEFINE_COUNTER(AckErrorNotController);
DEFINE_COUNTER(AckErrorNotCoordinatorForGroup);
DEFINE_COUNTER(AckErrorNotEnoughReplicas);
DEFINE_COUNTER(AckErrorNotEnoughReplicasAfterAppend);
DEFINE_COUNTER(AckErrorNotLeaderForPartition);
DEFINE_COUNTER(AckErrorOffsetMetadataTooLarge);
DEFINE_COUNTER(AckErrorOffsetOutOfRange);
DEFINE_COUNTER(AckErrorRebalanceInProgress);
DEFINE_COUNTER(AckErrorRecordListTooLarge);
DEFINE_COUNTER(AckErrorReplicaNotAvailable);
DEFINE_COUNTER(AckErrorRequestTimedOut);
DEFINE_COUNTER(AckErrorStaleControllerEpoch);
DEFINE_COUNTER(AckErrorTopicAlreadyExists);
DEFINE_COUNTER(AckErrorTopicAuthorizationFailed);
DEFINE_COUNTER(AckErrorUndocumented);
DEFINE_COUNTER(AckErrorUnknown);
DEFINE_COUNTER(AckErrorUnknownMemberId);
DEFINE_COUNTER(AckErrorUnknownTopicOrPartition);
DEFINE_COUNTER(AckErrorUnsupportedForMessageFormat);
DEFINE_COUNTER(AckErrorUnsupportedSaslMechanism);
DEFINE_COUNTER(AckErrorUnsupportedVersion);
DEFINE_COUNTER(AckOk);

std::unique_ptr<TProduceRequestWriterApi>
TProduceProto::CreateProduceRequestWriter() const {
  return std::unique_ptr<TProduceRequestWriterApi>(new TProduceRequestWriter);
}

std::unique_ptr<TMsgSetWriterApi>
TProduceProto::CreateMsgSetWriter() const {
  return std::unique_ptr<TMsgSetWriterApi>(new TMsgSetWriter);
}

std::unique_ptr<TProduceResponseReaderApi>
TProduceProto::CreateProduceResponseReader() const {
  return std::unique_ptr<TProduceResponseReaderApi>(
      new TProduceResponseReader);
}

TProduceProtocol::TAckResultAction
TProduceProto::ProcessAck(int16_t ack_value) const {
  /* See https://kafka.apache.org/protocol for documentation on the error codes
     below. */
  switch (static_cast<TKafkaErrorCode>(ack_value)) {
    case TKafkaErrorCode::Unknown: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorUnknown.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::None: {
      AckOk.Increment();
      break;  // successful ACK
    }
    case TKafkaErrorCode::OffsetOutOfRange: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorOffsetOutOfRange.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::CorruptMessage: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorCorruptMessage.Increment();
      return TAckResultAction::Resend;
    }
    case TKafkaErrorCode::UnknownTopicOrPartition: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
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
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidFetchSize.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::LeaderNotAvailable: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorLeaderNotAvailable.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::NotLeaderForPartition: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorNotLeaderForPartition.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::RequestTimedOut: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorRequestTimedOut.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::BrokerNotAvailable: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorBrokerNotAvailable.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::ReplicaNotAvailable: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorReplicaNotAvailable.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::MessageTooLarge: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorMessageTooLarge.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::StaleControllerEpoch: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorStaleControllerEpoch.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::OffsetMetadataTooLarge: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorOffsetMetadataTooLarge.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NetworkException: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorNetworkException.Increment();
      return TAckResultAction::Pause;
    }
    case TKafkaErrorCode::GroupLoadInProgress: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorGroupLoadInProgress.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::GroupCoordinatorNotAvailable: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorGroupCoordinatorNotAvailable.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotCoordinatorForGroup: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorNotCoordinatorForGroup.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidTopicException: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidTopicException.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::RecordListTooLarge: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorRecordListTooLarge.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotEnoughReplicas: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorNotEnoughReplicas.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotEnoughReplicasAfterAppend: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorNotEnoughReplicasAfterAppend.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidRequiredAcks: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidRequiredAcks.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::IllegalGeneration: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorIllegalGeneration.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InconsistentGroupProtocol: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInconsistentGroupProtocol.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidGroupId: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidGroupId.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnknownMemberId: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorUnknownMemberId.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidSessionTimeout: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidSessionTimeout.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::RebalanceInProgress: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorRebalanceInProgress.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidCommitOffsetSize: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidCommitOffsetSize.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::TopicAuthorizationFailed: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorTopicAuthorizationFailed.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::GroupAuthorizationFailed: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorGroupAuthorizationFailed.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::ClusterAuthorizationFailed: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorClusterAuthorizationFailed.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidTimestamp: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidTimestamp.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnsupportedSaslMechanism: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorUnsupportedSaslMechanism.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::IllegalSaslState: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorIllegalSaslState.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnsupportedVersion: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorUnsupportedVersion.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::TopicAlreadyExists: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorTopicAlreadyExists.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidPartitions: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidPartitions.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidReplicationFactor: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidReplicationFactor.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidReplicaAssignment: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidReplicaAssignment.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidConfig: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidConfig.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::NotController: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorNotController.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::InvalidRequest: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorInvalidRequest.Increment();
      return TAckResultAction::Discard;
    }
    case TKafkaErrorCode::UnsupportedForMessageFormat: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorUnsupportedForMessageFormat.Increment();
      return TAckResultAction::Discard;
    }
    default: {
      const auto &error_info = LookupKafkaErrorCode(ack_value);
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Kafka ACK returned error (" << error_info.ErrorName << "): "
          << error_info.ErrorDescription;
      AckErrorUndocumented.Increment();
      return TAckResultAction::Discard;
    }
  }

  return TAckResultAction::Ok;
}

TProduceProtocol::TConstants TProduceProto::ComputeConstants() {
  using PRC = TProduceRequestConstants;
  return {PRC::MSG_OFFSET_SIZE + PRC::MSG_SIZE_SIZE +
      PRC::CRC_SIZE + PRC::MAGIC_BYTE_SIZE + PRC::ATTRIBUTES_SIZE +
      PRC::KEY_LEN_SIZE + PRC::VALUE_LEN_SIZE};
}
