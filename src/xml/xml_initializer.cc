/* <xml/xml_initializer.cc>

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

   Implements <xml/xml_initializer.h>.
 */

#include <xml/xml_initializer.h>

#include <xercesc/util/PlatformUtils.hpp>

using namespace xercesc;

using namespace Xml;

TXmlInitializer::~TXmlInitializer() {
  Cleanup();
}

bool TXmlInitializer::Init() {
  if (!Initialized) {
    try {
      XMLPlatformUtils::Initialize();
      Initialized = true;
    } catch (const XMLException &x) {
      if (HandleInitError(x)) {
        throw;  // subclass requested exception to be rethrown
      }
    }
  }

  return Initialized;
}

bool TXmlInitializer::Cleanup() noexcept {
  if (!Initialized) {
    return true;  // no-op success
  }

  /* Set this to false before we even try doing our cleanup.  If the cleanup
     fails, we gave it our best effort and will not retry if called again. */
  Initialized = false;

  bool success = false;

  try {
    XMLPlatformUtils::Terminate();
    success = true;
  } catch (const XMLException &x) {
    HandleCleanupError(x);
  } catch (...) {
    /* Just in case the library throws some weird undocumented exception. */
    HandleUnknownErrorOnCleanup();
  }

  return success;
}

TXmlInitializer::TXmlInitializer(bool init_on_construction) {
  if (init_on_construction) {
    Init();
  }
}
