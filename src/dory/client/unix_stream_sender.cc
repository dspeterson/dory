/* <dory/client/unix_stream_sender.cc>

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

   Implements <dory/client/unix_stream_sender.h>.
 */

#include <dory/client/unix_stream_sender.h>

#include <cassert>
#include <cstring>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <base/error_util.h>
#include <dory/client/path_too_long.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Client;

void TUnixStreamSender::DoPrepareToSend() {
  Sock = IfLt0(socket(AF_LOCAL, SOCK_STREAM, 0));
  struct sockaddr_un servaddr;
  std::memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  std::strncpy(servaddr.sun_path, Path.c_str(), Path.size());
  servaddr.sun_path[sizeof(servaddr.sun_path) - 1] = '\0';

  if (std::strcmp(Path.c_str(), servaddr.sun_path)) {
    throw TPathTooLong(Path);
  }

  IfLt0(connect(Sock, reinterpret_cast<const struct sockaddr *>(&servaddr),
      sizeof(servaddr)));
}

void TUnixStreamSender::DoSend(const uint8_t *msg, size_t msg_size) {
  IfLt0(send(Sock, msg, msg_size, MSG_NOSIGNAL));
}

void TUnixStreamSender::DoReset() {
  Sock.Reset();
}
