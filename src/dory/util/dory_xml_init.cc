/* <dory/util/dory_xml_init.cc>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/util/dory_xml_init.h>.
 */

#include <stdexcept>
#include <string>

#include <dory/util/dory_xml_init.h>
#include <log/log.h>
#include <xml/xml_string_util.h>

using namespace xercesc;

using namespace Dory;
using namespace Dory::Util;
using namespace Log;
using namespace Xml;

bool TDoryXmlInit::HandleInitError(const XMLException &x) {
  std::string msg("Xerces XML library initialization error: ");
  msg += TranscodeToString(x.getMessage());
  throw std::runtime_error(msg);
}

void TDoryXmlInit::HandleCleanupError(const XMLException &x) noexcept {
  try {
    LOG(TPri::ERR) << "Xerces XML library cleanup error: "
        << TranscodeToString(x.getMessage());
  } catch (...) {
    LOG(TPri::ERR) << "Xerces XML library cleanup error";
  }
}

void TDoryXmlInit::HandleUnknownErrorOnCleanup() noexcept {
  LOG(TPri::ERR) << "Unknown error while doing Xerces XML library cleanup";
}
