/* <dory/kafka_proto/produce/produce_protocol.h>

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
   of the Kafka producer wire format.  Derived classes implement specific
   versions, and the core dory server code interacts with a base class
   instance to insulate itself from version-specific wire format details.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <base/no_copy_semantics.h>
#include <dory/kafka_proto/produce/msg_set_writer_api.h>
#include <dory/kafka_proto/produce/produce_request_writer_api.h>
#include <dory/kafka_proto/produce/produce_response_reader_api.h>

namespace Dory {

  namespace KafkaProto {

    namespace Produce {

      class TProduceProtocol {
        NO_COPY_SEMANTICS(TProduceProtocol);

        public:
        enum class TAckResultAction {
          Ok,
          Resend,
          Discard,
          Pause,
          DiscardAndPause
        };

        virtual ~TProduceProtocol() = default;

        /* Return the number of bytes in a message set containing a single
           empty message (i.e. empty key and value). */
        size_t GetSingleMsgOverhead() const {
          return Constants.SingleMsgOverhead;
        }

        /* Return a pointer to a newly created produce request writer object.
           Caller assumes responsibility for deleting object. */
        virtual std::unique_ptr<TProduceRequestWriterApi>
        CreateProduceRequestWriter() const = 0;

        /* Return a pointer to a newly created message set writer object.
           Caller assumes responsibility for deleting object. */
        virtual std::unique_ptr<TMsgSetWriterApi>
        CreateMsgSetWriter() const = 0;

        /* Return a pointer to a newly created produce response reader object.
           Caller assumes responsibility for deleting object. */
        virtual std::unique_ptr<TProduceResponseReaderApi>
        CreateProduceResponseReader() const = 0;

        virtual TAckResultAction ProcessAck(int16_t ack_value) const = 0;

        protected:
        struct TConstants {
          /* This is the number of bytes of overhead for a single message in a
             produce request. */
          size_t SingleMsgOverhead;
        };

        explicit TProduceProtocol(const TConstants &constants)
            : Constants(constants) {
        }

        private:
        const TConstants Constants;
      };  // TProduceProtocol

    }  // Produce

  }  // KafkaProto

}  // Dory
