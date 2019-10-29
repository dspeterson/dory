/* <dory/util/misc_util.h>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Utility functions.
 */

#pragma once

#include <cstddef>
#include <string>

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <log/pri.h>

namespace Dory {

  namespace Util {

    /* Cause file descriptor returned by GetShutdownRequestFd() below to become
       readable.  This is called in response to a shutdown signal, but it may
       also be called directly (for instance, by test code). */
    void RequestShutdown() noexcept;

    /* Cause file descriptor returned by GetShutdownRequestFd() below to become
       unreadable.  This is intended for use by unit tests.  Calling this
       function at the start of a test will reset the file descriptor to its
       original state if a prior test left it readable. */
    void ClearShutdownRequest();

    /* Return a file descriptor that becomes readable when a shutdown signal
       has been received or RequestShutdown() above has been called. */
    const Base::TFd &GetShutdownRequestedFd() noexcept;

    /* Start signal handler thread.  SIGINT and SIGTERM will cause file
       descriptor returned bu GetShutdownSignalFd() below to become readable.
       SIGUSR1 will cause logging subsystem to reopen logfile if file logging
       is enabled. */
    void StartSignalHandlerThread();

    /* Stop signal handler thread.  This is a no-op if signal handler thread
       has not yet been started. */
    void StopSignalHandlerThread() noexcept;

    /* Simple RAII class for starting and stopping signal handler thread. */
    class TSignalHandlerThreadStarter final {
      NO_COPY_SEMANTICS(TSignalHandlerThreadStarter);

      public:
      explicit TSignalHandlerThreadStarter(bool start_now = true) {
        if (start_now) {
          Start();
        }
      }

      ~TSignalHandlerThreadStarter() {
        StopSignalHandlerThread();
      }

      TSignalHandlerThreadStarter(TSignalHandlerThreadStarter &&) = delete;

      TSignalHandlerThreadStarter &operator=(
          TSignalHandlerThreadStarter &&) = delete;

      void Start() {
        StartSignalHandlerThread();
      }

      void Stop() noexcept {
        StopSignalHandlerThread();
      }
    };

    /* Result of call to TestUnixDgSize() below. */
    enum class TUnixDgSizeTestResult {
      /* Test passed with default value for SO_SNDBUF. */
      Pass,

      /* Test passed after setting SO_SNDBUF to size of test datagram. */
      PassWithLargeSendbuf,

      /* Test failed. */
      Fail
    };

    /* Attempt to send and receive a UNIX domain datagram of 'size' bytes.
       Return true on success or false on failure.  Throw on fatal system
       error.  */
    TUnixDgSizeTestResult TestUnixDgSize(size_t size);

  }  // Util

}  // Dory
