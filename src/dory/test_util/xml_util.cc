/* <dory/test_util/xml_util.cc>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/test_util/xml_util.h>.
 */

#include <dory/test_util/xml_util.h>

#include <stdexcept>

#include <base/opt.h>
#include <dory/util/handle_xml_errors.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Conf;
using namespace Dory::TestUtil;
using namespace Dory::Util;

TConf Dory::TestUtil::XmlToConf(const std::string &xml) {
  TConf conf;
  TOpt<std::string> opt_err_msg = HandleXmlErrors(
      [&xml, &conf]() -> void {
        conf = Dory::Conf::TConf::TBuilder(
            true /* allow_input_bind_ephemeral */,
            true /* enable_lz4 */).Build(xml);
      }
  );

  if (opt_err_msg.IsKnown()) {
    throw std::runtime_error(*opt_err_msg);
  }

  return conf;
}
