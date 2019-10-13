/* <base/wr/file_util.h>

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

   Wrappers for file-related system/library calls.
 */

#pragma once

#include <cassert>
#include <initializer_list>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/wr/common.h>

namespace Base {

  namespace Wr {

    int chdir(TDisp disp, std::initializer_list<int> errors,
        const char *path) noexcept;

    inline int chdir(const char *path) noexcept {
      return chdir(TDisp::AddFatal, {}, path);
    }

    int chmod(TDisp disp, std::initializer_list<int> errors, const char *path,
        mode_t mode) noexcept;

    inline int chmod(const char *path, mode_t mode) noexcept {
      return chmod(TDisp::AddFatal, {}, path, mode);
    }

    int closedir(TDisp disp, std::initializer_list<int> errors,
        DIR *dirp) noexcept;

    inline void closedir(DIR *dirp) noexcept {
      const int ret = closedir(TDisp::AddFatal, {}, dirp);
      assert(ret == 0);
    }

    int fstat(TDisp disp, std::initializer_list<int> errors, int fd,
        struct stat *buf) noexcept;

    inline int fstat(int fd, struct stat *buf) noexcept {
      return fstat(TDisp::AddFatal, {}, fd, buf);
    }

    int ftruncate(TDisp disp, std::initializer_list<int> errors, int fd,
        off_t length) noexcept;

    inline int ftruncate(int fd, off_t length) noexcept {
      return ftruncate(TDisp::AddFatal, {}, fd, length);
    }

    char *mkdtemp(TDisp disp, std::initializer_list<int> errors,
        char *tmpl) noexcept;

    inline char *mkdtemp(char *tmpl) noexcept {
      return mkdtemp(TDisp::AddFatal, {}, tmpl);
    }

    int mkstemps(TDisp disp, std::initializer_list<int> errors, char *tmpl,
        int suffixlen) noexcept;

    inline int mkstemps(char *tmpl, int suffixlen) noexcept {
      return mkstemps(TDisp::AddFatal, {}, tmpl, suffixlen);
    }

    int open(TDisp disp, std::initializer_list<int> errors,
        const char *pathname, int flags) noexcept;

    int open(TDisp disp, std::initializer_list<int> errors,
        const char *pathname, int flags, mode_t mode) noexcept;

    inline int open(const char *pathname, int flags) noexcept {
      return open(TDisp::AddFatal, {}, pathname, flags);
    }

    inline int open(const char *pathname, int flags, mode_t mode) noexcept {
      return open(TDisp::AddFatal, {}, pathname, flags, mode);
    }

    DIR *opendir(TDisp disp, std::initializer_list<int> errors,
        const char *name) noexcept;

    inline DIR *opendir(const char *name) noexcept {
      return opendir(TDisp::AddFatal, {}, name);
    }

    int readdir_r(TDisp disp, std::initializer_list<int> errors, DIR *dirp,
        dirent *entry, dirent **result) noexcept;

    inline void readdir_r(DIR *dirp, dirent *entry, dirent **result) noexcept {
      const int ret = readdir_r(TDisp::AddFatal, {}, dirp, entry, result);
      assert(ret == 0);
    }

    int rename(TDisp disp, std::initializer_list<int> errors,
        const char *oldpath, const char *newpath) noexcept;

    inline int rename(const char *oldpath, const char *newpath) noexcept {
      return rename(TDisp::AddFatal, {}, oldpath, newpath);
    }

    int stat(TDisp disp, std::initializer_list<int> errors, const char *path,
        struct stat *buf) noexcept;

    inline int stat(const char *path, struct stat *buf) noexcept {
      return stat(TDisp::AddFatal, {}, path, buf);
    }

    int truncate(TDisp disp, std::initializer_list<int> errors,
        const char *path, off_t length) noexcept;

    inline int truncate(const char *path, off_t length) noexcept {
      return truncate(TDisp::AddFatal, {}, path, length);
    }

    int unlink(TDisp disp, std::initializer_list<int> errors,
        const char *pathname) noexcept;

    inline int unlink(const char *pathname) noexcept {
      return unlink(TDisp::AddFatal, {}, pathname);
    }

  }  // Wr

}  // Base
