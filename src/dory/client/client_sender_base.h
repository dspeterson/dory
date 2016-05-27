/* <dory/client/client_sender_base.h>

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

   Abstract base class for sending messages to Dory.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <base/no_copy_semantics.h>

namespace Dory {

  namespace Client {

    class TClientSenderBase {
      NO_COPY_SEMANTICS(TClientSenderBase);

      public:
      virtual ~TClientSenderBase() noexcept {
      }

      void PrepareToSend() {
        assert(this);
        DoPrepareToSend();
      }

      void Send(const void *msg, size_t msg_size) {
        assert(this);
        DoSend(reinterpret_cast<const uint8_t *>(msg), msg_size);
      }

      void Reset() {
        assert(this);
        DoReset();
      }

      protected:
      TClientSenderBase() = default;

      virtual void DoPrepareToSend() = 0;

      virtual void DoSend(const uint8_t *msg, size_t msg_size) = 0;

      virtual void DoReset() = 0;
    };  // TClientSenderBase

  }  // Client

}  // Dory
