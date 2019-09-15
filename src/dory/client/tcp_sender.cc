/* <dory/client/tcp_sender.cc>

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

   Implements <dory/client/tcp_sender.h>.
 */

#include <dory/client/tcp_sender.h>

#include <cassert>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <base/error_util.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Client;

void TTcpSender::DoPrepareToSend() {
  assert(this);
  Sock = IfLt0(socket(AF_INET, SOCK_STREAM, 0));
  struct sockaddr_in servaddr;
  std::memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(Port);
  inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
  IfLt0(connect(Sock, reinterpret_cast<const struct sockaddr *>(&servaddr),
      sizeof(servaddr)));
}

void TTcpSender::DoSend(const uint8_t *msg, size_t msg_size) {
  assert(this);
  IfLt0(send(Sock, msg, msg_size, MSG_NOSIGNAL));
}

void TTcpSender::DoReset() {
  assert(this);
  Sock.Reset();
}
