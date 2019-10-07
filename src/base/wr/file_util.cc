/* <base/wr/file_util.cc>

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

   Implements <base/wr/file_util.h>.
 */

#include <base/wr/file_util.h>

#include <cerrno>

#include <unistd.h>

#include <base/error_util.h>

using namespace Base;

int Base::Wr::closedir(TDisp disp, std::initializer_list<int> errors,
    DIR *dirp) noexcept {
  const int ret = ::closedir(dirp);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF})) {
    DieErrno("closedir()", errno);
  }

  return ret;
}

int Base::Wr::fstat(TDisp disp, std::initializer_list<int> errors, int fd,
    struct stat *buf) noexcept {
  const int ret = ::fstat(fd, buf);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EBADF, EFAULT, ENOMEM})) {
    DieErrno("fstat()", errno);
  }

  return ret;
}

int Base::Wr::ftruncate(TDisp disp, std::initializer_list<int> errors,
    int fd, off_t length) noexcept {
  const int ret = ::ftruncate(fd, length);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EINVAL, EBADF})) {
    DieErrno("ftruncate()", errno);
  }

  return ret;
}

char *Base::Wr::mkdtemp(TDisp disp, std::initializer_list<int> errors,
    char *tmpl) noexcept {
  char *const ret = ::mkdtemp(tmpl);

  if ((ret == nullptr) && IsFatal(errno, disp, errors,
      true /* default_fatal */, {EINVAL, EFAULT, ENOMEM})) {
    DieErrno("mkdtemp()", errno);
  }

  return ret;
}

int Base::Wr::mkstemps(TDisp disp, std::initializer_list<int> errors,
    char *tmpl, int suffixlen) noexcept {
  const int ret = ::mkstemps(tmpl, suffixlen);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EEXIST, EINVAL, EFAULT, EMFILE, ENFILE, ENOMEM})) {
    DieErrno("mkstemps()", errno);
  }

  return ret;
}

int Base::Wr::open(TDisp disp, std::initializer_list<int> errors,
    const char *pathname, int flags) noexcept {
  const int ret = ::open(pathname, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EMFILE, ENFILE, ENOMEM})) {
    DieErrno("open()", errno);
  }

  return ret;
}

int Base::Wr::open(TDisp disp, std::initializer_list<int> errors,
    const char *pathname, int flags, mode_t mode) noexcept {
  const int ret = ::open(pathname, flags, mode);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EMFILE, ENFILE, ENOMEM})) {
    DieErrno("open()", errno);
  }

  return ret;
}

DIR *Base::Wr::opendir(TDisp disp, std::initializer_list<int> errors,
    const char *name) noexcept {
  DIR *const ret = ::opendir(name);

  if ((ret == nullptr) && IsFatal(errno, disp, errors,
      true /* default_fatal */, {EMFILE, ENFILE, ENOMEM})) {
    DieErrno("opendir()", errno);
  }

  return ret;
}

int Base::Wr::readdir_r(TDisp disp, std::initializer_list<int> errors,
    DIR *dirp, dirent *entry, dirent **result) noexcept {
  const int ret = ::readdir_r(dirp, entry, result);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* default_fatal */,
      {EBADF})) {
    DieErrno("readdir_r()", ret);
  }

  return ret;
}

int Base::Wr::truncate(TDisp disp, std::initializer_list<int> errors,
    const char *path, off_t length) noexcept {
  const int ret = ::truncate(path, length);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, EINVAL, EBADF})) {
    DieErrno("truncate()", errno);
  }

  return ret;
}

int Base::Wr::unlink(TDisp disp, std::initializer_list<int> errors,
    const char *pathname) noexcept {
  const int ret = ::unlink(pathname);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* default_fatal */,
      {EFAULT, ENOMEM})) {
    DieErrno("unlink()", errno);
  }

  return ret;
}
