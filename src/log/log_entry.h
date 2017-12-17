/* <log/log_entry.h>

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

   A single log entry, which functions as a std::ostream backed by a fixed size
   buffer.
 */

#pragma once

#include <cassert>
#include <utility>

#include <base/no_copy_semantics.h>
#include <log/array_ostream_base.h>
#include <log/log_entry_access_api.h>
#include <log/log_writer_api.h>

namespace Log {

  /* A single log entry, which functions as a std::ostream backed by a fixed
     size buffer.  If more output is written than the buffer can hold, the
     extra output is discarded. */
  class TLogEntry : public TArrayOstreamBase<512>,
      public TLogEntryAccessApi {
    NO_COPY_SEMANTICS(TLogEntry);

    public:
    TLogEntry(TLogWriterApi &log_writer, int level);

    /* Destructor invokes LogWriter for log entry, if not already invoked at
       time of destruction. */
    virtual ~TLogEntry() noexcept;

    /* Return a pair of pointers, where the first pointer indicates the start
       of the log entry, and the second pointer points one byte past the last
       byte to be written to the log.  The log entry will _not_ have a newline
       appended, but it will have a C string terminator. */
    std::pair<const char *, const char *>
    GetWithoutTrailingNewline() noexcept override;

    /* Return a pair of pointers, where the first pointer indicates the start
       of the log entry, and the second pointer points one byte past the last
       byte to be written to the log (which will be an appended newline).  The
       log entry will have a newline appended, and followed by a C string
       terminator. */
    std::pair<const char *, const char *>
    GetWithTrailingNewline() noexcept override;

    virtual int GetLevel() const noexcept;

    void SetLevel(int level) noexcept;

    /* True indicates that entry was written (either successfully or
       unsuccessfully). */
    bool IsWritten() const noexcept {
      assert(this);
      return Written;
    }

    /* Write log entry by invoking TLogWriterApi passed to constructor.  Then
       clear log entry.  On error, throws TLogWriterApi::TLogWriteError() and
       leaves log entry contents intact (although further calls to Write() will
       have no effect). */
    void Write();

    private:
    /* Destination to write log entry to. */
    TLogWriterApi &LogWriter;

    /* Log level, as defined by syslog(). */
    int Level;

    /* True indicates that entry has been written (either successfully or
       unsuccessfully).  In this case, destructor should _not_ attempt to write
       entry. */
    bool Written;
  };  // TLogEntry

}  // Log
