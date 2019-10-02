/* <base/wr/common.h>

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

   Common definitions for system/library call wrappers.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <initializer_list>

namespace Base {

  namespace Wr {

    /* Defines interpretation of err_list and default_err_list below in
       IsFatal(). */
    enum class TDisp {
      /* err_list specifies additional fatal errors beyond the default set of
         fatal errors for the given system/library call. */
      AddFatal,

      /* err_list specifies additional nonfatal errors beyond the default set
         of nonfatal errors for the given system/library call. */
      AddNonfatal,

      /* err_list specifies the set of all errors that should be treated as
         fatal.  default_err_list is ignored. */
      Fatal,

      /* err_list specifies the set of all errors that should be treated as
         nonfatal.  default_err_list is ignored. */
      Nonfatal
    };

    bool IsFatal(int err, TDisp disp, std::initializer_list<int> err_list,
        bool default_fatal,
        std::initializer_list<int> default_err_list) noexcept;

    [[ noreturn ]] void DieErrno(const char *fn_name,
        int errno_value) noexcept;

  }  // Wr

}  // Base
