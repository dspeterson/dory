/* <dory/client/unix_dg_sender.cc>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/client/unix_dg_sender.h>.
 */

#include <dory/client/unix_dg_sender.h>

#include <cassert>
#include <stdexcept>

#include <base/error_utils.h>
#include <dory/client/path_too_long.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Client;

void TUnixDgSender::DoPrepareToSend() {
  assert(this);

  switch (Sock.Bind(Path.c_str())) {
    case DORY_OK: {
      break;
    }
    case DORY_CLIENT_SOCK_IS_OPENED: {
      throw std::logic_error("UNIX domain datagram socket is already opened");
    }
    case DORY_SERVER_SOCK_PATH_TOO_LONG: {
      throw TPathTooLong(Path);
    }
    default: {
      throw std::logic_error(
          "Unexpected return value from UNIX domain datagram socket bind() operation");
    }
  }
}

void TUnixDgSender::DoSend(const uint8_t *msg, size_t msg_size) {
  assert(this);
  int ret = Sock.Send(msg, msg_size);

  if (ret != DORY_OK) {
    assert(ret > 0);
    ThrowSystemError(ret);
  }
}

void TUnixDgSender::DoReset() {
  assert(this);
  Sock.Close();
}
