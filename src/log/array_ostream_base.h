/* <log/array_ostream_base.h>

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
     more output is written than the array can hold, the extra output is
     discarded. */
  template <size_t BufSize>
  class TArrayOstreamBase
      : private TArrayStreambuf<BufSize>, public std::ostream {
    NO_COPY_SEMANTICS(TArrayOstreamBase);

    public:
    static const size_t BUF_SIZE = BufSize;

    ~TArrayOstreamBase() override = default;

    /* Return number of bytes written to stream. */
    size_t Size() const noexcept {
      assert(this);
      assert(GetPos() >= GetBuf());
      return GetPos() - GetBuf();
    }

    bool IsEmpty() const noexcept {
      assert(this);
      return Size() == 0;
    }

    protected:
    /* 'reserve_bytes' specifies a number of bytes to reserve at the end of the
       buffer.  The intended use is to create space for a trailing newline
       and/or C string terminator.  This leaves (BufSize - reserve_bytes) bytes
       available for output data. */
    explicit TArrayOstreamBase(size_t reserve_bytes) noexcept
        : TArrayStreambuf<BufSize>(reserve_bytes),
          std::ostream(this) {
    }

    /* Return pointer to start of internal array. */
    const char *GetBuf() const noexcept {
      assert(this);
      return this->Buf;
    }

    /* Return pointer to start of internal array. */
    char *GetBuf() noexcept {
      assert(this);
      return this->Buf;
    }

    /* Return pointer to array position one byte past last byte of output. */
    const char *GetPos() const noexcept {
      assert(this);
      return this->pptr();
    }

    /* Return pointer to array position one byte past last byte of output. */
    char *GetPos() noexcept {
      assert(this);
      return this->pptr();
    }
  };

}  // Log
