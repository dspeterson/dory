/* <dory/util/handle_xml_errors.cc>

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

   Implements <dory/util/handle_xml_errors.h>.
 */

#include <dory/util/handle_xml_errors.h>

#include <sstream>
#include <utility>

#include <base/no_default_case.h>
#include <xml/config/config_errors.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace Xml;
using namespace Xml::Config;

static void AddPreamble(std::ostringstream &os) {
  os << "Config file error: ";
}

static void AddPreamble(std::ostringstream &os, const TErrorLineOnlyInfo &x) {
  os << "Config file error (line " << x.GetLine() << "): ";
}

static void AddPreamble(std::ostringstream &os,
    const TErrorLineAndColumnInfo &x) {
  os << "Config file error (line " << x.GetLine() << " column "
      << x.GetColumn() << "): ";
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

static std::string BuildAllowedBasesString(unsigned int allowed) {
  std::ostringstream os;
  os << "{ ";
  bool following = false;

  if (allowed & TBase::BIN) {
    os << "binary";
    following = true;
  }

  if (allowed & TBase::OCT) {
    if (following) {
      os << ", ";
    }

    os << "octal";
    following = true;
  }

  if (allowed & TBase::DEC) {
    if (following) {
      os << ", ";
    }

    os << "decimal";
    following = true;
  }

  if (allowed & TBase::HEX) {
    if (following) {
      os << ", ";
    }

    os << "hexadecimal";
  }

  os << " }";
  return os.str();
}

TOpt<std::string> Dory::Util::HandleXmlErrors(
    const std::function<void()> &fn) {
  TOpt<std::string> opt_msg;
  std::ostringstream os;

  try {
    fn();
  } catch (const TAttrOutOfRange &x) {
    AddPreamble(os, x);
    os << "Value for integer attribute [" << x.GetAttrName()
        << "] of element <" << x.GetElementName() << "> is out of range.";
  } catch (const TInvalidUnsignedIntegerAttr &x) {
    AddPreamble(os, x);
    os << "Value for unsigned integer attribute [" << x.GetAttrName()
       << "] of element <" << x.GetElementName() << "> is invalid.";
  } catch (const TInvalidSignedIntegerAttr &x) {
    AddPreamble(os, x);
    os << "Value for signed integer attribute [" << x.GetAttrName()
        << "] of element <" << x.GetElementName() << "> is invalid.";
  } catch (const TWrongUnsignedIntegerBase &x) {
    AddPreamble(os, x);
    os << "Value for unsigned integer attribute [" << x.GetAttrName()
       << "] of element <" << x.GetElementName() << "> is in unsupported base "
       << ToString(x.GetFoundBase()) << ".  Allowed bases are "
       << BuildAllowedBasesString(x.GetAllowedBases()) << ".";
  } catch (const TInvalidBoolAttr &x) {
    AddPreamble(os, x);
    os << "Value for boolean attribute [" << x.GetAttrName()
        << "] of element <" << x.GetElementName()
        << "> is invalid.  Allowed values are [" << x.GetTrueValue()
        << "] and [" << x.GetFalseValue() << "].";
  } catch (const TInvalidAttr &x) {
    AddPreamble(os, x);
    os << "Value for attribute [" << x.GetAttrName() << "] of element <"
        << x.GetElementName() << "> is invalid: " << x.what();
  } catch (const TMissingAttrValue &x) {
    AddPreamble(os, x);
    os << "Value for attribute [" << x.GetAttrName() << "] of element <"
        << x.GetElementName() << "> is missing.";
  } catch (const TAttrError &x) {
    AddPreamble(os, x);
    os << "Error in attribute [" << x.GetAttrName() << "] of element <"
        << x.GetElementName() << ">: " << x.what();
  } catch (const TExpectedLeaf &x) {
    AddPreamble(os, x);
    os << "Element <" << x.GetElementName() << "> must not have any children.";
  } catch (const TMissingChildElement &x) {
    AddPreamble(os, x);
    os << "Element <" << x.GetElementName() << "> is missing child element <"
        << x.GetChildElementName() << ">.";
  } catch (const TUnexpectedElementName &x) {
    AddPreamble(os, x);
    os << "Element <" << x.GetElementName()
        << "> is unexpected, and should be <" << x.GetExpectedElementName()
        << ">.";
  } catch (const TUnknownElement &x) {
    AddPreamble(os, x);
    os << "Element <" << x.GetElementName() << "> is unknown.";
  } catch (const TDuplicateElement &x) {
    AddPreamble(os, x);
    os << "Duplicate element <" << x.GetElementName() << "> is not allowed.";
  } catch (const TElementError &x) {
    AddPreamble(os, x);
    os << "Error in element <" << x.GetElementName() << ">: " << x.what();
  } catch (const TUnexpectedText &x) {
    AddPreamble(os, x);
    os << "Document contains unexpected text.";
  } catch (const TContentError &x) {
    AddPreamble(os, x);
    os << "Document content error: " << x.what();
  } catch (const TWrongEncoding &x) {
    AddPreamble(os);
    os << "Document has wrong encoding of [" << x.GetEncoding()
        << "]: expected value is [" << x.GetExpectedEncoding() << "].";
  } catch (const TMissingEncoding &x) {
    AddPreamble(os);
    os << "Document is missing encoding.";
  } catch (const TDomException &x) {
    AddPreamble(os);
    os << "XML DOM error: " << x.what();
  } catch (const TSaxParseException &x) {
    AddPreamble(os, x);
    os << "XML parse error: " << x.what();
  } catch (const TXmlException &x) {
    AddPreamble(os, x);
    os << "XML exception: " << x.what();
  } catch (const TXmlConfigError &x) {
    AddPreamble(os);
    os << "XML error: " << x.what();
  }

  std::string msg(os.str());

  if (!msg.empty()) {
    opt_msg.MakeKnown(std::move(msg));
  }

  return opt_msg;
}
