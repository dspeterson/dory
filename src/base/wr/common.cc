/* <base/wr/common.cc>

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

   Implements <base/wr/common.h>.
 */

#include <base/wr/common.h>

#include <base/error_util.h>
#include <base/no_default_case.h>

using namespace Base;

static bool contains(std::initializer_list<int> err_list, int err) noexcept {
  return (std::find(err_list.begin(), err_list.end(), err) != err_list.end());
}

bool Base::Wr::IsFatal(int err, TDisp disp,
    std::initializer_list<int> err_list, bool list_fatal,
    std::initializer_list<int> default_err_list) noexcept {
  bool fatal_if_found = false;

  switch (disp) {
    case TDisp::AddFatal: {
      fatal_if_found = true;
      /* fallthrough */
    }
    case TDisp::AddNonfatal: {
      if (contains(err_list, err)) {
        return fatal_if_found;
      }

      break;
    }
    case TDisp::Fatal: {
      fatal_if_found = true;
      /* fallthrough */
    }
    case TDisp::Nonfatal: {
      return (contains(err_list, err) == fatal_if_found);
    }
    NO_DEFAULT_CASE;
  }

  return (contains(default_err_list, err) == list_fatal);
}

