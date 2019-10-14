/* <socket/named_unix_socket.cc>

   ----------------------------------------------------------------------------
   Copyright 2013 if(we)

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

   Implements <socket/named_unix_socket.h>.
 */

#include <socket/named_unix_socket.h>

#include <sys/socket.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/wr/file_util.h>
#include <base/wr/net_util.h>

using namespace Base;
using namespace Socket;

TNamedUnixSocket::TNamedUnixSocket(int type, int protocol)
    : Fd(IfLt0(Wr::socket(AF_LOCAL, type, protocol))) {
}

void TNamedUnixSocket::Reset() noexcept {
  assert(this);
  Fd.Reset();

  if (!Path.empty()) {
    Wr::unlink(Path.c_str());
    Path.clear();
  }
}
