/* <log/log_entry.h>

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

   A single log entry, which functions as a std::ostream backed by a fixed size
   buffer.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include <base/error_util.h>
#include <base/no_copy_semantics.h>
#include <log/array_ostream_base.h>
#include <log/log_entry_access_api.h>
#include <log/log_prefix_assign_api.h>
#include <log/log_writer_base.h>
#include <log/pri.h>

namespace Log {

  /* Pointer to function with the following signature:

         void func(TLogPrefixAssignApi &entry) noexcept;

     The function type is defined using decltype because C++ does not allow
     direct use of noexcept in a typedef or using declaration.
     std::remove_reference is needed because the std::declval expression passed
     to decltype() is a temporary (i.e. an rvalue), so the decltype(...)
     evaluates to an rvalue reference to a function pointer.
   */
  using TPrefixWriteFn = std::remove_reference<decltype(std::declval<
      void (*)(TLogPrefixAssignApi &entry) noexcept>())>::type;

  /* Access to the prefix writer is not protected from multithreading races, so
     it should be set before concurrent access is possible. */
  void SetPrefixWriter(TPrefixWriteFn writer) noexcept;

  void WritePrefix(TLogPrefixAssignApi &entry) noexcept;

  /* A single log entry, which functions as a std::ostream backed by a fixed
     size buffer of size 'BufSize'.  If more than (BufSize - PrefixSpace - 2)
     bytes of output are written, the extra output is discarded.  Here the
     value 2 is due to 2 bytes being reserved for a trailing newline and C
     string terminator.

     The first PrefixSpace bytes, and last 2 bytes, of the array are reserved
     for a prefix and suffix, where the suffix is an optional trailing newline
     followed by a mandatory C string terminator.  These bytes are inaccessible
     to the std::ostream.  The prefix space is reserved for an optional log
     entry prefix.  A log entry can be accessed with or without its prefix. */
  template <size_t BufSize, size_t PrefixSpace>
  class TLogEntry final
      : public TArrayOstreamBase<BufSize, PrefixSpace, 2 /* SuffixSpace */>,
        public TLogEntryAccessApi {
    NO_COPY_SEMANTICS(TLogEntry);

    static_assert(PrefixSpace < BufSize, "PrefixSpace larger than BufSize");
    static_assert((BufSize - PrefixSpace) > 2,
        "Not enough space for trailing newline and C string terminator");

    public:
    /* no_stdout_stderr will get a true value only when we are writing fatal
       error output, which always goes to stderr regardless of how the logging
       subsystem is configured.  To avoid duplication, we therefore we want to
       suppress stdout/stderr output from logging in this case.  If errno_value
       is nonzero, a strerror() message will be appended to the log entry. */
    TLogEntry(std::shared_ptr<TLogWriterBase> &&log_writer,
        TPri level, bool no_stdout_stderr = false,
        int errno_value = 0) noexcept
        : TArrayOstreamBase<BufSize, PrefixSpace, 2 /* SuffixSpace */>(),
          LogWriter(std::move(log_writer)),
          Level(level),
          NoStdoutStderr(no_stdout_stderr),
          ErrnoValue(errno_value) {
      assert(!!LogWriter);
    }

    /* Destructor invokes LogWriter for log entry, if not already invoked at
       time of destruction. */
    ~TLogEntry() override {
      Write();
    }

    /* Facilitates expressions such as the following.

           IsEnabled(TPri::INFO) &&
               TLogEntry<K1, K2>(GetLogWriter(), TPri::INFO)
                   << "The answer is " << ComputeAnswer();

       Since TLogEntry has a bool conversion operator, the entire subexpression
       following the && operator has a type of bool, and is not evaluated if
       IsEnabled(TPri::INFO) returns false, avoiding an unnecessary call to
       ComputeAnswer() if logging at level TPri::INFO is disabled.  In
       practice, a preprocessor macro can be defined as follows.

           #define LOG(p) IsEnabled(p) && TLogEntry<K1, K2>(GetLogWriter(), p)

        Then the following shorter syntax achieves the same effect.

            LOG(TPri::INFO) << "The answer is " << ComputeAnswer();
       */
    explicit operator bool() const noexcept {
      return true;
    }

    void AssignPrefix(const char *start, size_t len) noexcept override {
      assert(start || (len == 0));
      PrefixLen = std::min(len, PrefixSpace);
      std::memcpy(&this->GetBuf()[PrefixSpace - PrefixLen], start, PrefixLen);
    }

    /* This will be 0 until Get() has been called with a true value for
       with_prefix.  In other words, the prefix is assigned on-demand. */
    size_t PrefixSize() const noexcept override {
      assert(PrefixLen <= PrefixSpace);
      return PrefixLen;
    }

    std::pair<const char *, const char *>
    Get(bool with_prefix, bool with_trailing_newline) noexcept override {
      if (with_prefix && !HasPrefix()) {
        WritePrefix(*this);
      }

      char *end_pos = this->GetPos();

      if (with_trailing_newline) {
        *end_pos = '\n';
        ++end_pos;
      }

      *end_pos = '\0';  // C string terminator
      char *start_pos = this->GetBuf() + PrefixSpace -
          (with_prefix ? PrefixSize() : 0);
      assert(end_pos >= start_pos);
      return std::make_pair(start_pos, end_pos);
    }

    TPri GetLevel() const noexcept override {
      return Level;
    }

    /* True indicates that entry was written (either successfully or
       unsuccessfully). */
    bool IsWritten() const noexcept {
      return Written;
    }

    /* If log entry has not already been written, write it by invoking
       TLogWriterApi passed to constructor. */
    void Write() noexcept {
      if (!Written) {
        Written = true;

        if (ErrnoValue) {
          Base::AppendStrerror(ErrnoValue, *this);
        }

        if (!this->IsEmpty()) {
          LogWriter->WriteEntry(*this, NoStdoutStderr);
        }
      }
    }

    private:
    /* Destination to write log entry to. */
    const std::shared_ptr<TLogWriterBase> LogWriter;

    /* Log levels correspond to those defined by syslog(). */
    const TPri Level;

    /* If true, omit stdout/stderr output. */
    const bool NoStdoutStderr;

    /* If nonzero, append strerror() message. */
    const int ErrnoValue;

    /* Prefix can be at most PrefixSpace bytes, and starts at buffer index
       (PrefixSpace - PrefixLen). */
    size_t PrefixLen = 0;

    /* True indicates that entry has been written (either successfully or
       unsuccessfully).  In this case, destructor should _not_ attempt to write
       entry. */
    bool Written = false;
  };  // TLogEntry

}  // Log
