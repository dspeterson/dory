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

#include <base/no_default_case.h>
#include <xml/xml_input_line_info.h>
#include <xml/xml_string_util.h>

using namespace xercesc;

using namespace Base;
using namespace Xml;
using namespace Xml::Config;

std::pair<TFileLocation, std::string> TXmlError::BuildInfo(
    const XMLException &x) {
  return std::make_pair(TFileLocation(x.getSrcLine()),
      std::string(GetTranscoded(x.getMessage()).get()));
}

std::string TXmlError::BuildMsg(const std::optional<TFileLocation> &location,
    const char *msg) {
  std::string result;

  if (location) {
    result = "(line ";
    result += std::to_string(location->Line);

    if (location->Column) {
      result += ", column ";
      result += std::to_string(*location->Column);
    }

    result += "): ";
  }

  result += msg;
  return result;
}

std::pair<TFileLocation, std::string>
TSaxParseError::BuildInfo(const SAXParseException &x) {
  std::string msg("XML document parse error: ");
  msg += GetTranscoded(x.getMessage()).get();
  return std::make_pair(TFileLocation(x.getLineNumber(), x.getColumnNumber()),
      std::move(msg));
}

std::string TDomError::BuildMsg(const DOMException &x) {
  std::string result("XML DOM error: ");
  result += GetTranscoded(x.getMessage()).get();
  return result;
}

std::string TWrongEncoding::BuildMsg(const char *encoding,
    const char *expected_encoding) {
  std::string result("XML document has wrong encoding of [");
  result += encoding;
  result += "]: expected value is [";
  result += expected_encoding;
  result += "]";
  return result;
}

std::optional<TFileLocation>
TContentError::BuildLocation(const DOMNode &node) {
  std::optional<TFileLocation> result;
  const TXmlInputLineInfo *info = TXmlInputLineInfo::Get(node);

  if (info) {
    result.emplace(info->GetLineNum(), info->GetColumnNum());
  }

  return result;
}

TElementError::TElementError(const xercesc::DOMElement &elem, const char *msg)
    : TContentError(elem, msg),
      ElementName(GetTranscoded(elem.getNodeName()).get()) {
}

std::string TDuplicateElement::BuildMsg(const DOMElement &elem) {
  std::string result("XML document contains unexpected duplicate element [");
  result += GetTranscoded(elem.getNodeName()).get();
  result += "]";
  return result;
}

std::string TUnknownElement::BuildMsg(const DOMElement &elem) {
  std::string result("XML document contains unknown element [");
  result += GetTranscoded(elem.getNodeName()).get();
  result += "]";
  return result;
}

std::string TUnexpectedElementName::BuildMsg(const DOMElement &elem,
    const char *expected_elem_name) {
  std::string result("XML document contains unexpected element [");
  result += GetTranscoded(elem.getNodeName()).get();
  result += "]: expected element is [";
  result += expected_elem_name;
  result += "]";
  return result;
}

std::string TMissingChildElement::BuildMsg(const DOMElement &elem,
    const char *child_elem_name) {
  std::string result("XML  element [");
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is missing child element [";
  result += child_elem_name;
  result += "]";
  return result;
}

std::string TExpectedLeaf::BuildMsg(const DOMElement &elem) {
  std::string result("XML element [");
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] must not have any children";
  return result;
}

std::string TMissingAttrValue::BuildMsg(const DOMElement &elem,
    const char *attr_name) {
  std::string result("XML  element [");
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is missing attribute [";
  result += attr_name;
  result += "]";
  return result;
}

std::string TInvalidAttr::BuildMsg(const DOMElement &elem,
    const char *attr_name, const char *attr_value) {
  std::string result("Value [");
  result += attr_value;
  result += "] for attribute [";
  result += attr_name;
  result += "] of XML element [";
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is invalid";
  return result;
}

std::string TInvalidBoolAttr::BuildMsg(const DOMElement &elem,
    const char *attr_name, const char *attr_value, const char *true_value,
    const char *false_value) {
  std::string result("Value [");
  result += attr_value;
  result += "] for boolean attribute [";
  result += attr_name;
  result += "] of XML element [";
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is invalid: allowed values are [";
  result += true_value;
  result += "] and [";
  result += false_value;
  result += "]";
  return result;
}

std::string TAttrOutOfRange::BuildMsg(const DOMElement &elem,
    const char *attr_name, const char *attr_value) {
  std::string result("Value [");
  result += attr_value;
  result += "] for attribute [";
  result += attr_name;
  result += "] of XML element [";
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is out of range";
  return result;
}

static const char *ToString(TBase b) {
  switch (b) {
    case TBase::BIN: {
      return "binary";
    }
    case TBase::OCT: {
      return "octal";
    }
    case TBase ::DEC: {
      return "decimal";
    }
    case TBase::HEX: {
      break;
    }
    NO_DEFAULT_CASE;
  };

  return "hexadecimal";
}

std::string TWrongUnsignedIntegerBase::BuildMsg(const DOMElement &elem,
    const char *attr_name, const char *attr_value, Base::TBase found,
    unsigned int allowed) {
  std::string result("Value [");
  result += attr_value;
  result += "] for attribute [";
  result += attr_name;
  result += "] of XML element [";
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is in an unsupported ";
  result += ToString(found);
  result += " base.  Allowed bases are {";
  bool following = false;

  if (allowed & TBase::BIN) {
    result += "binary";
    following = true;
  }

  if (allowed & TBase::OCT) {
    if (following) {
      result += ", ";
    }

    result += "octal";
    following = true;
  }

  if (allowed & TBase::DEC) {
    if (following) {
      result += ", ";
    }

    result += "decimal";
    following = true;
  }

  if (allowed & TBase::HEX) {
    if (following) {
      result += ", ";
    }

    result += "hexadecimal";
  }

  result += "}";
  return result;
}

std::string TInvalidUnsignedIntegerAttr::BuildMsg(const DOMElement &elem,
    const char *attr_name, const char *attr_value) {
  std::string result("Value [");
  result += attr_value;
  result += "] for attribute [";
  result += attr_name;
  result += "] of XML element [";
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is not a vaild unsigned integer";
  return result;
}

std::string TInvalidSignedIntegerAttr::BuildMsg(const DOMElement &elem,
    const char *attr_name, const char *attr_value) {
  std::string result("Value [");
  result += attr_value;
  result += "] for attribute [";
  result += attr_name;
  result += "] of XML element [";
  result += GetTranscoded(elem.getNodeName()).get();
  result += "] is not a vaild signed integer";
  return result;
}
