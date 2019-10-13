/* <log/log.h>

   ----------------------------------------------------------------------------
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

   Header to include for basic logging operations.
 */

#pragma once

#include <chrono>
#include <cstddef>

#include <base/thread_safe_rate_limiter.h>
#include <log/log_entry.h>
#include <log/log_writer.h>
#include <log/pri.h>

namespace Log {

  /* Bytes of space available to hold a single log entry, including prefix,
     trailing newline, and C string terminator. */
  const size_t LogEntryBufSize = 512;

  /* Bytes of space from LogEntryBufSize reserved for a prefix. */
  const size_t LogEntryPrefixSpace = 64;

  /* Used in LOG() and LOG_R() macros below. */
  using TLogEntryType = TLogEntry<LogEntryBufSize, LogEntryPrefixSpace>;

  /* Used in LOG_R() macro below. */
  using TLogRateLimiter = Base::TThreadSafeRateLimiter<
      std::chrono::steady_clock::time_point,
      std::chrono::steady_clock::duration>;

  /* Generate and log a stack trace.  Stack trace will be written to logger
     regardless of value of 'pri' because it is assumed that a stack trace is
     always interesting enough to log.  If syslog logging is enabled, 'pri'
     will be passed to syslog(). */
  void LogStackTrace(TPri pri) noexcept;

}

/* Facilitates expressions such as the following.

       LOG(TPri::INFO) << "The answer is " << ComputeAnswer();

   Since Log::TLogEntry has a bool conversion operator, the entire
   subexpression following the && operator in the macro below has a type of
   bool, and is not evaluated if Log::IsEnabled(TPri::INFO) returns false,
   avoiding an unnecessary call to ComputeAnswer() if logging at level
   TPri::INFO is disabled.
 */
#define LOG(p) Log::IsEnabled(p) && Log::TLogEntryType(Log::GetLogWriter(), p)

/* Same as LOG(), but appends a strerror() message associated with errno_value
   to log entry before writing. */
#define LOG_ERRNO(p, errno_value) Log::IsEnabled(p) && \
    Log::TLogEntryType(Log::GetLogWriter(), p, false, errno_value)

/* Same as LOG(), but rate limits log messages.  For instance:

       LOG_R(TPri::INFO, std::chrono::seconds(30)) << "The answer is "
           << ComputeAnswer();

   The above will limit log messages to at most one every 30 seconds.
 */
#define LOG_R(p, d) Log::IsEnabled(p) && \
    []() -> Log::TLogRateLimiter & { \
      static Log::TLogRateLimiter lim(&std::chrono::steady_clock::now, \
          std::chrono::steady_clock::duration(d)); \
      return lim; \
    }().Test() && \
    Log::TLogEntryType(Log::GetLogWriter(), p)

/* Same as LOG_R(), but appends a strerror() message associated with
   errno_value to log entry before writing. */
#define LOG_ERRNO_R(p, errno_value, d) Log::IsEnabled(p) && \
    []() -> Log::TLogRateLimiter & { \
      static Log::TLogRateLimiter lim(&std::chrono::steady_clock::now, \
          std::chrono::steady_clock::duration(d)); \
      return lim; \
    }().Test() && \
    Log::TLogEntryType(Log::GetLogWriter(), p, false, errno_value)
