/* <dory/util/dory_xml_init.h>

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

   Utility class for initializing XML processing library.
 */

#pragma once

#include <xercesc/util/XMLException.hpp>

#include <xml/xml_initializer.h>

namespace Dory {

  namespace Util {

    class TDoryXmlInit final : public Xml::TXmlInitializer {
      public:
      TDoryXmlInit()
          : TXmlInitializer(false) {
      }

      protected:
      virtual bool HandleInitError(const xercesc::XMLException &x);

      virtual void HandleCleanupError(const xercesc::XMLException &x) noexcept;

      virtual void HandleUnknownErrorOnCleanup() noexcept;
    };  // TDoryXmlInit

  }  // Util

}  // Dory
