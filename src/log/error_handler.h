/* <log/error_handler.h>

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

   Error handler function definition.
 */

#pragma once

#include <type_traits>

namespace Log {

  enum class TLogWriteError {
    /* Write succeeded, but the count of bytes written was short. */
    ShortCount,

    /* Some sort of system-level error occurred on attempted write. */
    SysError
  };

  /* Pointer to error handler function with the following signature:

         void func(TLogWriteError error) noexcept;

     The handler type is defined using decltype because C++ does not allow
     direct use of noexcept in a typedef or using declaration.
     std::remove_reference is needed because the std::declval expression passed
     to decltype() is a temporary (i.e. an rvalue), so the decltype(...)
     evaluates to an rvalue reference to a function pointer.
   */
  using TWriteErrorHandler = std::remove_reference<decltype(std::declval<
      void (*)(TLogWriteError) noexcept>())>::type;

}
