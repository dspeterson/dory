/* <base/narrow_cast.h>

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

   Safe cast to narrower integral type.
 */

#pragma once

#include <stdexcept>

namespace Base {

  template <typename NARROW, typename WIDE>
  NARROW narrow_cast(WIDE wide) {
    NARROW result = static_cast<NARROW>(wide);

    if (static_cast<WIDE>(result) != wide) {
      throw std::range_error("narrow_cast failed");
    }

    return result;
  }

  template <typename NARROW, typename WIDE>
  constexpr bool CanNarrowCast(WIDE n) {
    return (static_cast<WIDE>(static_cast<NARROW>(n)) == n);
  }

}  // Base
