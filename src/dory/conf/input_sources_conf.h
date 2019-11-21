/* <dory/conf/input_sources_conf.h>

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

   Class representing input sources configuration obtained from Dory's config
   file.
 */

#pragma once

#include <cassert>
#include <string>

#include <netinet/in.h>
#include <sys/types.h>

#include <base/opt.h>
#include <dory/conf/conf_error.h>

namespace Dory {

  namespace Conf {

    class TInputSourcesRelativeUnixDgPath final : public TConfError {
      public:
      TInputSourcesRelativeUnixDgPath()
          : TConfError("UNIX datagram socket path must be absolute") {
      }
    };  // TInputSourcesRelativeUnixDgPath

    class TInputSourcesInvalidUnixDgFileMode final : public TConfError {
      public:
      TInputSourcesInvalidUnixDgFileMode()
          : TConfError("Invalid UNIX datagram socket mode") {
      }
    };  // TInputSourcesInvalidUnixDgFileMode

    class TInputSourcesRelativeUnixStreamPath final : public TConfError {
      public:
      TInputSourcesRelativeUnixStreamPath()
          : TConfError("UNIX stream socket path must be absolute") {
      }
    };  // TInputSourcesRelativeUnixStreamPath

    class TInputSourcesInvalidUnixStreamFileMode final : public TConfError {
      public:
      TInputSourcesInvalidUnixStreamFileMode()
          : TConfError("Invalid UNIX stream socket mode") {
      }
    };  // TInputSourcesInvalidUnixStreamFileMode

    struct TInputSourcesConf final {
      /* Absolute path for UNIX datagram socket.  Empty means disable. */
      std::string UnixDgPath;

      /* File creation mode for UNIX datagram socket. */
      mode_t UnixDgMode = 0222;

      /* Absolute path for UNIX stream socket.  Empty means disable. */
      std::string UnixStreamPath;

      /* File creation mode for UNIX stream socket. */
      mode_t UnixStreamMode = 0222;

      /* Optional port for local TCP input. */
      Base::TOpt<in_port_t> LocalTcpPort;

      void SetUnixDgConf(const std::string &path, mode_t mode);

      void SetUnixStreamConf(const std::string &path, mode_t mode);

      void SetTcpConf(const Base::TOpt<in_port_t> &port);
    };  // TInputSourcesConf

  }  // Conf

}  // Dory