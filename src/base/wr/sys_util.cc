/* <base/wr/sys_util.cc>

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

   Implements <base/wr/sys_util.h>.
 */

#include <base/wr/sys_util.h>

#include <cerrno>

#include <unistd.h>

#include <base/error_util.h>

using namespace Base;

int Base::Wr::gethostname(TDisp disp, std::initializer_list<int> errors,
    char *name, size_t len) noexcept {
  const int ret = ::gethostname(name, len);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EINVAL})) {
    DieErrno("gethostname()", errno);
  }

  return ret;
}
