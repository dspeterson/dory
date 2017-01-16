/* <dory/kafka_proto/request_response.h>

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

   Common code for dealing with requests and responses.  Everything here is
   independent of API type (produce API, metadata API, etc.) and version.
 */

#pragma once

#include <cstddef>

namespace Dory {

  namespace KafkaProto {

    /* The size of the first field in a request or response, which is a
       (signed) integer giving the size in bytes of the rest of the request or
       response. */
    const size_t REQUEST_OR_RESPONSE_SIZE_SIZE = 4;

    /* Paramater 'data_begin' points to a buffer containing a partial or
       complete request or response.  It is assumed that the buffer contains at
       least the first REQUEST_OR_RESPONSE_SIZE_SIZE bytes of the request or
       response.  Return the size in bytes of the entire request or response.
       Throw TBadRequestOrResponseSize if the response size obtained from the
       buffer is invalid. */
    size_t GetRequestOrResponseSize(const void *data_begin);

  }  // KafkaProto

}  // Dory
