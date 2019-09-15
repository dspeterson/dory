/* <base/tmp_dir.cc>

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

   Implements <base/tmp_dir.h>.
 */

#include <base/tmp_dir.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <base/error_util.h>

using namespace Base;

TTmpDir::TTmpDir(const char *name_template, bool delete_on_destroy)
    : DeleteOnDestroy(delete_on_destroy) {
  std::vector<char> name_buf(1 + std::strlen(name_template));
  std::strncpy(&name_buf[0], name_template, name_buf.size());

  if (mkdtemp(&name_buf[0]) == nullptr) {
    ThrowSystemError(errno);
  }

  Name = &name_buf[0];
}

TTmpDir::~TTmpDir() {
  if (DeleteOnDestroy) {
    std::string cmd("/bin/rm -fr ");
    cmd += Name;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    std::system(cmd.c_str());
#pragma GCC diagnostic pop
  }
}
