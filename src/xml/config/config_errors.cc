/* <xml/config/config_errors.cc>

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

   Implements <xml/config/config_errors.h>.
 */

#include <xml/config/config_errors.h>

#include <memory>

#include <xml/xml_input_line_info.h>
#include <xml/xml_string_util.h>

using namespace xercesc;

using namespace Xml;
using namespace Xml::Config;

TXmlException::TXmlException(const XMLException &x)
    : TXmlConfigError(GetTranscoded(x.getMessage()).get()),
      Line(x.getSrcLine()) {
}

size_t TXmlException::GetLine() const noexcept {
  assert(this);
  return Line;
}

TSaxParseException::TSaxParseException(const SAXParseException &x)
    : TXmlConfigError(GetTranscoded(x.getMessage()).get()),
      Line(x.getLineNumber()),
      Column(x.getColumnNumber()) {
}

size_t TSaxParseException::GetLine() const noexcept {
  assert(this);
  return Line;
}

size_t TSaxParseException::GetColumn() const noexcept {
  assert(this);
  return Column;
}

TDomException::TDomException(const DOMException &x)
    : TXmlConfigError(GetTranscoded(x.getMessage()).get()) {
}

TWrongEncoding::TWrongEncoding(const char *encoding,
    const char *expected_encoding)
    : TDocumentEncodingError("XML document has wrong encoding"),
      Encoding(encoding),
      ExpectedEncoding(expected_encoding) {
}

TContentError::TContentError(const char *msg, const DOMNode &location)
    : TXmlConfigError(msg),
      Line(0),
      Column(0) {
  const TXmlInputLineInfo *info = TXmlInputLineInfo::Get(location);

  if (info) {
    Line = info->GetLineNum();
    Column = info->GetColumnNum();
  }
}

TElementError::TElementError(const char *msg, const DOMElement &location)
    : TContentError(msg, location),
      ElementName(GetTranscoded(location.getNodeName()).get()) {
}

static std::string MakeInvalidBoolAttrMsg(const char *true_value,
    const char *false_value) {
  std::string msg("XML attribute must be either \"");
  msg += true_value;
  msg += "\" or \"";
  msg += false_value;
  msg += "\"";
  return std::move(msg);
}

TInvalidBoolAttr::TInvalidBoolAttr(const xercesc::DOMElement &location,
    const char *attr_name, const char *attr_value, const char *true_value,
    const char *false_value)
    : TInvalidAttr(
          MakeInvalidBoolAttrMsg(true_value, false_value).c_str(),
              location, attr_name, attr_value),
      TrueValue(true_value),
      FalseValue(false_value) {
}
