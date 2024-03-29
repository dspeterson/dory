/* <base/error_util.h>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)
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

   Error utilities.
 */

#pragma once

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <ostream>
#include <string>
#include <system_error>
#include <type_traits>

#include <execinfo.h>

#include <base/no_copy_semantics.h>

namespace Base {

  /* Throw the given code as an error in the system category. */
  template <typename TCode>
  [[ noreturn ]] inline void ThrowSystemError(TCode code) {
    throw std::system_error(code, std::system_category());
  }

  /* If the given value is < 0, throw a system error based on errno.
     Use this function to test the results of system I/O calls. */
  template <typename TRet>
  TRet IfLt0(TRet &&ret) {
    if (ret < 0) {
      ThrowSystemError(errno);
    }
    return ret;
  }

  /* If the given value != 0, throw a system error based on the return value.
     Use this function to test the results of pthread calls. */
  template <typename TRet>
  TRet IfNe0(TRet &&ret) {
    if (ret != 0) {
      ThrowSystemError(ret);
    }
    return ret;
  }

  /* Return true iff. the error was caused by a signal. */
  inline bool WasInterrupted(const std::system_error &error) {
    /* TODO: change this to:
          return error.code() == errc::interrupted;
       As soon as gcc fixes the bug in cerr. */
    return error.code().value() == EINTR;
  }

  /* This is a thread safe wrapper that hides ugly platform-specific issues
     associated with strerror_r().  It will either copy an error message
     corresponding to 'errno_value' into the caller-supplied 'buf' and return a
     pointer to that buffer, or return a pointer to some statically allocated
     string constant.  The valid lifetime of the memory pointed to by the
     return value must be assumed to not exceed the lifetime of 'buf'. */
  const char *Strerror(int errno_value, char *buf, size_t buf_size) noexcept;

  /* Append a message associated with errno_value to msg. */
  void AppendStrerror(int errno_value, std::string &msg);

  /* Append a message associated with errno_value to the given output
     stream. */
  void AppendStrerror(int errno_value, std::ostream &out);

  /* RAII container for result of backtrace_symbols() library call. */
  class TBacktraceSymbols final {
    NO_COPY_SEMANTICS(TBacktraceSymbols);

    public:
    TBacktraceSymbols() noexcept = default;

    /* Parameters are result of call to backtrace(). */
    TBacktraceSymbols(void *const *buf, size_t buf_size) noexcept
        : Buf(buf ?
              backtrace_symbols(buf, static_cast<int>(buf_size)) : nullptr),
          BufSize(Buf ? buf_size : 0) {
    }

    TBacktraceSymbols(TBacktraceSymbols &&other) noexcept
        : Buf(other.Buf),
          BufSize(other.BufSize) {
      other.Buf = nullptr;
      other.BufSize = 0;
    }

    ~TBacktraceSymbols() {
      Clear();
    }

    TBacktraceSymbols &operator=(TBacktraceSymbols &&other) noexcept {
      if (&other != this) {
        Clear();
        Buf = other.Buf;
        BufSize = other.BufSize;
        other.Buf = nullptr;
        other.BufSize = 0;
      }

      return *this;
    }

    void Clear() noexcept {
      /* As specified by the man page for backtrace_symbols(), free Buf, but
         not the individual pointers in the array it points to. */
      free(Buf);
      Buf = nullptr;
      BufSize = 0;
    }

    size_t Size() const noexcept {
      return BufSize;
    }

    const char *operator[](size_t index) const noexcept {
      assert(index < BufSize);
      return Buf[index];
    }

    private:
    char **Buf = nullptr;

    size_t BufSize = 0;
  };

  /* TFatalMsgWriter is a pointer to a function with the following signature:

         void msg_writer(const char *msg) noexcept;

     TFatalStackTraceWriter is a pointer to a function with the following
     signature:

         void stack_trace_writer(void *const *stack_trace_buffer,
             size_t stack_trace_size) noexcept;

     In both cases, the handler type is defined using decltype because C++ does
     not allow direct use of noexcept in a typedef or using declaration.
     std::remove_reference is needed because the std::declval() expression
     passed to decltype() is a temporary (i.e. an rvalue), so the
     decltype(...) evaluates to an rvalue reference to a function pointer.

     stack_trace_buffer and stack_trace_size contain the results of a call to
     backtrace().  TFatalStackTraceWriter should ideally call
     backtrace_symbols_fd() to log the stack trace with maximum reliability.
     If this is not possible (for instance because output is not going to a
     file descriptor), backtrace_symbols() should be used.
   */

  using TFatalMsgWriter = std::remove_reference<decltype(std::declval<
      void (*)(const char *msg) noexcept>())>::type;

  using TFatalStackTraceWriter = std::remove_reference<decltype(std::declval<
      void (*)(void *const *stack_trace_buffer,
          size_t stack_trace_size) noexcept>())>::type;

  /* Install functions for secondary fatal error output (i.e. logging
     subsystem).  Functions should avoid writing to stdout/stderr since primary
     output always goes to stderr. */
  void InitSecondaryFatalErrorLogging(TFatalMsgWriter msg_writer,
      TFatalStackTraceWriter stack_trace_writer) noexcept;

  /* Intended to be called by a function of type TDebugDumpFn to log debug info
     during fatal error handling.  Writes to stderr, and additionally calls any
     function specified by InitSecondaryFatalErrorLogging(). */
  void LogFatal(const char *msg) noexcept;

  /* Call std::set_terminate() to install a std::terminate_handler that
     immediately calls Die(), which should generate a stack trace before
     calling std::abort(). */
  void DieOnTerminate() noexcept;

  class TDieHandler {
    public:
    TDieHandler() noexcept = default;

    virtual ~TDieHandler()  = default;

    /* Override this to perform custom action in Die() code path.  Call to this
       method will be omitted if Die() is already in progress.  If called, this
       method will be called only once by a single thread (no need to worry
       about multiple threads calling it concurrently).  Implementation may
       call LogFatal() as needed to emit debug output. */
    virtual void operator()() noexcept;
  };

  /* Generate a stack trace, log fatal error message, and dump core.
     Optionally call a caller-provided function for emitting debug output. */
  [[ noreturn ]] void Die(const char *msg,
      TDieHandler *die_handler = nullptr) noexcept;

  /* Log error message and die.  Dump core if requested, but don't generate a
     stack trace.  Optionally call a caller-provided function for emitting
     debug output. */
  [[ noreturn ]] void DieNoStackTrace(const char *msg, bool dump_core,
      TDieHandler *die_handler = nullptr) noexcept;

  /* fn_name is the name of a system call or library function (for instance,
     "fcntl()" or "socket()") that failed with the given errno value.  Die with
     an appropriate error message.  Optionally call a caller-provided function
     for emitting debug output. */
  [[ noreturn ]] void DieErrno(const char *fn_name, int errno_value,
      TDieHandler *die_handler = nullptr) noexcept;

}  // Base
