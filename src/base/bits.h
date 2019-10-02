/* <base/bits.h>

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

   Traits class template for determining the number of bits in a non-bool
   integral type.
 */

#pragma once

#include <climits>
#include <cstddef>
#include <type_traits>

namespace Base {

  template <typename T>
  struct TBits {
    static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value,
        "Type parameter must be integral and not bool");

    // Returns number of bits in integral type T.
    static constexpr size_t Value() noexcept {
      return sizeof(T) * CHAR_BIT;
    }
  };

}  // Base
