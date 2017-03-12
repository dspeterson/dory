/* <dory/util/handle_xml_errors.h>

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

   Utility function for handling exceptions related to XML config files, and
   creating corresponding error message strings.
 */

#pragma once

#include <functional>
#include <string>

#include <base/opt.h>

namespace Dory {

  namespace Util {

    /* Input parameter 'fn' is assumed to do some processing of XML config file
       contents, and may throw an exception defined in
       <xml/config/config_errors.h>.  Call 'fn', and catch any of these
       exceptions it throws.  If an exception is caught, create and return a
       corresponding error message.  If no exception is caught, return a value
       in the unknown state. */
    Base::TOpt<std::string> HandleXmlErrors(const std::function<void()> &fn);

  }  // Util

}  // Dory
