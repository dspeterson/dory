/* <base/on_destroy.h>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

   Utility class for executing caller-supplied function on destructor
   invocation.
 */

#pragma once

#include <cassert>
#include <utility>

#include <base/no_copy_semantics.h>

namespace Base {

  /* RAII class template that calls the provided lambda on destruction.  To
     use, call OnDestroy() helper function below. */
  template <typename Lambda>
  class TOnDestroy final {
    NO_COPY_SEMANTICS(TOnDestroy);

    public:
    /* noexcept if Lambda copy constructor is noexcept.  Note that due to
       reference collapsing, std::declval<const Lambda &>() is of type
       const lambda &. */
    explicit TOnDestroy(Lambda const &fn)
        noexcept(noexcept(Lambda(std::declval<const Lambda &>())))
        : Fn(fn) {
    }

    /* noexcept if Lambda move constructor is noexcept. */
    explicit TOnDestroy(Lambda &&fn)
        noexcept(noexcept(Lambda(std::declval<Lambda>())))
        : Fn(std::move(fn)) {
    }

    /* noexcept if Lambda move constructor is noexcept. */
    TOnDestroy(TOnDestroy<Lambda> &&other)
    noexcept(noexcept(Lambda(std::declval<Lambda>())))
        : Fn(std::move(other.Fn)),
          Active(other.Active) {
      other.Active = false;
    }

    /* noexcept if Lambda move constructor is noexcept. */
    TOnDestroy &operator=(TOnDestroy &&other)
        noexcept(noexcept(Lambda(std::declval<Lambda>()))) {
      if (&other != this) {
        Fn = std::move(other.Fn);
        Active = other.Active;
        other.Active = false;
      }

      return *this;
    }

    ~TOnDestroy() {
      if (Active) {
        Fn();
      }
    }

    void Cancel() noexcept {
      assert(this);
      Active = false;
    }

    private:
    Lambda Fn;

    bool Active = true;
  };  // TOnDestroy

  template <typename Lambda>
  TOnDestroy<Lambda> OnDestroy(Lambda &&action) {
    return TOnDestroy<Lambda>(std::forward<Lambda>(action));
  }

}  // Base
