/* <base/dynamic_lib.cc>

   ----------------------------------------------------------------------------
   Copyright 2014 if(we)

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

   Implements <base/dynamic_lib.h>.
 */

#include <base/dynamic_lib.h>

using namespace Base;

std::string TDynamicLib::TLibLoadError::CreateMsg(const char *libname) {
  assert(libname);
  std::string msg("Failed to load library [");
  msg += libname;
  msg += "]";
  return msg;
}

std::string TDynamicLib::TSymLoadError::CreateMsg(const char *libname,
    const char *symname) {
  assert(libname);
  assert(symname);
  std::string msg("Failed to load symbol [");
  msg += symname;
  msg += "] for library [";
  msg += libname;
  msg += "]";
  return msg;
}

TDynamicLib::TDynamicLib(const char *libname, int flags)
    : LibName(libname),
      Handle(dlopen(libname, flags)) {
  if (Handle == nullptr) {
    throw TLibLoadError(libname);
  }
}
