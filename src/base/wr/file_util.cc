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
#include <cstdio>

#include <unistd.h>

#include <base/error_util.h>
#include <base/wr/debug.h>

using namespace Base;

int Base::Wr::chdir(TDisp disp, std::initializer_list<int> errors,
    const char *path) noexcept {
  const int ret = ::chdir(path);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, ENOMEM})) {
    DieErrnoWr("chdir()", errno);
  }

  return ret;
}

int Base::Wr::chmod(TDisp disp, std::initializer_list<int> errors,
    const char *path, mode_t mode) noexcept {
  const int ret = ::chmod(path, mode);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, ENOMEM})) {
    DieErrnoWr("chmod()", errno);
  }

  return ret;
}

int Base::Wr::closedir(TDisp disp, std::initializer_list<int> errors,
    DIR *dirp) noexcept {
  const int ret = ::closedir(dirp);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF})) {
    DieErrnoWr("closedir()", errno);
  }

  return ret;
}

int Base::Wr::fstat(TDisp disp, std::initializer_list<int> errors, int fd,
    struct stat *buf) noexcept {
  const int ret = ::fstat(fd, buf);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, ENOMEM})) {
    DieErrnoWr("fstat()", errno);
  }

  return ret;
}

int Base::Wr::ftruncate(TDisp disp, std::initializer_list<int> errors,
    int fd, off_t length) noexcept {
  const int ret = ::ftruncate(fd, length);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EINVAL, EBADF})) {
    DieErrnoWr("ftruncate()", errno);
  }

  return ret;
}

char *Base::Wr::mkdtemp(TDisp disp, std::initializer_list<int> errors,
    char *tmpl) noexcept {
  char *const ret = ::mkdtemp(tmpl);

  if ((ret == nullptr) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EINVAL, EFAULT, ENOMEM})) {
    DieErrnoWr("mkdtemp()", errno);
  }

  return ret;
}

int Base::Wr::mkstemps(TDisp disp, std::initializer_list<int> errors,
    char *tmpl, int suffixlen) noexcept {
  const int ret = ::mkstemps(tmpl, suffixlen);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EEXIST, EINVAL, EFAULT, EMFILE, ENFILE, ENOMEM})) {
    DieErrnoWr("mkstemps()", errno);
  }

  return ret;
}

int Base::Wr::open(TDisp disp, std::initializer_list<int> errors,
    const char *pathname, int flags) noexcept {
  const int ret = ::open(pathname, flags);

  if ((ret < 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EMFILE, ENFILE, ENOMEM})) {
    DieErrnoWr("open()", errno);
  }

  return ret;
}

int Base::Wr::open(TDisp disp, std::initializer_list<int> errors,
    const char *pathname, int flags, mode_t mode) noexcept {
  const int ret = ::open(pathname, flags, mode);

  if (ret < 0) {
    if (IsFatal(errno, disp, errors, true /* list_fatal */,
        {EFAULT, EMFILE, ENFILE, ENOMEM})) {
      DieErrnoWr("open()", errno);
    }
  } else {
    TrackFdOp(TFdOp::Create1, ret);
  }

  return ret;
}

DIR *Base::Wr::opendir(TDisp disp, std::initializer_list<int> errors,
    const char *name) noexcept {
  DIR *const ret = ::opendir(name);

  if ((ret == nullptr) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EMFILE, ENFILE, ENOMEM})) {
    DieErrnoWr("opendir()", errno);
  }

  return ret;
}

int Base::Wr::readdir_r(TDisp disp, std::initializer_list<int> errors,
    DIR *dirp, dirent *entry, dirent **result) noexcept {
  const int ret = ::readdir_r(dirp, entry, result);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* list_fatal */,
      {EBADF})) {
    DieErrnoWr("readdir_r()", ret);
  }

  return ret;
}

int Base::Wr::rename(TDisp disp, std::initializer_list<int> errors,
    const char *oldpath, const char *newpath) noexcept {
  const int ret = ::rename(oldpath, newpath);

  if ((ret != 0) && IsFatal(ret, disp, errors, true /* list_fatal */,
      {EFAULT, ENOMEM})) {
    DieErrnoWr("rename()", ret);
  }

  return ret;
}

int Base::Wr::stat(TDisp disp, std::initializer_list<int> errors,
    const char *path, struct stat *buf) noexcept {
  const int ret = ::stat(path, buf);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EBADF, EFAULT, ENOMEM})) {
    DieErrnoWr("stat()", errno);
  }

  return ret;
}

int Base::Wr::truncate(TDisp disp, std::initializer_list<int> errors,
    const char *path, off_t length) noexcept {
  const int ret = ::truncate(path, length);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, EINVAL, EBADF})) {
    DieErrnoWr("truncate()", errno);
  }

  return ret;
}

int Base::Wr::unlink(TDisp disp, std::initializer_list<int> errors,
    const char *pathname) noexcept {
  const int ret = ::unlink(pathname);

  if ((ret != 0) && IsFatal(errno, disp, errors, true /* list_fatal */,
      {EFAULT, ENOMEM})) {
    DieErrnoWr("unlink()", errno);
  }

  return ret;
}
