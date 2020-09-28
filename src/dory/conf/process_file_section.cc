/* <dory/conf/process_file_section.cc>

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

   Implements <dory/conf/process_common_logging.h>.
 */

#include <dory/conf/process_file_section.h>

#include <cassert>
#include <unordered_map>

#include <base/to_integer.h>
#include <xml/config/config_errors.h>
#include <xml/config/config_util.h>

using namespace xercesc;

using namespace Base;
using namespace Dory;
using namespace Dory::Conf;
using namespace Xml;
using namespace Xml::Config;

using TOpts = TAttrReader::TOpts;

std::pair<std::string, TOpt<mode_t>>
Dory::Conf::ProcessFileSection(const DOMElement &file_section,
    bool allow_relative_path) {
  const bool enable = TAttrReader::GetBool(file_section, "enable");
  RequireAllChildElementLeaves(file_section);
  std::unordered_map<std::string, const DOMElement *> subsection_map =
      GetSubsectionElements(file_section, {{"path", true}, {"mode", false}},
          false);
  const DOMElement &path_elem = *subsection_map.at("path");

  /* Make sure path element has a value attribute, even if enable is false. */
  std::string path = TAttrReader::GetString(path_elem, "value");

  if (!enable) {
    path.clear();
  }

  TOpt<mode_t> mode;
  const DOMElement *mode_elem = nullptr;

  if (subsection_map.count("mode")) {
    mode_elem = subsection_map.at("mode");
    mode = TAttrReader::GetOptUnsigned<mode_t>(*mode_elem, "value",
        "unspecified", 0 | TBase::BIN | TBase::OCT,
        TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
  }

  if (!allow_relative_path && !path.empty() && (path[0] != '/')) {
      throw TInvalidAttr(path_elem, "value", path.c_str(),
          "Path must be absolute");
  }

  if (mode.IsKnown() && (*mode > 0777)) {
      assert(mode_elem);
      throw TInvalidAttr(*mode_elem, "value",
          TAttrReader::GetString(*mode_elem, "value").c_str(),
          "File mode must be <= 0777");
  }

  return std::make_pair(std::move(path), mode);
}
