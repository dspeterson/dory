/* <dory/util/exceptions.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Implements <dory/util/exceptions.h>.
 */

#include <dory/util/exceptions.h>

using namespace Dory;
using namespace Dory::Util;

static std::string MakeOpenErrorMsg(const char *filename) {
  std::string msg("Error opening file: [");
  msg += filename;
  msg += "]";
  return msg;
}

TFileOpenError::TFileOpenError(const char *filename)
    : std::runtime_error(MakeOpenErrorMsg(filename)) {
}

static std::string MakeReadErrorMsg(const char *filename) {
  std::string msg("Error reading from file: [");
  msg += filename;
  msg += "]";
  return msg;
}

TFileReadError::TFileReadError(const char *filename)
    : std::runtime_error(MakeReadErrorMsg(filename)) {
}

static std::string MakeWriteErrorMsg(const char *filename) {
  std::string msg("Error writing to file: [");
  msg += filename;
  msg += "]";
  return msg;
}

TFileWriteError::TFileWriteError(const char *filename)
    : std::runtime_error(MakeWriteErrorMsg(filename)) {
}
