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

#include <cstdlib>
#include <cstring>

#include <unistd.h>

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

static void DefaultDieHandler(const char *msg,
    void *const *stack_trace_buffer, size_t stack_trace_size) noexcept {
  /* Default behavior is to write 'msg' and stack trace to stderr.  If write()
     fails here, there is nothing we can do about it. */
  const int out_fd = 2;  // stderr
  write(out_fd, msg, std::strlen(msg));
  write(out_fd, "\n", 1);
  backtrace_symbols_fd(stack_trace_buffer, stack_trace_size, out_fd);
}

static TDieHandler DieHandler = DefaultDieHandler;

void Base::SetDieHandler(Base::TDieHandler handler) noexcept {
  assert(handler);
  DieHandler = handler;
}

[[ noreturn ]] void Base::Die(const char *msg) noexcept {
  /* Ideally, we should use an off-stack location for the stack trace buffer.
     This would allow preservation of memory contents beyond the end of the
     stack, which can potentially be useful to examine in the core file.  For
     now, this is good enough. */
  const size_t STACK_TRACE_SIZE = 128;
  void *trace_buf[STACK_TRACE_SIZE];
  const int trace_size = backtrace(trace_buf,
      static_cast<int>(STACK_TRACE_SIZE));
  assert(static_cast<size_t>(trace_size) <= STACK_TRACE_SIZE);

  /* Log error message and stack trace. */
  DieHandler(msg, trace_buf, static_cast<size_t>(trace_size));

  abort();  // force core dump
}
