/* <log/array_ostream_base.h>

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

   A simple std::ostream backed by an array.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <ostream>

#include <base/no_copy_semantics.h>
#include <log/array_streambuf.h>

namespace Log {

  /* Simple std::ostream backed by an internal array of size 'BufSize'.  If
     more than (BufSize - PrefixSpace - SuffixSpace) bytes of output are
     written, the extra output is discarded.

     The first PrefixSpace bytes, and last SuffixSpace bytes, of the array are
     reserved for a prefix and suffix.  These bytes are inaccessible to the
     std::ostream.  The prefix space can be used for a log entry prefix, and
     the suffix space can be used for a trailing newline and/or C string
     terminator. */
  template <size_t BufSize, size_t PrefixSpace, size_t SuffixSpace>
  class TArrayOstreamBase
      : private TArrayStreambuf<BufSize, PrefixSpace, SuffixSpace>,
        public std::ostream {
    NO_COPY_SEMANTICS(TArrayOstreamBase);

    static_assert(PrefixSpace < BufSize, "PrefixSpace too large");
    static_assert((BufSize - PrefixSpace) > SuffixSpace,
        "Not enough space for suffix");

    public:
    /* Return number of bytes written to stream.  First byte written appears at
       Buf[PrefixSpace], and when (BufSize - PrefixSpace - SuffixSpace) bytes
       have been written, additional written bytes are discarded. */
    size_t Size() const noexcept {
      assert(GetPos() >= GetBuf());
      return GetPos() - GetBuf();
    }

    /* Return true if no bytes have been written to stream. */
    bool IsEmpty() const noexcept {
      return (Size() == 0);
    }

    protected:
    TArrayOstreamBase() noexcept
        : TArrayStreambuf<BufSize, PrefixSpace, SuffixSpace>(),
          std::ostream(this) {
    }

    /* Return pointer to start of internal array.  Stream output begins at
       Buf[PrefixSpace]. */
    const char *GetBuf() const noexcept {
      return this->Buf;
    }

    /* Return pointer to start of internal array.  Stream output begins at
       Buf[PrefixSpace]. */
    char *GetBuf() noexcept {
      return this->Buf;
    }

    /* Return pointer to array position one byte past last byte of stream
       output. */
    const char *GetPos() const noexcept {
      return this->pptr();
    }

    /* Return pointer to array position one byte past last byte of stream
       output. */
    char *GetPos() noexcept {
      return this->pptr();
    }
  };

}  // Log
