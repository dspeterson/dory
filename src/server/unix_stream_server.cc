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
#include <base/no_default_case.h>
#include <base/wr/file_util.h>
#include <base/wr/net_util.h>

using namespace Base;
using namespace Server;

static const char *ReasonToString(
    TUnixStreamServer::TErrorReason reason) noexcept {
  switch (reason) {
    case TUnixStreamServer::TErrorReason::SockFileUnlinkFailed: {
      break;
    }
    case TUnixStreamServer::TErrorReason::BindFailed: {
      return "socket bind() failed";
    }
    case TUnixStreamServer::TErrorReason::SockFileChmodFailed: {
      return "socket file chmod() failed";
    }
    NO_DEFAULT_CASE;
  }

  return "socket file unlink() failed";
}

std::string TUnixStreamServer::TError::BuildErrorMsg(const char *msg,
    const char *path, TErrorReason reason) {
  std::string result("UNIX domain stream socket server error (reason: ");
  result += ReasonToString(reason);
  result += "): path [";
  result += path;
  result += "], details: ";
  result += msg;
  return result;
}

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
  Mode.emplace(mode);
}

void TUnixStreamServer::InitListeningSocket(TFd &sock) {
  TFd sock_fd(IfLt0(Wr::socket(AF_LOCAL, SOCK_STREAM, 0)));
  struct sockaddr_un serv_addr;
  std::memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_LOCAL;
  std::strncpy(serv_addr.sun_path, Path.c_str(), sizeof(serv_addr.sun_path));
  serv_addr.sun_path[sizeof(serv_addr.sun_path) - 1] = '\0';

  try {
    /* Make sure socket file doesn't already exist. */
    UnlinkPath();
  } catch (const std::exception &x) {
    throw TError(TErrorReason::SockFileUnlinkFailed, Path.c_str(), x.what());
  }

  try {
    IfLt0(Wr::bind(sock_fd, reinterpret_cast<const sockaddr *>(&serv_addr),
        sizeof(serv_addr)));
  } catch (const std::exception &x) {
    throw TError(TErrorReason::BindFailed, Path.c_str(), x.what());
  }

  sock = std::move(sock_fd);

  /* Set the permission bits on the socket file if they have been specified.
     If unspecified, the umask determines the permission bits. */
  if (Mode) {
    try {
      IfLt0(Wr::chmod(Path.c_str(), *Mode));
    } catch (const std::exception &x) {
      throw TError(TErrorReason::SockFileChmodFailed, Path.c_str(), x.what());
    }
  }
}

void TUnixStreamServer::CloseListeningSocket(TFd &sock) {
  TStreamServerBase::CloseListeningSocket(sock);
  UnlinkPath();
}

void TUnixStreamServer::UnlinkPath() {
  int ret = Wr::unlink(Path.c_str());

  if ((ret < 0) && (errno != ENOENT)) {
    ThrowSystemError(errno);
  }
}
