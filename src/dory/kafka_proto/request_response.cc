/* <dory/kafka_proto/request_response.cc>

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

   Implements <dory/kafka_proto/request_response.h>
 */

#include <dory/kafka_proto/request_response.h>

#include <cstdint>

#include <base/counter.h>
#include <base/field_access.h>
#include <dory/kafka_proto/errors.h>

using namespace Dory;
using namespace Dory::KafkaProto;

SERVER_COUNTER(BadKafkaResponseSize);

size_t Dory::KafkaProto::GetRequestOrResponseSize(
    const void *data_begin) {
  int32_t size_field = ReadInt32FromHeader(data_begin);

  if (size_field < 0) {
    BadKafkaResponseSize.Increment();
    throw TBadRequestOrResponseSize();
  }

  /* The value stored in the size field does not include the size of the size
     field itself, so we add REQUEST_OR_RESPONSE_SIZE_SIZE bytes for that. */
  return static_cast<size_t>(size_field) + REQUEST_OR_RESPONSE_SIZE_SIZE;
}
