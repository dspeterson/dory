/* <base/opt.h>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)
   Copyright 2018 Dave Peterson <dave@dspeterson.com>

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

   An optional value.
 */

#pragma once

#include <cassert>
#include <istream>
#include <ostream>
#include <type_traits>
#include <utility>

#include <base/safe_global.h>

namespace Base {

  /* An optional value; that is, a value which may or may not be known.  This
     is a value type, and TVal must also be a value type.  The domain of TVal
     is augmented with the additional state of being unknown.

     The interface to TOpt<> looks like that of a smart-pointer.  The implicit
     cast to bool and deference operators are overloaded.  You can therefore do
     things like this:

        void OtherFunc(const TFoo &foo);

        void ThisFunc(const TOpt<TFoo> &opt_foo) {
          if (opt_foo) {
            OtherFunc(*opt_foo);
          }
        }

     Note that dereferencing an unknown TOpt<> is illegal.  You can, however,
     call MakeKnown() to force an unknown TOpt<TVal> to construct a TVal if it
     doesn't already have one.

     The storage for the instance of TVal is allocated within the TOpt<TVal>
     instance, but remains uninitialized until the TVal is referred to.  This
     allows you to pass instances of TOpt<> around without worrying about extra
     heap allocations.  If TVal has light-weight copying semantics (such as a
     COW scheme), then it plausible to pass instances of TOpt<TVal> by value.

     A constant of the type TOpt<TVal> and with the unknown value is defined
     for you as a convenience.  Refer to it as TOpt<TVal>::Unknown.

     There is a std stream inserter for this type. */
  template <typename TVal>
  class TOpt final {
    public:
    using TValueType = TVal;

    /* Default-construct as an unknown. */
    TOpt() noexcept = default;

    TOpt(TOpt &&that)
        noexcept(std::is_nothrow_move_constructible<TVal>::value)
        : Val((that.Val) ?
              new (Storage) TVal(std::move(*that.Val)) : nullptr) {
    }

    TOpt(const TOpt &that)
        noexcept(std::is_nothrow_copy_constructible<TVal>::value)
        : Val(that.Val ? new (Storage) TVal(*that.Val) : nullptr) {
    }

    /* Construct by moving the given value into our internal storage.
       What remains of the donor is up to TVal. */
    TOpt(TVal &&that)
        noexcept(std::is_nothrow_move_constructible<TVal>::value)
        : Val(new (Storage) TVal(std::move(that))) {
    }

    /* Construct with a copy of the given value. */
    TOpt(const TVal &that)
        noexcept(std::is_nothrow_copy_constructible<TVal>::value)
        : Val(new (Storage) TVal(that)) {
    }

    /* If we're known, destroy our value as we go.  Implicitly noexcept if
       Reset() is noexcept. */
    ~TOpt() {
      assert(this);
      Reset();
    }

    TOpt &operator=(TOpt &&that)
        noexcept(std::is_nothrow_move_constructible<TVal>::value &&
            std::is_nothrow_move_assignable<TVal>::value) {
      assert(this);
      assert(&that);

      if (this != &that) {
        if (Val) {
          if (that.Val) {
            *Val = std::move(*that.Val);
          } else {
            Reset();
          }
        } else if (that.Val) {
          Val = new (Storage) TVal(std::move(*that.Val));
        }
      }

      return *this;
    }

    TOpt &operator=(const TOpt &that)
        noexcept(std::is_nothrow_copy_constructible<TVal>::value &&
            std::is_nothrow_copy_assignable<TVal>::value) {
      assert(this);
      assert(&that);

      if (this != &that) {
        if (Val) {
          if (that.Val) {
            *Val = *that.Val;
          } else {
            Reset();
          }
        } else if (that.Val) {
          Val = new (Storage) TVal(*that.Val);
        }
      }

      return *this;
    }

    TOpt &operator=(TVal &&that)
        noexcept(std::is_nothrow_move_constructible<TVal>::value &&
            std::is_nothrow_move_assignable<TVal>::value) {
      assert(this);
      assert(&that);

      if (Val != &that) {
        if (Val) {
            *Val = std::move(that);
        } else {
          Val = new (Storage) TVal(std::move(that));
        }
      }

      return *this;
    }

    /* Assume a copy of the given value.  If we weren't known before, we will
       be now. */
    TOpt &operator=(const TVal &that)
        noexcept(std::is_nothrow_copy_constructible<TVal>::value &&
            std::is_nothrow_copy_assignable<TVal>::value) {
      assert(this);
      assert(&that);

      if (Val != &that) {
        if (Val) {
          *Val = that;
        } else {
          Val = new (Storage) TVal(that);
        }
      }

      return *this;
    }

    /* True iff. we're known. */
    operator bool() const noexcept {
      assert(this);
      return IsKnown();
    }

    /* Our value.  We must already be known. */
    const TVal &operator*() const noexcept {
      assert(this);
      assert(Val);
      return *Val;
    }

    /* Our value.  We must already be known. */
    TVal &operator*() noexcept {
      assert(this);
      assert(Val);
      return *Val;
    }

    /* Our value.  We must already be known. */
    const TVal *operator->() const noexcept {
      assert(this);
      assert(Val);
      return Val;
    }

    /* Our value.  We must already be known. */
    TVal *operator->() noexcept {
      assert(this);
      assert(Val);
      return Val;
    }

    /* A pointer to our value.  We must already be known. */
    const TVal *Get() const noexcept {
      assert(this);
      assert(Val);
      return Val;
    }

    /* A pointer to our value.  We must already be known. */
    TVal *Get() noexcept {
      assert(this);
      assert(Val);
      return Val;
    }

    /* True iff. we're known. */
    bool IsKnown() const noexcept {
      assert(this);
      return Val != nullptr;
    }

    /* True iff. we're not known. */
    bool IsUnknown() const noexcept {
      assert(this);
      return !IsKnown();
    }

    /* If we're already known, do nothing; otherwise, construct a new value
       using the given args.  Return a reference to our (possibly new)
       value. */
    template <typename... TArgs>
    TVal &MakeKnown(TArgs &&... args) noexcept(
        noexcept(TVal(std::forward<TArgs>(args)...))) {
      assert(this);

      if (!Val) {
        Val = new (Storage) TVal(std::forward<TArgs>(args)...);
      }

      return *Val;
    }

    /* Reset to the unknown state. */
    TOpt &Reset() noexcept(std::is_nothrow_destructible<TVal>::value) {
      assert(this);

      if (Val) {
        Val->~TVal();
        Val = nullptr;
      }

      return *this;
    }

    /* A pointer to our value.  If we're not known, return nullptr. */
    const TVal *TryGet() const noexcept {
      assert(this);
      return Val;
    }

    /* A pointer to our value.  If we're not known, return nullptr. */
    TVal *TryGet() noexcept {
      assert(this);
      return Val;
    }

    /* A constant instance in the unknown state.  Useful to have around. */
    static const TSafeGlobal<TOpt> Unknown;

    private:
    /* The storage space used to hold our known value, if any.  We use in-place
       new operators and explicit destruction to make values come and go from
       this storage space. */
    char Storage[sizeof(TVal)] __attribute__((aligned(__BIGGEST_ALIGNMENT__)));

    /* A pointer to our current value.  If this is null, then our value is
       unknown.  If it is non-null, then it points to Storage. */
    TVal *Val = nullptr;
  };  // TOpt

  /* See declaration. */
  template <typename TVal>
  const TSafeGlobal<TOpt<TVal>> TOpt<TVal>::Unknown(
      [] {
        return new TOpt<TVal>();
      });

  /* A std stream inserter for Base::TOpt<>.  If the TOpt<> is unknown, then
     this function inserts nothing. */
  template <typename TVal>
  std::ostream &operator<<(std::ostream &strm, const Base::TOpt<TVal> &that) {
    assert(&strm);
    assert(&that);

    if (that) {
      strm << *that;
    }

    return strm;
  }

  /* A std stream extractor for Base::TOpt<>. */
  template <typename TVal>
  std::istream &operator>>(std::istream &strm, Base::TOpt<TVal> &that) {
    assert(&strm);
    assert(&that);

    if (!strm.eof()) {
      strm >> that.MakeKnown();
    } else {
      that.Reset();
    }

    return strm;
  }

}  // Base
