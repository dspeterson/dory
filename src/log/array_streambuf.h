/* <log/array_streambuf.h>

   ----------------------------------------------------------------------------
   Copyright 2017-2019 Dave Peterson <dave@dspeterson.com>

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
     more than (BufSize - PrefixSpace - SuffixSpace) bytes of output are
     written, the extra output is discarded.

     The first PrefixSpace bytes, and last SuffixSpace bytes, of the array are
     reserved for a prefix and suffix.  These bytes are inaccessible to the
     std::streambuf.  The prefix space can be used for a log entry prefix, and
     the suffix space can be used for a trailing newline and/or C string
     terminator. */
  template <size_t BufSize, size_t PrefixSpace, size_t SuffixSpace>
  struct TArrayStreambuf : public std::streambuf {
    NO_COPY_SEMANTICS(TArrayStreambuf);

    static_assert(PrefixSpace < BufSize, "PrefixSpace too large");
    static_assert((BufSize - PrefixSpace) > SuffixSpace,
        "Not enough space for suffix");

    TArrayStreambuf() noexcept {
      setp(Buf + PrefixSpace, Buf + BufSize - SuffixSpace);
    }

    /* Output is stored here. */
    char Buf[BufSize];
  };  // TArrayStreambuf

}  // Log
