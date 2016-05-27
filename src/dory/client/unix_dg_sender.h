/* <dory/client/unix_dg_sender.h>

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

   Client class for sending UNIX domain datagram messages to Dory.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <base/no_copy_semantics.h>
#include <dory/client/client_sender_base.h>
#include <dory/client/dory_client_socket.h>

namespace Dory {

  namespace Client {

    class TUnixDgSender final : public TClientSenderBase {
      NO_COPY_SEMANTICS(TUnixDgSender);

      public:
      explicit TUnixDgSender(const char *path)
          : Path(path) {
      }

      virtual ~TUnixDgSender() noexcept {
      }

      protected:
      virtual void DoPrepareToSend();

      virtual void DoSend(const uint8_t *msg, size_t msg_size);

      virtual void DoReset();

      private:
      std::string Path;

      TDoryClientSocket Sock;
    };  // TUnixDgSender

  }  // Client

}  // Dory
