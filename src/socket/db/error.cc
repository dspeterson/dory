/* <socket/db/error.cc>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   Implements <socket/db/error.h>.
 */

#include <socket/db/error.h>

#include <netdb.h>

#include <base/error_util.h>

using namespace std;
using namespace Base;
using namespace Socket::Db;

TError::TError(int error_code)
    : runtime_error(gai_strerror(error_code)) {
}

void Socket::Db::IfNe0(int error_code) {
  switch (error_code) {
    case 0: {
      break;
    }
    case EAI_SYSTEM: {
      ThrowSystemError(error_code);
      break;  // not reached
    }
    default: {
      throw TError(error_code);
    }
  }
}
