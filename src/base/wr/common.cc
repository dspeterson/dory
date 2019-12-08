/* <base/wr/common.cc>

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

   Implements <base/wr/common.h>.
 */

#include <base/wr/common.h>

#include <algorithm>
#include <cerrno>
#include <string>

#include <base/error_util.h>
#include <base/no_default_case.h>
#include <base/wr/debug.h>

using namespace Base;

static bool contains(std::initializer_list<int> err_list, int err) noexcept {
  return (std::find(err_list.begin(), err_list.end(), err) != err_list.end());
}

bool Base::Wr::IsFatal(int err, TDisp disp,
    std::initializer_list<int> err_list, bool list_fatal,
    std::initializer_list<int> default_err_list) noexcept {
  bool fatal_if_found = false;

  switch (disp) {
    case TDisp::AddFatal: {
      fatal_if_found = true;

      /* TODO: Once C++17 support is enabled, fallthrough to next case and
         annotate with [[fallthrough]]. */

      if (contains(err_list, err)) {
        return fatal_if_found;
      }

      break;
    }
    case TDisp::AddNonfatal: {
      if (contains(err_list, err)) {
        return fatal_if_found;
      }

      break;
    }
    case TDisp::Fatal: {
      fatal_if_found = true;

      /* TODO: Once C++17 support is enabled, fallthrough to next case and
         annotate with [[fallthrough]]. */

      return (contains(err_list, err) == fatal_if_found);
    }
    case TDisp::Nonfatal: {
      return (contains(err_list, err) == fatal_if_found);
    }
    NO_DEFAULT_CASE;
  }

  return (contains(default_err_list, err) == list_fatal);
}

[[ noreturn ]] void Base::Wr::DieErrnoWr(const char *fn_name,
    int errno_value, int fd1, int fd2) noexcept {
  class wr_die_handler : public TDieHandler {
    public:
    wr_die_handler(int fd1, int fd2)
        : Fd1(fd1),
          Fd2(fd2) {
    }

    void operator()() noexcept override {
      std::string msg("File descriptor info for debugging: fd1: ");
      msg += std::to_string(Fd1);
      msg += ", fd2: ";
      msg += std::to_string(Fd2);
      LogFatal(msg.c_str());
      DumpFdTrackingBuffer();
    }

    private:
    const int Fd1;

    const int Fd2;
  };

  if ((errno_value == EBADF) || (errno_value == ENOTSOCK)) {
    wr_die_handler die_handler(fd1, fd2);
    DieErrno(fn_name, errno_value, &die_handler);
  } else {
    DieErrno(fn_name, errno_value);
  }
}
