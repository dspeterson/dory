/* <base/tmp_dir.h>

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

   Generates a temporary directory.
 */

#pragma once

#include <cassert>
#include <string>

#include <base/no_copy_semantics.h>

namespace Base {

  class TTmpDir final {
    NO_COPY_SEMANTICS(TTmpDir);

    public:
    TTmpDir(const char *name_template, bool delete_on_destroy);

    ~TTmpDir();

    const std::string &GetName() const {
      assert(this);
      return Name;
    }

    void SetDeleteOnDestroy(bool delete_on_destroy) {
      assert(this);
      DeleteOnDestroy = delete_on_destroy;
    }

    private:
    std::string Name;

    bool DeleteOnDestroy;
  };  // TTmpDir

}  // Base
