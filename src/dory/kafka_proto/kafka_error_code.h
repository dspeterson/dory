/* <dory/kafka_proto/kafka_error_code.h>

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

   Error codes defined by Kafka protocol.  See
   https://kafka.apache.org/protocol for more information.
 */

#pragma once

#include <cstdint>

namespace Dory {

  namespace KafkaProto {

    enum class TKafkaErrorCode : int16_t {
      Unknown = -1,
      None = 0,
      OffsetOutOfRange = 1,
      CorruptMessage = 2,
      UnknownTopicOrPartition = 3,
      InvalidFetchSize = 4,
      LeaderNotAvailable = 5,
      NotLeaderForPartition = 6,
      RequestTimedOut = 7,
      BrokerNotAvailable = 8,
      ReplicaNotAvailable = 9,
      MessageTooLarge = 10,
      StaleControllerEpoch = 11,
      OffsetMetadataTooLarge = 12,
      NetworkException = 13,
      GroupLoadInProgress = 14,
      GroupCoordinatorNotAvailable = 15,
      NotCoordinatorForGroup = 16,
      InvalidTopicException = 17,
      RecordListTooLarge = 18,
      NotEnoughReplicas = 19,
      NotEnoughReplicasAfterAppend = 20,
      InvalidRequiredAcks = 21,
      IllegalGeneration = 22,
      InconsistentGroupProtocol = 23,
      InvalidGroupId = 24,
      UnknownMemberId = 25,
      InvalidSessionTimeout = 26,
      RebalanceInProgress = 27,
      InvalidCommitOffsetSize = 28,
      TopicAuthorizationFailed = 29,
      GroupAuthorizationFailed = 30,
      ClusterAuthorizationFailed = 31,
      InvalidTimestamp = 32,
      UnsupportedSaslMechanism = 33,
      IllegalSaslState = 34,
      UnsupportedVersion = 35,
      TopicAlreadyExists = 36,
      InvalidPartitions = 37,
      InvalidReplicationFactor = 38,
      InvalidReplicaAssignment = 39,
      InvalidConfig = 40,
      NotController = 41,
      InvalidRequest = 42,
      UnsupportedForMessageFormat = 43
    };

    struct TKafkaErrorInfo {
      const char *ErrorName;

      const char *ErrorDescription;
    };  // TKafkaErrorInfo

    inline bool operator==(TKafkaErrorCode x, int16_t y) noexcept {
      return (static_cast<int16_t>(x) == y);
    }

    inline bool operator==(int16_t x, TKafkaErrorCode y) noexcept {
      return (y == x);
    }

    inline bool operator!=(TKafkaErrorCode x, int16_t y) noexcept {
      return !(x == y);
    }

    inline bool operator!=(int16_t x, TKafkaErrorCode y) noexcept {
      return !(y == x);
    }

    /* Returns information about a Kafka error code.  If 'error_code' is an
       unknown value, UndocumentedKafkaErrorInfo will be returned (see below).
     */
    const TKafkaErrorInfo &LookupKafkaErrorCode(int16_t error_code) noexcept;

    extern const TKafkaErrorInfo UndocumentedKafkaErrorInfo;

    inline bool KafkaErrorCodeIsDocumented(int16_t error_code) noexcept {
      return (&LookupKafkaErrorCode(error_code) !=
          &UndocumentedKafkaErrorInfo);
    }

  }  // KafkaProto

}  // Dory
