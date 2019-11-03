/* <base/wr/debug.cc>

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

   Implements <base/wr/debug.h>.
 */

#include <base/wr/debug.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <string>
#include <sstream>

#include <execinfo.h>

#include <base/error_util.h>

using namespace Base;

#ifdef TRACK_FILE_DESCRIPTORS

namespace {

  const size_t TRACE_DEPTH = 8;  // stack trace depth

  const size_t BUF_SIZE = 16384;

  struct FdLogEntry {
    FdLogEntry() noexcept = default;

    FdLogEntry(Wr::TFdOp op, int fd1, int fd2) noexcept
        : Op(op),
          Fd1(fd1),
          Fd2(fd2) {
    }

    size_t OpNum = 0;

    Wr::TFdOp Op = Wr::TFdOp::Create1;

    int Fd1 = -1;

    int Fd2 = -1;

    void *Trace[TRACE_DEPTH] = { nullptr };
  };

  size_t OpCount = 0;

  FdLogEntry Buf[BUF_SIZE];  // circular buffer

  /* Calling DumpFdTrackingBuffer sets this to true and prevents recording of
     additional entries. */
  bool Dumping = false;

  std::mutex BufMutex;  // protects OpCount, Buf, and Dumping

};

void Base::Wr::TrackFdOp(Base::Wr::TFdOp op, int fd1, int fd2) noexcept {
  std::lock_guard<std::mutex> lock(BufMutex);

  if (!Dumping) {
    FdLogEntry &dst = Buf[OpCount % BUF_SIZE];
    dst.OpNum = OpCount;
    ++OpCount;
    dst.Op = op;
    dst.Fd1 = fd1;
    dst.Fd2 = fd2;

    /* TODO: Measure how long a typical call to backtrace() takes, and consider
       creating stack trace before acquiring lock and then copying it using
       memcpy() while lock is held.  For now, do things this way because the
       other approach triggers an apparently spurious address sanitizer
       stack-use-after-scope failure when using gcc (GCC) 8.2.1 20180905
       (Red Hat 8.2.1-3). */
    const size_t num_frames = static_cast<size_t>(backtrace(dst.Trace,
        static_cast<int>(TRACE_DEPTH)));
    std::memset(&dst.Trace[num_frames], 0,
        (TRACE_DEPTH - num_frames) * sizeof(dst.Trace[0]));
  }
}

static const char *ToString(Wr::TFdOp op) {
  switch (op) {
    case Wr::TFdOp::Create1: {
      return "Create1";
    }
    case Wr::TFdOp::Create2: {
      return "Create2";
    }
    case Wr::TFdOp::Dup: {
      return "Dup";
    }
    case Wr::TFdOp::Close: {
      return "Close";
    }
    default: {
      break;
    }
  }

  return "unknown";
}

static void DumpEntry(const FdLogEntry &entry) {
  static const char indent[] = "    ";
  std::string line(indent);
  line += "op num: ";
  line += std::to_string(entry.OpNum);
  line += ", type: ";
  line += ToString(entry.Op);
  line += ", fd1: ";
  line += std::to_string(entry.Fd1);
  line += ", fd2: ";
  line += std::to_string(entry.Fd2);
  LogFatal(line.c_str());

  for (size_t i = 0; i < TRACE_DEPTH; ++i) {
    std::ostringstream os;
    os << indent << indent << "0x" << std::hex << std::setw(16)
        << std::setfill('0')
        << reinterpret_cast<unsigned long>(entry.Trace[i]);
    LogFatal(os.str().c_str());
  }
}

void Base::Wr::DumpFdTrackingBuffer() noexcept {
  bool WasDumping = false;

  {
    std::lock_guard<std::mutex> lock(BufMutex);
    WasDumping = Dumping;
    Dumping = true;
  }

  /* At this point, we no longer need to hold the mutex because the true value
     for Dumping prevents further modifications. */

  if (WasDumping) {
    /* Log buffer has already been dumped, or is being dumped by another
       thread. */
    return;
  }

  size_t entry_count = std::min(OpCount, BUF_SIZE);
  std::string msg("Dumping FD tracking buffer of size ");
  msg += std::to_string(entry_count);
  LogFatal(msg.c_str());

  /* Dump log entries from mose recent to oldest. */
  while (entry_count) {
    --entry_count;
    DumpEntry(Buf[entry_count % BUF_SIZE]);
  }
}

#else

void Base::Wr::TrackFdOp(Base::Wr::TFdOp /*op*/, int /*fd1*/,
    int /*fd2*/) noexcept {
  /* no-op */
}

void Base::Wr::DumpFdTrackingBuffer() noexcept {
  /* no-op */
}

#endif
