/* <dory/kafka_proto/errors.h>

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

   Common exception definitions for kafka protocol-specific code.
 */

#pragma once

#include <stdexcept>

namespace Dory {

  namespace KafkaProto {

    class TBadRequestOrResponseSize final : public std::runtime_error {
      public:
      TBadRequestOrResponseSize()
          : std::runtime_error("Invalid Kafka wire protocol message size") {
      }

      ~TBadRequestOrResponseSize() noexcept override = default;
    };  // TBadRequestOrResponseSize

  }  // KafkaProto

}  // Dory
