/* <base/tmp_file.cc>

   ----------------------------------------------------------------------------
   Copyright 2013 if(we)
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

   Implements <base/tmp_file.h>.
 */

#include <base/tmp_file.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include <unistd.h>

#include <base/error_util.h>
#include <base/wr/file_util.h>

using namespace Base;

TTmpFile::TTmpFile(const char *name_template, bool delete_on_destroy)
    : DeleteOnDestroy(delete_on_destroy) {
  const size_t len = std::strlen(name_template);

  /* Number of consecutive 'X' chars found. */
  size_t x_count = 0;

  size_t i = len;

  /* Search 'name_template' in reverse for "XXXXXX".  If found, x_count will be
     6 and i will be the start index of the found substring on loop
     termination.  Otherwise, x_count will be left with some value < 6. */
  while (i) {
    --i;

    if (name_template[i] == 'X') {
      if (++x_count == 6) {
        break;
      }
    } else {
      x_count = 0;
    }
  }

  /* Verify that we found "XXXXXX". */
  assert(x_count == 6);

  assert(i <= len);
  assert((len - i) >= 6);

  /* Compute # of remaining chars after "XXXXXX". */
  const size_t suffix_len = len - i - 6;

  std::vector<char> name_buf(name_template, name_template + len + 1);
  Fd = IfLt0(Wr::mkstemps(&name_buf[0], static_cast<int>(suffix_len)));
  Name = &name_buf[0];
}

TTmpFile::TTmpFile(TTmpFile &&that) noexcept
    : Name(std::move(that.Name)),
      DeleteOnDestroy(that.DeleteOnDestroy),
      Fd(std::move(that.Fd)) {
  /* Make sure 'that' doesn't attempt to delete file on destruction. */
  that.Name.clear();

  assert(!that.Fd.IsOpen());
}

TTmpFile::~TTmpFile() {
  Reset();
}

void TTmpFile::Reset() noexcept {
  if (DeleteOnDestroy && !Name.empty()) {
    Wr::unlink(Name.c_str());
  }

  Name.clear();
  Fd.Reset();
}

TTmpFile &TTmpFile::operator=(TTmpFile &&that) noexcept {
  if (&that != this) {
    Reset();
    Name = std::move(that.Name);
    DeleteOnDestroy = that.DeleteOnDestroy;
    Fd = std::move(that.Fd);

    /* Make sure 'that' doesn't attempt to delete file on destruction. */
    that.Name.clear();
  }

  return *this;
}

void TTmpFile::Swap(TTmpFile &that) noexcept {
  Name.swap(that.Name);
  std::swap(DeleteOnDestroy, that.DeleteOnDestroy);
  Fd.Swap(that.Fd);
}

std::string Base::MakeTmpFilename(const char *name_template) {
  TTmpFile tmp_file(name_template, true /* delete_on_destroy */);
  return tmp_file.GetName();
}
