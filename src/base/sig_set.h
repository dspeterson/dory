/* <base/sig_set.h>

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

#include <base/error_util.h>
#include <base/no_default_case.h>
#include <base/wr/signal_util.h>

namespace Base {

  /* A set of signals. */
  class TSigSet {
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
    TSigSet() noexcept {
      Wr::sigemptyset(&OsObj);
    }

    explicit TSigSet(const sigset_t &sigset) noexcept {
      std::memcpy(&OsObj, &sigset, sizeof(OsObj));
    }

    /* See TOp1.*/
    TSigSet(TListInit init, std::initializer_list<int> sigs) noexcept {
      switch (init) {
        case TListInit::Include: {
          Wr::sigemptyset(&OsObj);

          for (int sig: sigs) {
            Wr::sigaddset(&OsObj, sig);
          }

          break;
        }
        case TListInit::Exclude: {
          Wr::sigfillset(&OsObj);

          for (int sig: sigs) {
            Wr::sigdelset(&OsObj, sig);
          }

          break;
        }
        NO_DEFAULT_CASE;
      }
    }

    static TSigSet FromSigmask() noexcept {
      TSigSet result;
      Wr::pthread_sigmask(0, nullptr, &result.OsObj);
      return result;
    }

    /* Copy constructor. */
    TSigSet(const TSigSet &that) noexcept {
      std::memcpy(&OsObj, &that.OsObj, sizeof(OsObj));
    }

    /* Assignment operator. */
    TSigSet &operator=(const TSigSet &that) noexcept {
      assert(this);
      std::memcpy(&OsObj, &that.OsObj, sizeof(OsObj));
      return *this;
    }

    /* Assignment operator from sigset_t. */
    TSigSet &operator=(const sigset_t &sigset) noexcept {
      assert(this);
      std::memcpy(&OsObj, &sigset, sizeof(OsObj));
      return *this;
    }

    /* Add the signal to the set. */
    TSigSet &operator+=(int sig) noexcept {
      assert(this);
      Wr::sigaddset(&OsObj, sig);
      return *this;
    }

    /* Remove the signal from the set. */
    TSigSet &operator-=(int sig) noexcept {
      assert(this);
      Wr::sigdelset(&OsObj, sig);
      return *this;
    }

    /* Construct a new set with the signal added. */
    TSigSet operator+(int sig) noexcept {
      assert(this);
      return TSigSet(*this) += sig;
    }

    /* Construct a new set with the signal removed. */
    TSigSet operator-(int sig) noexcept {
      assert(this);
      return TSigSet(*this) -= sig;
    }

    /* True iff. the signal is in the set. */
    bool operator[](int sig) const noexcept {
      assert(this);
      return Wr::sigismember(&OsObj, sig);
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
  };  // TSigSet

}  // Base
