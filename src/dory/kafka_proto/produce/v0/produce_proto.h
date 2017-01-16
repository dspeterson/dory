/* <dory/kafka_proto/produce/v0/produce_proto.h>

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

   Kafka produce protocol version 0 implementation class.
 */

#pragma once

#include <dory/kafka_proto/produce/produce_protocol.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <base/no_copy_semantics.h>
#include <base/thrower.h>

namespace Dory {

  namespace KafkaProto {

    namespace Produce {

      namespace V0 {

        class TProduceProto final : public TProduceProtocol {
          NO_COPY_SEMANTICS(TProduceProto);

          public:
          TProduceProto()
              : TProduceProtocol(ComputeConstants()) {
          }

          virtual ~TProduceProto() noexcept { }

          virtual TProduceRequestWriterApi *
          CreateProduceRequestWriter() const override;

          virtual TMsgSetWriterApi *CreateMsgSetWriter() const override;

          virtual TProduceResponseReaderApi *
          CreateProduceResponseReader() const override;

          virtual TAckResultAction ProcessAck(
              int16_t ack_value) const override;

          private:
          static TConstants ComputeConstants();

          int16_t RequiredAcks;

          int32_t ReplicationTimeout;
        };  // TProduceProto

      }  // V0

    }  // Produce

  }  // KafkaProto

}  // Dory
