/* <dory/client/tcp_sender.h>

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

   Client class for sending local TCP messages to Dory.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <netinet/in.h>

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <dory/client/client_sender_base.h>

namespace Dory {

  namespace Client {

    class TTcpSender final : public TClientSenderBase {
      NO_COPY_SEMANTICS(TTcpSender);

      public:
      explicit TTcpSender(in_port_t port)
          : Port(port) {
      }

      ~TTcpSender() override = default;

      protected:
      void DoPrepareToSend() override;

      void DoSend(const uint8_t *msg, size_t msg_size) override;

      void DoReset() override;

      private:
      in_port_t Port;

      Base::TFd Sock;
    };  // TTcpSender

  }  // Client

}  // Dory
