/* <dory/client/status_codes.h>

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

   Status codes for Dory client library.
 */

#pragma once

/* Status codes for library calls. */
enum {
  /* Success. */
  DORY_OK = 0,

  /* Supplied output buffer does not have enough space for result. */
  DORY_BUF_TOO_SMALL = -1,

  /* Kafka topic is too large. */
  DORY_TOPIC_TOO_LARGE = -2,

  /* Result message would exceed maximum possible size. */
  DORY_MSG_TOO_LARGE = -3,

  /* Client socket is already opened. */
  DORY_CLIENT_SOCK_IS_OPENED = -4,

  /* Pathname of Dory server socket is too long. */
  DORY_SERVER_SOCK_PATH_TOO_LONG = -6
};
