/* <base/dir_iter.cc>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   Implements <base/dir_iter.h>.
 */

#include <base/dir_iter.h>

#include <cerrno>
#include <cstring>

#include <base/wr/file_util.h>

using namespace Base;

TDirIter::TDirIter(const char *dir)
    : Handle(Wr::opendir(dir)) {
  if (!Handle) {
    ThrowSystemError(errno);
  }
}

TDirIter::~TDirIter() {
  Wr::closedir(Handle);
}

void TDirIter::Rewind() noexcept {
  rewinddir(Handle);
  Pos = NotFresh;
}

bool TDirIter::TryRefresh() const noexcept {
  while (Pos == NotFresh) {
    dirent *const ptr = Wr::readdir(Handle);

    if (ptr) {
      DirEnt = *ptr;

      if (DirEnt.d_type != DT_DIR || (std::strcmp(DirEnt.d_name, "..") &&
          std::strcmp(DirEnt.d_name, "."))) {
        Pos = AtEntry;
      }
    } else {
      Pos = AtEnd;
    }
  }

  return (Pos == AtEntry);
}
