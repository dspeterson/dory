/* <base/wr/time_util.cc>

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

   Implements <base/wr/time_util.h>.
 */

#include <base/wr/time_util.h>

#include <cerrno>

#include <base/error_util.h>

using namespace Base;

int Base::Wr::clock_gettime(TDisp disp, std::initializer_list<int> errors,
    clockid_t clk_id, timespec *tp) noexcept {
  const int ret = ::clock_gettime(clk_id, tp);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EINVAL})) {
    DieErrno("clock_gettime()", errno);
  }

  return ret;
}

int Base::Wr::nanosleep(TDisp disp, std::initializer_list<int> errors,
    const timespec *req, timespec *rem) noexcept {
  const int ret = ::nanosleep(req, rem);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EINVAL})) {
    DieErrno("nanosleep()", errno);
  }

  return ret;
}
