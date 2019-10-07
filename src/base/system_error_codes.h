/* <base/system_error_codes.h>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Functions for interpreting system error codes.
 */

#pragma once

#include <algorithm>
#include <cerrno>
#include <initializer_list>
#include <system_error>

namespace Base {

  extern std::initializer_list<int> LostTcpConnectionErrorCodes;

  static inline bool LostTcpConnection(int errno_value) {
    return (std::find(LostTcpConnectionErrorCodes.begin(),
        LostTcpConnectionErrorCodes.end(), errno_value) !=
        LostTcpConnectionErrorCodes.end());
  }

  static inline bool LostTcpConnection(const std::system_error &x) {
    return LostTcpConnection(x.code().value());
  }

}  // Base
