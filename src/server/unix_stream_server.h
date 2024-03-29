/* <server/unix_stream_server.h>

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

   Class for server that uses UNIX domain stream sockets for communication with
   clients.
 */

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <base/no_copy_semantics.h>
#include <server/stream_server_base.h>

namespace Server {

  /* This may be either instantiated directly or used as a base class. */
  class TUnixStreamServer : public TStreamServerBase {
    NO_COPY_SEMANTICS(TUnixStreamServer);

    public:
    enum class TErrorReason {
      SockFileUnlinkFailed,
      BindFailed,
      SockFileChmodFailed
    };  // TErrorReason

    class TError : public std::runtime_error {
      public:
      TError(TErrorReason reason, const char *path, const char *msg)
          : std::runtime_error(BuildErrorMsg(msg, path, reason)),
            Reason(reason),
            Path(path) {
      }

      TErrorReason GetReason() const noexcept {
        return Reason;
      }

      const std::string &GetPath() const noexcept {
        return Path;
      }

      private:
      static std::string BuildErrorMsg(const char *msg, const char *path,
          TErrorReason reason);

      TErrorReason Reason;

      std::string Path;
    };  // TError

    TUnixStreamServer(int backlog, const char *path,
        std::unique_ptr<TConnectionHandlerApi> &&connection_handler);

    ~TUnixStreamServer() override = default;

    const std::string &GetPath() const noexcept {
      return Path;
    }

    const struct sockaddr_un &GetClientAddr() const noexcept {
      return ClientAddr;
    }

    /* Specify a value to chmod() the socket file to the next time it is
       created.  If unspecified, the umask determines the permission bits. */
    void SetMode(mode_t mode) noexcept;

    /* Specify that the next time the socket file is created, its mode will be
       determined by the umask.  This is the default behavior if SetMode() has
       not been called. */
    void ClearMode() noexcept {
      Mode.reset();
    }

    protected:
    void InitListeningSocket(Base::TFd &sock) override;

    void CloseListeningSocket(Base::TFd &sock) override;

    private:
    void UnlinkPath();

    const std::string Path;

    std::optional<mode_t> Mode;

    struct sockaddr_un ClientAddr;
  };  // TUnixStreamServer

}  // Server
