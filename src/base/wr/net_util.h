/* <base/wr/net_util.h>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Wrappers for network-related system/library calls.
 */

#pragma once

#include <cstddef>
#include <initializer_list>

#include <sys/types.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    ssize_t send(TDisp disp, std::initializer_list<int> errors, int sockfd,
        const void *buf, size_t len, int flags) noexcept;

    inline ssize_t send(int sockfd, const void *buf, size_t len,
        int flags) noexcept {
      return send(TDisp::AddFatal, {}, sockfd, buf, len, flags);
    }

    int socketpair(TDisp disp, std::initializer_list<int> errors, int domain,
        int type, int protocol, int sv[2]) noexcept;

    inline void socketpair(int domain, int type, int protocol,
        int sv[2]) noexcept {
      const int ret = socketpair(TDisp::AddFatal, {}, domain, type, protocol,
          sv);
      assert(ret == 0);
    }

  }  // Wr

}  // Base
