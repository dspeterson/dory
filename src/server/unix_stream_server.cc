/* <server/unix_stream_server.cc>

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

   Implements <server/unix_stream_server.h>.
 */

#include <server/unix_stream_server.h>

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>

#include <base/error_util.h>
#include <base/wr/file_util.h>
#include <base/wr/net_util.h>

using namespace Base;
using namespace Server;

TUnixStreamServer::TUnixStreamServer(int backlog, const char *path,
    std::unique_ptr<TConnectionHandlerApi> &&connection_handler)
    : TStreamServerBase(backlog,
          reinterpret_cast<struct sockaddr *>(&ClientAddr), sizeof(ClientAddr),
          std::move(connection_handler)),
      Path(path) {
  if (std::strlen(path) >= sizeof(ClientAddr.sun_path)) {
    ThrowSystemError(ENAMETOOLONG);
  }
}

void TUnixStreamServer::SetMode(mode_t mode) noexcept {
  assert(this);

  if (Mode.IsKnown()) {
    *Mode = mode;
  } else {
    Mode.MakeKnown(mode);
  }
}

void TUnixStreamServer::InitListeningSocket(TFd &sock) {
  assert(this);
  TFd sock_fd(IfLt0(Wr::socket(AF_LOCAL, SOCK_STREAM, 0)));
  struct sockaddr_un serv_addr;
  std::memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_LOCAL;
  std::strncpy(serv_addr.sun_path, Path.c_str(), sizeof(serv_addr.sun_path));
  serv_addr.sun_path[sizeof(serv_addr.sun_path) - 1] = '\0';

  /* Make sure socket file doesn't already exist. */
  UnlinkPath();

  IfLt0(Wr::bind(sock_fd, reinterpret_cast<const sockaddr *>(&serv_addr),
      sizeof(serv_addr)));
  sock = std::move(sock_fd);

  /* Set the permission bits on the socket file if they have been specified.
     If unspecified, the umask determines the permission bits. */
  if (Mode.IsKnown()) {
    IfLt0(Wr::chmod(Path.c_str(), *Mode));
  }
}

void TUnixStreamServer::CloseListeningSocket(TFd &sock) {
  TStreamServerBase::CloseListeningSocket(sock);
  UnlinkPath();
}

void TUnixStreamServer::UnlinkPath() {
  assert(this);
  int ret = Wr::unlink(Path.c_str());

  if ((ret < 0) && (errno != ENOENT)) {
    ThrowSystemError(errno);
  }
}
