/* <signal/set.h>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   A set of signals.
 */

#pragma once

#include <cassert>
#include <cstring>
#include <initializer_list>

#include <signal.h>

#include <base/error_utils.h>
#include <base/no_default_case.h>

namespace Signal {

  /* A set of signals. */
  class TSet {
    public:
    /* How to construct a new set from an initializer list. */
    enum class TListInit {
      /* Include only the signals in the list. */
      Include,

      /* Include all signals except the ones in the list. */
      Exclude
    };  // TListInit

    /* In the implementation below, we assume that sigemptyset(), sigfillset(),
       sigaddset(), sigdelset(), and sigismember() will never return an error,
       which seems reasonable.  This avoids inconvenience. */

    /* See TOp0. */
    TSet() noexcept {
      int result = sigemptyset(&OsObj);
      assert(result == 0);
    }

    /* See TOp1.*/
    TSet(TListInit init, std::initializer_list<int> sigs) noexcept {
      switch (init) {
        case TListInit::Include: {
          int result = sigemptyset(&OsObj);
          assert(result == 0);

          for (int sig: sigs) {
            result = sigaddset(&OsObj, sig);
            assert(result == 0);
          }

          break;
        }
        case TListInit::Exclude: {
          int result = sigfillset(&OsObj);
          assert(result == 0);

          for (int sig: sigs) {
            result = sigdelset(&OsObj, sig);
            assert(result == 0);
          }

          break;
        }
        NO_DEFAULT_CASE;
      }
    }

    static TSet FromSigmask() {
      TSet result;
      Base::IfNe0(pthread_sigmask(0, nullptr, &result.OsObj));
      return result;
    }

    /* Copy constructor. */
    TSet(const TSet &that) noexcept {
      assert(&that);
      std::memcpy(&OsObj, &that.OsObj, sizeof(OsObj));
    }

    /* Assignment operator. */
    TSet &operator=(const TSet &that) noexcept {
      assert(&that);
      std::memcpy(&OsObj, &that.OsObj, sizeof(OsObj));
      return *this;
    }

    /* Add the signal to the set. */
    TSet &operator+=(int sig) noexcept {
      assert(this);
      int result = sigaddset(&OsObj, sig);
      assert(result == 0);
      return *this;
    }

    /* Remove the signal from the set. */
    TSet &operator-=(int sig) noexcept {
      assert(this);
      int result = sigdelset(&OsObj, sig);
      assert(result == 0);
      return *this;
    }

    /* Construct a new set with the signal added. */
    TSet operator+(int sig) noexcept {
      assert(this);
      return TSet(*this) += sig;
    }

    /* Construct a new set with the signal removed. */
    TSet operator-(int sig) noexcept {
      assert(this);
      return TSet(*this) -= sig;
    }

    /* True iff. the signal is in the set. */
    bool operator[](int sig) const noexcept {
      assert(this);
      int result = sigismember(&OsObj, sig);
      assert((result == 0) || (result == 1));
      return result != 0;
    }

    /* Access the OS object. */
    const sigset_t &operator*() const noexcept {
      assert(this);
      return OsObj;
    }

    /* Access the OS object. */
    const sigset_t *Get() const noexcept {
      assert(this);
      return &OsObj;
    }

    private:
    /* The OS representation of the set. */
    sigset_t OsObj;
  };  // TSet

}  // Signal
