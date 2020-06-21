/* <socket/named_unix_socket.h>

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

   C++ wrapper for a UNIX domain socket with a pathname bound to it.
 */

#pragma once

#include <string>

#include <base/fd.h>
#include <base/no_copy_semantics.h>

namespace Socket {

  class TAddress;
  class TNamedUnixSocket;

  void Bind(TNamedUnixSocket &socket, const TAddress &address);

  class TNamedUnixSocket final {
    NO_COPY_SEMANTICS(TNamedUnixSocket);

    public:
    TNamedUnixSocket(int type, int protocol);

    ~TNamedUnixSocket() {
      Reset();
    }

    const Base::TFd &GetFd() const noexcept {
      return Fd;
    }

    const std::string &GetPath() const noexcept {
      return Path;
    }

    operator int() const noexcept {
      return Fd;
    }

    bool IsBound() const noexcept {
      return !Path.empty();
    }

    bool IsOpen() const noexcept {
      return Fd.IsOpen();
    }

    void Reset() noexcept;

    private:
    Base::TFd Fd;

    std::string Path;

    /* See <socket/address.h>. */
    friend void ::Socket::Bind(TNamedUnixSocket &socket,
        const TAddress &address);
  };  // TNamedUnixSocket

}  // Socket
