/* <base/tmp_file.h>

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

   Generates a temporary file.
 */

#pragma once

#include <cassert>
#include <string>

#include <base/fd.h>
#include <base/no_copy_semantics.h>

namespace Base {

  class TTmpFile final {
    NO_COPY_SEMANTICS(TTmpFile);

    public:
    /* name_template must adhere to the format specified by mkstemps(). */
    TTmpFile(const char *name_template, bool delete_on_destroy);

    TTmpFile(const std::string &name_template, bool delete_on_destroy)
        : TTmpFile(name_template.c_str(), delete_on_destroy) {
    }

    /* Take ownership from 'that', leaving 'that' empty. */
    TTmpFile(TTmpFile &&that) noexcept;

    ~TTmpFile();

    bool IsEmpty() const noexcept {
      assert(this);
      return Name.empty();
    }

    /* If we are nonempty and DeleteOnDestroy is true, delete the file.
       Regardless, reset our internal state to empty, leaving DeleteOnDestroy
       with its prior value. */
    void Reset() noexcept;

    /* Take ownership from 'that', leaving 'that' empty. */
    TTmpFile &operator=(TTmpFile &&that) noexcept;

    void Swap(TTmpFile &that) noexcept;

    const std::string &GetName() const {
      assert(this);
      return Name;
    }

    const TFd &GetFd() const {
      assert(this);
      return Fd;
    }

    bool GetDeleteOnDestroy() const noexcept {
      assert(this);
      return DeleteOnDestroy;
    }

    void SetDeleteOnDestroy(bool delete_on_destroy) {
      assert(this);
      DeleteOnDestroy = delete_on_destroy;
    }

    private:
    /* Name of the temporary file. */
    std::string Name;

    /* Whether we unlink the file on destruction of the object or not. */
    bool DeleteOnDestroy;

    /* Fd associated to the file. */
    TFd Fd;
  };  // TTmpFile

  /* Return a unique filename but leave the file uncreated.  name_template must
     adhere to the format specified by mkstemps(). */
  std::string MakeTmpFilename(const char *name_template);

}  // Base
