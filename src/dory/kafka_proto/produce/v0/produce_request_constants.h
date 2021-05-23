/* <dory/kafka_proto/produce/v0/produce_request_constants.h>

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

   Constants related to Kafka produce protocol version 0 requests.
 */

#pragma once

namespace Dory {

  namespace KafkaProto {

    namespace Produce {

      namespace V0 {

        class TProduceRequestConstants {
          public:
          enum { API_KEY_SIZE = 2 };

          enum { API_VERSION_SIZE = 2 };

          enum { CORRELATION_ID_SIZE = 4 };

          enum { CLIENT_ID_LEN_SIZE = 2 };

          enum { REQUIRED_ACKS_SIZE = 2 };

          enum { REPLICATION_TIMEOUT_SIZE = 4 };

          enum { TOPIC_COUNT_SIZE = 4 };

          enum { TOPIC_NAME_LEN_SIZE = 2 };

          enum { PARTITION_COUNT_SIZE = 4 };

          enum { PARTITION_SIZE = 4 };

          enum { MSG_SET_SIZE_SIZE = 4 };

          enum { MSG_OFFSET_SIZE = 8 };

          enum { MSG_SIZE_SIZE = 4 };

          enum { CRC_SIZE = 4 };

          enum { MAGIC_BYTE_SIZE = 1 };

          enum { ATTRIBUTES_SIZE = 1 };

          enum { KEY_LEN_SIZE = 4 };

          enum { VALUE_LEN_SIZE = 4 };

          enum {
            MIN_MSG_SIZE = size_t(CRC_SIZE) + size_t(MAGIC_BYTE_SIZE) +
                size_t(ATTRIBUTES_SIZE) + size_t(KEY_LEN_SIZE) +
                size_t(VALUE_LEN_SIZE)
          };

          enum {
            NO_COMPRESSION_ATTR = 0,
            GZIP_COMPRESSION_ATTR = 1,
            SNAPPY_COMPRESSION_ATTR = 2,
            LZ4_COMPRESSION_ATTR = 3
          };
        };  // TProduceRequestConstants

      }  // V0

    }  // Produce

  }  // KafkaProto

}  // Dory
