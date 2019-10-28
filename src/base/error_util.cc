/* <base/error_util.cc>

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

   Implements <base/error_util.h>.
 */

#include <base/error_util.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <base/gettid.h>

using namespace Base;

const char *Base::Strerror(int errno_value, char *buf,
    size_t buf_size) noexcept {
  assert(buf);
  assert(buf_size);

  /* The man page for strerror_r explains all of this ugliness. */

#if ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE)
  /* This is the XSI-compliant version of strerror_r(). */
  int err = strerror_r(errno_value, buf, buf_size);

  if (err) {
    /* In the unlikely event that something went wrong, make the buffer
       contain the empty string, in case it would otherwise be left with
       arbitrary junk. */
    buf[0] = '\0';
  }

  return buf;
#else
  /* This is the GNU-specific version of strerror_r().  Its return type is
     'char *'. */
  return strerror_r(errno_value, buf, buf_size);
#endif
}

static const size_t StrerrorBufSize = 256;

void Base::AppendStrerror(int errno_value, std::string &msg) {
  char tmp_buf[StrerrorBufSize];
  const char *err_msg = Strerror(errno_value, tmp_buf, sizeof(tmp_buf));
  msg += err_msg;
}

void Base::AppendStrerror(int errno_value, std::ostream &out) {
  char tmp_buf[StrerrorBufSize];
  const char *err_msg = Strerror(errno_value, tmp_buf, sizeof(tmp_buf));
  out << err_msg;
}

static void DefaultSecondaryFatalMsgWriter(const char * /* msg */) noexcept {
  /* no-op */
}

static void DefaultSecondaryFatalStackTraceWriter(
    void *const * /*stack_trace_buffer*/,
    size_t /* stack_trace_size */) noexcept {
  /* no-op */
}

static TFatalMsgWriter SecondaryFatalMsgWriter =
    DefaultSecondaryFatalMsgWriter;

static TFatalStackTraceWriter SecondaryFatalStackTraceWriter =
    DefaultSecondaryFatalStackTraceWriter;

void Base::InitSecondaryFatalErrorLogging(TFatalMsgWriter msg_writer,
    TFatalStackTraceWriter stack_trace_writer) noexcept {
  assert(msg_writer);
  assert(stack_trace_writer);
  SecondaryFatalMsgWriter = msg_writer;
  SecondaryFatalStackTraceWriter = stack_trace_writer;
}

static const int stderr_fd = 2;

static void WriteFatalMsgToStderr(const char *msg) noexcept {
  /* In the presence of concurrency, a single call to writev() may make the
     output look a bit nicer than two calls to write().  In some cases, the
     entire output vector is supposed to be written atomically, which avoids
     interleaved output between the message and trailing newline.  The write is
     best effort.  If it fails, there is nothing we can do about it, so ignore
     the error. */
  struct iovec iov[2];
  iov[0].iov_base = const_cast<char *>(msg);
  iov[0].iov_len = std::strlen(msg);
  iov[1].iov_base = const_cast<char *>("\n");
  iov[1].iov_len = 1;
  writev(stderr_fd, iov, sizeof(iov) / sizeof(*iov));
}

static void EmitStackTrace(const char *msg) {
  /* trace_buf is static because we want to use an off-stack location.  This
     minimizes stack usage to preserve memory contents beyond the end of the
     stack, which can potentially be useful to examine in the core file.  Due
     to die_flag (see below), this code path can only execute once, so we don't
     have to worry about concurrency. */
  static const int STACK_TRACE_SIZE = 128;
  static void *trace_buf[STACK_TRACE_SIZE];
  const int trace_size = backtrace(trace_buf, STACK_TRACE_SIZE);
  assert(trace_size <= STACK_TRACE_SIZE);

  /* Write error message and stack trace to stderr first, since this should
     never fail in a manner that causes an additional fatal error. */
  WriteFatalMsgToStderr(msg);
  backtrace_symbols_fd(trace_buf, trace_size, stderr_fd);

  /* Now write output to any configured secondary location(s).  For instance,
     maybe syslog and/or a file. */
  SecondaryFatalMsgWriter(msg);
  SecondaryFatalStackTraceWriter(trace_buf, trace_size);
}

void Base::LogFatal(const char *msg) noexcept {
  WriteFatalMsgToStderr(msg);
  SecondaryFatalMsgWriter(msg);
}

[[ noreturn ]] static void TerminateHandler() noexcept {
  Die("Calling Die() on terminate");
}

void Base::DieOnTerminate() noexcept {
  /* Arrange for Die() to be called on std::terminate() so we hopefully get a
     stack trace before calling std::abort(). */
  std::set_terminate(TerminateHandler);
}

/* First caller of Die() takes this flag. */
static std::atomic_flag die_flag = ATOMIC_FLAG_INIT;

/* ID of thread that holds die_flag.  Initially 0 because no thread can have ID
   of 0. */
static std::atomic<pid_t> die_flag_holder(0);

[[ noreturn ]] static void DieImpl(const char *msg,
    bool stack_trace, TDebugDumpFn debug_dump_fn) noexcept {
  const pid_t my_tid = Gettid();

  if (!die_flag.test_and_set()) {
    /* We are the first caller of Die().  Record our own thread ID so we can
       detect recursive invocation.  The purpose of die_flag is to prevent any
       thread from recursively calling Die() and overflowing its stack, not to
       prevent multiple threads from emitting stack traces, although it has
       that side effect.  If we can get a core dump, stack traces for all
       threads can be obtained from the core file. */
    die_flag_holder.store(my_tid);

    if (stack_trace) {
      EmitStackTrace(msg);  // implementation assumes no concurrency
    } else {
      LogFatal(msg);
    }

    if (debug_dump_fn) {
      debug_dump_fn();
    }

    /* Unless we are running with libasan, this should cause a core dump.
       libasan disables core dumps by default, since they may be very large. */
    std::abort();
  }

  /* If we get here, there are two possibilities:

         1.  die_flag was already taken by this thread.  In other words, we are
             calling Die() recursively.  In this case, we will see a value of
             my_tid when reading die_flag_holder below.  This can occur if
             something really bad happens while emitting the stack trace.  For
             instance, if an exception escapes from a noexcept function, that
             will cause invocation of std::terminate(), which calls
             TerminateHandler() above, which calls Die().

         2.  die_flag was already taken by another thread.  In this case, we
             will either see the other thread's ID or 0 when reading
             die_flag_holder below.

     Don't log secondary output at this point, since that risks calling Die()
     recursively.  Another atomic_flag here would allow us to safely log
     secondary output, but it's probably not worth the effort.
   */

  WriteFatalMsgToStderr(msg);

  if (die_flag_holder.load() == my_tid) {
    /* Call abort() to prevent recursion from continuing until we overflow our
       stack. */
    WriteFatalMsgToStderr("Calling abort() on recursive Die() invocation");
    std::abort();
  }

  WriteFatalMsgToStderr(
      "Other thread detected in Die(): waiting for stack trace to finish");

  /* Wait for die_flag holder to finish stack trace and call abort(). */
  for (; ; ) {
    pause();
  }
}

[[ noreturn ]] void Base::Die(const char *msg,
    TDebugDumpFn debug_dump_fn) noexcept {
  DieImpl(msg, true /* stack_trace */, debug_dump_fn);
}

[[ noreturn ]] void Base::DieNoStackTrace(const char *msg,
    TDebugDumpFn debug_dump_fn) noexcept {
  DieImpl(msg, false /* stack_trace */, debug_dump_fn);
}

[[ noreturn ]] void Base::DieErrno(const char *fn_name,
    int errno_value, TDebugDumpFn debug_dump_fn) noexcept {
  if (errno_value == ENOMEM) {
    /* If we ran out of memory, a stack trace isn't useful and attempting to
       create one may fail.  Just log an error message that makes it obvious
       what happened. */
    DieNoStackTrace(
        "System or library call failed with ENOMEM (out of memory)");
  }

  try {
    std::string msg(fn_name);
    msg += " failed with errno ";
    msg += std::to_string(errno_value);
    msg += ": ";
    AppendStrerror(errno_value, msg);
    Die(msg.c_str(), debug_dump_fn);
  } catch (...) {
    /* We got an exception while creating the error message.  Just use the
       function name as the error message. */
    Die(fn_name, debug_dump_fn);
  }
}
