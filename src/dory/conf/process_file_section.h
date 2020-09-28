/* <dory/conf/process_file_section.h>

   ----------------------------------------------------------------------------
   Copyright 2020 Dave Peterson <dave@dspeterson.com>

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

   Utility function for processing a config file element that specifies a file.
 */

#pragma once

#include <string>
#include <utility>

#include <sys/types.h>
#include <xercesc/dom/DOMElement.hpp>

#include <base/opt.h>

namespace Dory {

  namespace Conf {

    /* Process a config file element that specifies a file.  The first item of
       the returned pair will be the filename (including path if given) if a
       file was specified, or otherwise the empty string.  The second item of
       the returned pair privides the file mode if specified. */
    std::pair<std::string, Base::TOpt<mode_t>>
    ProcessFileSection(const xercesc::DOMElement &file_section,
        bool allow_relative_path = false);

  }  // Conf

}  // Dory
