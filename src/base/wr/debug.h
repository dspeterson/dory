/* <base/wr/debug.h>

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

   Debug instrumentation for system/library call wrappers.
 */

#pragma once

namespace Base {

  namespace Wr {

    enum class TFdOp {
      Create1,
      Create2,
      Dup,
      Close
    };

    /* Track an operation that involves file descriptor creation or
       destruction.  Operation will be logged internally to a circular buffer,
       along with a partial stack trace. */
    void TrackFdOp(TFdOp op, int fd1, int fd2 = -1) noexcept;

    /* Log entire contents of file descriptor tracking buffer. */
    void DumpFdTrackingBuffer() noexcept;

  };  // Wr

}  // Base
