/* <base/stl_util.h>

   ----------------------------------------------------------------------------
   Copyright 2013 if(we)

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

   Some utilities for working with STL containers.
 */

#pragma once

#include <initializer_list>
#include <limits>

#include <base/impossible_error.h>

namespace Base {

  /* Return true iff. a given value is in a container. */
  template <typename TContainer>
  bool Contains(const TContainer &container,
      const typename TContainer::value_type &val) {
    return container.find(val) != container.end();
  }

  /* Returns the value mapped to the given key.  If the value doesn't appear in
     the container, an assertion fails. */
  template <typename TContainer>
  const typename TContainer::mapped_type &Find(const TContainer &container,
      const typename TContainer::key_type &key) {
    auto iter = container.find(key);
    assert(iter != container.end());
    return iter->second;
  }

  /* Returns an integer rotated to the left by n bits. */
  template <typename TVal>
  TVal RotatedLeft(TVal val, int n) {
    const int digits = std::numeric_limits<TVal>::digits;
    n %= digits;
    return (val << n) | (val >> (digits - n));
  }

  /* Returns an integer rotated to the right by n bits. */
  template <typename TVal>
  TVal RotatedRight(TVal val, int n) {
    const int digits = std::numeric_limits<TVal>::digits;
    n %= digits;
    return (val >> n) | (val << (digits - n));
  }

}  // Base
