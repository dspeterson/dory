/* <server/signal_handler_thread.h>

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

   Class for handling all signals for an application.
 */

#pragma once

#include <thread/fd_managed_thread.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <base/no_copy_semantics.h>
#include <base/sig_set.h>
#include <base/zero.h>

namespace Server {

  /* Dedicated thread for handling signals.  An application is intended to
     start the signal handler thread early during initialization before
     creating any other threads.  The thread then takes complete responsibility
     for all signal handling.  All other threads should keep all signals
     blocked for the lifetime of the application.  Other threads are
     simplified, since they are releived from the burden of dealing with
     signal-related concerns such as interrupted system calls.  When a signal
     is delivered, the handler does only a minimal amount of work, saving the
     associated siginfo_t (see sigaction(2)) and setting a boolean flag
     corresponding to the received signal number.  After the handler returns,
     the awakened thread sees the set flag and calls the client-provided
     callback.  Since the callback is executed by the thread from outside
     signal context, it is free to call functions that would be unsafe to call
     from signal handler context. */
  class TSignalHandlerThread : public Thread::TFdManagedThread {
    NO_COPY_SEMANTICS(TSignalHandlerThread);

    public:
    /* Pointer to function with the following signature:

           void func(int signum, const siginfo_t &info) noexcept;

       The client-supplied signal handling callback that the thread executes
       after the real handler has returned is specified in this manner.  The
       handler type is defined using decltype because C++ does not allow direct
       use of noexcept in a typedef or using declaration.
       std::remove_reference is needed because the std::declval expression
       passed to decltype() is a temporary (i.e. an rvalue), so the
       decltype(...) evaluates to an rvalue reference to a function pointer.
     */
    using THandler = std::remove_reference<decltype(std::declval<
        void (*)(int signum, const siginfo_t &info) noexcept>())>::type;

    /* Singleton accessor. */
    static TSignalHandlerThread &The();

    /* This must be called before calling Start() below.  Specifies a callback
       to execute on receipt of a signal, along with a list of signals to
       handle.  The handler thread will block all other signals. */
    void Init(THandler handler, std::initializer_list<int> signals);

    /* Start the signal handler thread.  This should be called early during
       program initialization, before any other threads have been created.  On
       return, the signal handler thread will be started, and all signals will
       be blocked for the calling thread.  The calling thread (and all of its
       descendents) should leave all signals blocked for the lifetime of the
       program. */
    void Start() override;

    protected:
    void Run() override;

    private:
    struct TSigInfo {
      bool Caught = false;

      siginfo_t Info;

      TSigInfo() noexcept {
        Base::Zero(Info);
      };

      /* Signal handler calls this to record receipt of signal. */
      void Set(const siginfo_t &sig_info) noexcept {
        assert(this);
        std::memcpy(&Info, &sig_info, sizeof(sig_info));
        Caught = true;
      }

      /* Thread calls this after calling client-provided callback. */
      void Clear() noexcept {
        assert(this);
        Caught = false;
        Base::Zero(Info);
      }
    };

    /* Actual signal handler.  Saves siginfo_t and sets boolean flag in
       CaughtSignals map below indicating receipt of signal. */
    static void Handler(int signum, siginfo_t *info, void *ucontext) noexcept;

    /* Private so only singleton accessor can construct one. */
    TSignalHandlerThread() = default;

    void InstallHandler() const;

    bool Initialized = false;

    /* Signal mask indicates "block all signals except those that the handler
       thread is supposed to handle". */
    Base::TSigSet BlockedSet;

    /* Client-provided callback to execute on receipt of signal. */
    THandler HandlerCallback = nullptr;

    /* Key is signal number.  Signal handler records receipt of signal in
       value.  After handler returns, awakened handler thread will examine map,
       see that signal was received, and call client-provided callback. */
    std::unordered_map<int, TSigInfo> CaughtSignals;
  };  // TSignalHandlerThread

};  // Server
