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

    ~TTmpFile();

    const std::string &GetName() const {
      return Name;
    }

    const TFd &GetFd() const {
      return Fd;
    }

    void SetDeleteOnDestroy(bool delete_on_destroy) {
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
