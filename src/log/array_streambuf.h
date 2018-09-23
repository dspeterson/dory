/* <log/array_streambuf.h>

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

   A simple std::streambuf backed by an array.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <streambuf>

#include <base/no_copy_semantics.h>

namespace Log {

  /* Simple std::streambuf backed by an internal array of size 'BufSize'.  If
     more output is written than the array can hold, the extra output is
     discarded. */
  template <size_t BufSize>
  struct TArrayStreambuf : public std::streambuf {
    NO_COPY_SEMANTICS(TArrayStreambuf);

    static const size_t BUF_SIZE = BufSize;

    /* 'reserve_bytes' specifies a number of bytes to reserve at the end of the
       buffer.  The intended use is to create space for a trailing newline
       and/or C string terminator.  This leaves (BufSize - reserve_bytes) bytes
       available for output data. */
    explicit TArrayStreambuf(size_t reserve_bytes) noexcept
        : ReserveBytes(reserve_bytes) {
      assert(ReserveBytes <= BufSize);
      setp(Buf, Buf + BufSize - ReserveBytes);
    }

    ~TArrayStreambuf() override = default;

    /* Number of bytes to reserve at end of buffer. */
    size_t ReserveBytes;

    /* Output is stored here. */
    char Buf[BufSize];
  };  // TArrayStreambuf

}  // Log
