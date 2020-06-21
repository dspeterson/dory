/* <xml/test/xml_test_initializer.cc>

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

   Implements <xml/test/xml_test_initializer.h>.
 */

#include <xml/test/xml_test_initializer.h>

#include <cstdlib>
#include <iostream>

#include <xml/xml_string_util.h>

using namespace xercesc;

using namespace Xml;
using namespace Xml::Test;

bool TXmlTestInitializer::HandleInitError(const XMLException &x) {
  std::cerr << "Xerces XML library initialization error: "
      << TranscodeToString(x.getMessage()) << std::endl;
  std::exit(1);
  return true;  // keep compiler happy
}

void TXmlTestInitializer::HandleCleanupError(const XMLException &x) noexcept {
  try {
    std::cerr << "Xerces XML library cleanup error: "
        << TranscodeToString(x.getMessage()) << std::endl;
    std::exit(1);
  } catch (...) {
  }
}

void TXmlTestInitializer::HandleUnknownErrorOnCleanup() noexcept {
  try {
    std::cerr << "Unknown error while doing Xerces XML library cleanup"
        << std::endl;
    std::exit(1);
  } catch (...) {
  }
}
