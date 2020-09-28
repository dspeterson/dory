/* <dory/conf/process_common_logging.h>

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

   Utility function for processing common logging elements in config file.
   This is for sharing between Dory and the mock Kafka server.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <xercesc/dom/DOMElement.hpp>

#include <dory/conf/common_logging_conf.h>

namespace Dory {

  namespace Conf {

    /* Process common logging config elements obtained from subtree rooted at
       'logging_elem' and store result in 'conf'.  If any extra subsections are
       expected, specify them in 'extra_subsection_vec' where first item in
       pair is subsection name.  A true value for second item in pair indicates
       extra subsection is required, and false indicates optional.  If
       allow_unknown_subsection is true, additional subsections beyond those
       expected in the common part of the config and those in
       'extra_subsection_vec' are allowed.  Return a map containing any
       subsections specified in 'extra_subsection_vec' that were found, as well
       as any unknown subsections found if 'allow_unknown_subsection' is true.
     */
    std::unordered_map<std::string, const xercesc::DOMElement *>
    ProcessCommonLogging(const xercesc::DOMElement &logging_elem,
        TCommonLoggingConf &conf,
        const std::vector<std::pair<std::string, bool>> &extra_subsection_vec,
        bool allow_unknown_subsection);

  }  // Conf

}  // Dory
