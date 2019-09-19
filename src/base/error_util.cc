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
#include <unistd.h>

#include <base/gettid.h>

using namespace Base;

const char *Base::Strerror(int errno_value, char *buf, size_t buf_size) {
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

void Base::DefaultDieHandler(const char *msg,
    void *const *stack_trace_buffer, size_t stack_trace_size) noexcept {
  /* Default behavior is to write 'msg' and stack trace to stderr.  If write()
     fails here, there is nothing we can do about it. */
  const int stderr_fd = 2;
  write(stderr_fd, msg, std::strlen(msg));
  write(stderr_fd, "\n", 1);
  backtrace_symbols_fd(stack_trace_buffer, stack_trace_size, stderr_fd);
}

static TDieHandler DieHandler = DefaultDieHandler;

void Base::SetDieHandler(Base::TDieHandler handler) noexcept {
  assert(handler);
  DieHandler = handler;
}

[[ noreturn ]] static void TerminateHandler() noexcept {
  Die("Calling Die() on terminate");
}

void Base::DieOnTerminate() noexcept {
  /* Arrange for Die() to be called on std::terminate() so we hopefully get a
     stack trace before calling std::abort(). */
  std::set_terminate(TerminateHandler);
}

static void EmitStackTrace(const char *msg) {
  /* trace_buf is static because we want to use an off-stack location.  This
     minimizes stack usage to preserve memory contents beyond the end of the
     stack, which can potentially be useful to examine in the core file.  Due
     to die_flag (see below), this code path can only execute once, so we don't
     have to worry about reentrant usage. */
  static const size_t STACK_TRACE_SIZE = 128;
  static void *trace_buf[STACK_TRACE_SIZE];
  const int trace_size = backtrace(trace_buf,
      static_cast<int>(STACK_TRACE_SIZE));
  assert(static_cast<size_t>(trace_size) <= STACK_TRACE_SIZE);

  /* Log error message and stack trace. */
  DieHandler(msg, trace_buf, static_cast<size_t>(trace_size));
}

/* First caller of Die() takes this flag. */
static std::atomic_flag die_flag = ATOMIC_FLAG_INIT;

/* ID of thread that holds die_flag.  Initially 0 because no thread can have ID
   of 0. */
static std::atomic<pid_t> die_flag_holder(0);

[[ noreturn ]] void Base::Die(const char *msg) noexcept {
  const pid_t my_tid = Gettid();

  if (!die_flag.test_and_set()) {
    /* We are the first caller of Die().  Record our own thread ID so we can
       detect recursive invocation.  The purpose of die_flag is to prevent any
       thread from recursively calling Die() and overflowing its stack, not to
       prevent multiple threads from emitting stack traces, although it has
       that side effect.  If we can get a core dump, stack traces for all
       threads can be obtained from the core file. */
    die_flag_holder.store(my_tid);

    EmitStackTrace(msg);

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

     All writes to stderr below are best effort.  If a write fails, there is
     nothing we can do about it, so ignore the error.
   */
  const int stderr_fd = 2;
  write(stderr_fd, msg, std::strlen(msg));
  write(stderr_fd, "\n", 1);

  if (die_flag_holder.load() == my_tid) {
    /* Call abort() immediately to prevent recursion from continuing until we
       overflow our stack. */
    const char abort_msg[] = "Calling abort() on recursive Die() invocation";
    write(stderr_fd, abort_msg, std::strlen(abort_msg));
    write(stderr_fd, "\n", 1);
    std::abort();
  }

  const char other_thread_msg[] =
      "Other thread detected in Die(): waiting for stack trace to finish";
  write(stderr_fd, other_thread_msg, std::strlen(other_thread_msg));
  write(stderr_fd, "\n", 1);

  /* Wait for die_flag holder to finish stack trace and call abort(). */
  for (; ; ) {
    pause();
  }
}
