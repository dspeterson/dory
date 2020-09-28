/* <dory/conf/process_common_logging.cc>

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

#include <dory/conf/process_common_logging.h>

#include <algorithm>
#include <stdexcept>

#include <base/opt.h>
#include <dory/conf/process_file_section.h>
#include <log/pri.h>
#include <xml/config/config_errors.h>
#include <xml/config/config_util.h>

using namespace xercesc;

using namespace Base;
using namespace Dory;
using namespace Dory::Conf;
using namespace Log;
using namespace Xml;
using namespace Xml::Config;

std::unordered_map<std::string, const DOMElement *>
Dory::Conf::ProcessCommonLogging(const DOMElement &logging_elem,
    TCommonLoggingConf &conf,
    const std::vector<std::pair<std::string, bool>> &extra_subsection_vec,
    bool allow_unknown_subsection) {
  const std::vector<std::pair<std::string, bool>> common_subsection_vec({
      {"level", false}, {"stdoutStderr", false}, {"syslog", false},
      {"file", false}
  });

  auto subsection_vec = common_subsection_vec;

  for (const auto &item : extra_subsection_vec) {
    if (std::find_if(subsection_vec.cbegin(), subsection_vec.cend(),
        [&item](const std::pair<std::string, bool> &x) {
          return x.first == item.first;
        }) != subsection_vec.cend()) {
      throw std::logic_error("Duplicate logging subsection");
    }

    subsection_vec.push_back(item);
  }

  auto subsection_map = GetSubsectionElements(logging_elem, subsection_vec,
      allow_unknown_subsection);

  if (subsection_map.count("level")) {
    const DOMElement &elem = *subsection_map.at("level");
    RequireLeaf(elem);
    const std::string level = TAttrReader::GetString(elem, "value");

    try {
      conf.Pri = ToPri(level);
    } catch (const std::range_error &) {
      throw TInvalidAttr(elem, "value", level.c_str(),
          "Logging level must be one of {EMERG, ALERT, CRIT, ERR, WARNING, "
          "NOTICE, INFO, DEBUG}");
    }
  }

  if (subsection_map.count("stdoutStderr")) {
    const DOMElement &elem = *subsection_map.at("stdoutStderr");
    RequireLeaf(elem);
    conf.EnableStdoutStderr = TAttrReader::GetBool(elem, "enable");
  }

  if (subsection_map.count("syslog")) {
    const DOMElement &elem = *subsection_map.at("syslog");
    RequireLeaf(elem);
    conf.EnableSyslog = TAttrReader::GetBool(elem, "enable");
  }

  if (subsection_map.count("file")) {
    const auto file_conf = ProcessFileSection(*subsection_map.at("file"));
    conf.SetFileConf(file_conf.first, file_conf.second);
  }

  // Erase items we have already processed before returning result.
  for (const auto &item : common_subsection_vec) {
    subsection_map.erase(item.first);
  }

  return subsection_map;
}
