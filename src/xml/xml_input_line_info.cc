/* <xml/xml_input_line_info.cc>

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

   Implements <xml/xml_input_line_info.h>.
 */

#include <xml/xml_input_line_info.h>

#include <memory>

#include <xml/xml_string_util.h>

using namespace Xml;

const char *TXmlInputLineInfo::GetDefaultKey() {
  return "LineInfo";
}

const TXmlInputLineInfo *TXmlInputLineInfo::Get(const xercesc::DOMNode &node,
    const char *line_info_key) {
  assert(&node);

  /* This isn't very efficient, since it creates and destroys a temporary
     transcoded version of 'line_info_key'. */
  return Get(node, GetTranscoded(line_info_key).get());
}

const TXmlInputLineInfo *TXmlInputLineInfo::Get(const xercesc::DOMNode &node) {
  assert(&node);

  /* This isn't very efficient, since it delegates to an inefficient
     implementation. */
  return Get(node, GetDefaultKey());
}
