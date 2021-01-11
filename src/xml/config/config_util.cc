/* <xml/config/config_util.cc>

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

   Implements <xml/config/config_util.h>.
 */

#include <xml/config/config_util.h>

#include <cctype>
#include <cstring>
#include <ios>
#include <limits>

#include <boost/algorithm/string/trim.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <xml/dom_document_util.h>
#include <xml/dom_parser_with_line_info.h>

using namespace xercesc;

using namespace Base;
using namespace Xml;
using namespace Xml::Config;

DOMDocument *Xml::Config::ParseXmlConfig(const void *buf, size_t buf_size,
    const char *expected_encoding) {
  assert(buf);
  assert(expected_encoding);

  try {
    MemBufInputSource input_source(
        reinterpret_cast<const XMLByte *>(buf), buf_size,
        /* Note: The contents of this blurb apparently don't matter, so I'm
           just making up some reasonable looking text.  It looks like it's
           meaningful when using a DTD (see
           https://en.wikipedia.org/wiki/XML_Catalog ). */
        "XML config file");
    TDomParserWithLineInfo parser;
    parser.parse(input_source);
    auto doc = MakeDomDocumentUniquePtr(parser.adoptDocument());
    const XMLCh *doc_encoding = doc->getXmlEncoding();

    if (!doc_encoding || TranscodeToString(doc_encoding).empty()) {
      throw TMissingEncoding();
    }

    auto trans_doc_encoding = GetTranscoded(doc_encoding);

    if (XMLString::compareIString(trans_doc_encoding.get(),
        expected_encoding)) {
      throw TWrongEncoding(trans_doc_encoding.get(), expected_encoding);
    }

    return doc.release();
  } catch (const XMLException &x) {
    throw TXmlError(x);
  } catch (const SAXParseException &x) {
    throw TSaxParseError(x);
  } catch (const DOMException &x) {
    throw TDomError(x);
  }
}

bool Xml::Config::IsAllWhitespace(const DOMText &node) {
  auto transcoded = GetTranscoded(node.getData());
  const char *data = transcoded.get();

  for (size_t i = 0; data[i]; ++i) {
    if (!std::isspace(data[i])) {
      return false;
    }
  }

  return true;
}

std::unordered_map<std::string, const DOMElement *>
Xml::Config::GetSubsectionElements(const DOMElement &parent,
    const std::vector<std::pair<std::string, bool>> &subsection_vec,
    bool allow_unknown_subsection) {
  std::unordered_map<std::string, const DOMElement *> result;
  std::unordered_map<std::string, bool> subsection_map;

  for (const auto &item : subsection_vec) {
    subsection_map.insert(item);
  }

  for (const DOMNode *child = parent.getFirstChild();
      child;
      child = child->getNextSibling()) {
    switch (child->getNodeType()) {
      case DOMNode::ELEMENT_NODE: {
        const auto *elem = dynamic_cast<const DOMElement *>(child);
        std::string name(TranscodeToString(elem->getTagName()));

        if (subsection_map.count(name)) {
          auto ret = result.insert(std::make_pair(name, elem));

          if (!ret.second) {
            throw TDuplicateElement(*elem);
          }
        } else if (!allow_unknown_subsection) {
          throw TUnknownElement(*elem);
        }

        break;
      }
      case DOMNode::TEXT_NODE:
      case DOMNode::CDATA_SECTION_NODE: {
        const auto *text = dynamic_cast<const DOMText *>(child);

        if (!IsAllWhitespace(*text)) {
          throw TUnexpectedText(*text);
        }

        break;
      }
      default: {
        break;  // ignore other node types
      }
    }
  }

  for (const auto &item : subsection_vec) {
    if (item.second && !result.count(item.first)) {
      throw TMissingChildElement(parent, item.first.c_str());
    }
  }

  return result;
}

const DOMElement * Xml::Config::TryGetChildElement(
    const xercesc::DOMElement &parent, const char *child_name) {
  assert(child_name);

  for (const DOMElement *child = parent.getFirstElementChild();
      child;
      child = child->getNextElementSibling()) {
    std::string name(TranscodeToString(child->getTagName()));

    if (name == child_name) {
      return child;
    }
  }

  return nullptr;
}

std::vector<const DOMElement *>
Xml::Config::GetItemListElements(const DOMElement &parent,
    const char *item_name) {
  std::vector<const DOMElement *> result;

  for (const DOMNode *child = parent.getFirstChild();
      child;
      child = child->getNextSibling()) {
    switch (child->getNodeType()) {
      case DOMNode::ELEMENT_NODE: {
        const auto *elem = dynamic_cast<const DOMElement *>(child);
        std::string name(TranscodeToString(elem->getTagName()));

        if (name != item_name) {
          throw TUnexpectedElementName(*elem, item_name);
        }

        result.push_back(elem);
        break;
      }
      case DOMNode::TEXT_NODE:
      case DOMNode::CDATA_SECTION_NODE: {
        const auto *text = dynamic_cast<const DOMText *>(child);

        if (!IsAllWhitespace(*text)) {
          throw TUnexpectedText(*text);
        }

        break;
      }
      default: {
        break;  // ignore other node types
      }
    }
  }

  return result;
}

void Xml::Config::RequireNoChildElement(const DOMElement &elem) {
  for (const DOMNode *child = elem.getFirstChild();
      child;
      child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::ELEMENT_NODE) {
      throw TUnknownElement(*dynamic_cast<const DOMElement *>(child));
    }
  }
}

void Xml::Config::RequireNoGrandchildElement(const DOMElement &elem) {
  for (const DOMNode *child = elem.getFirstChild();
      child;
      child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::ELEMENT_NODE) {
      RequireNoChildElement(*dynamic_cast<const DOMElement *>(child));
    }
  }
}

void Xml::Config::RequireLeaf(const DOMElement &elem) {
  if (elem.getFirstChild()) {
    throw TExpectedLeaf(elem);
  }
}

void Xml::Config::RequireAllChildElementLeaves(const DOMElement &elem) {
  for (const DOMNode *child = elem.getFirstChild();
      child;
      child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::ELEMENT_NODE) {
      RequireLeaf(*dynamic_cast<const DOMElement *>(child));
    }
  }
}

std::optional<std::string> TAttrReader::GetOptString(const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  assert(attr_name);
  assert(opts == (opts & TOpts::TRIM_WHITESPACE));
  std::optional<std::string> result;
  const DOMAttr *attr = elem.getAttributeNode(GetTranscoded(attr_name).get());

  if (attr) {
    std::string value(TranscodeToString(attr->getValue()));

    if (opts & TOpts::TRIM_WHITESPACE) {
      boost::algorithm::trim(value);
    }

    result.emplace(std::move(value));
  }

  return result;
}

std::string TAttrReader::GetString(const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  assert(attr_name);
  assert(opts == (opts & (TOpts::THROW_IF_EMPTY | TOpts::TRIM_WHITESPACE)));
  const DOMAttr *attr = elem.getAttributeNode(GetTranscoded(attr_name).get());

  if (!attr) {
    throw TMissingAttrValue(elem, attr_name);
  }

  std::string result(TranscodeToString(attr->getValue()));

  if (opts & TOpts::TRIM_WHITESPACE) {
    boost::algorithm::trim(result);
  }

  if ((opts & TOpts::THROW_IF_EMPTY) && result.empty()) {
    throw TMissingAttrValue(elem, attr_name);
  }

  return result;
}

static bool StringToBool(const std::string &s, const char *true_value,
    const char *false_value, bool case_sensitive, const DOMElement &elem,
    const char *attr_name) {
  int (*compare_fn)(const char *, const char *) =
      case_sensitive ? std::strcmp : strcasecmp;
  bool is_true = (compare_fn(s.c_str(), true_value) == 0);
  bool is_false = (compare_fn(s.c_str(), false_value) == 0);
  assert(!(is_true && is_false));

  if (!is_true && !is_false) {
    throw TInvalidBoolAttr(elem, attr_name, s.c_str(), true_value,
        false_value);
  }

  return is_true;
}

std::optional<bool> TAttrReader::GetOptNamedBool(const DOMElement &elem,
    const char *attr_name, const char *true_value, const char *false_value,
    unsigned int opts) {
  assert(attr_name);
  assert(true_value);
  assert(false_value);
  assert(opts == (opts & (TOpts::REQUIRE_PRESENCE | TOpts::CASE_SENSITIVE)));
  std::optional<bool> result;
  auto opt_s = GetOptString(elem, attr_name, 0 | TOpts::TRIM_WHITESPACE);

  if (!opt_s) {
    if (opts & TOpts::REQUIRE_PRESENCE) {
      throw TMissingAttrValue(elem, attr_name);
    }
  } else if (!(*opt_s).empty()) {
    result.emplace(StringToBool(*opt_s, true_value, false_value,
        (opts & TOpts::CASE_SENSITIVE) != 0, elem, attr_name));
  }

  return result;
}

bool TAttrReader::GetNamedBool(const xercesc::DOMElement &elem,
    const char *attr_name, const char *true_value, const char *false_value,
    unsigned int opts) {
  assert(attr_name);
  assert(true_value);
  assert(false_value);
  assert(opts == (opts & TOpts::CASE_SENSITIVE));
  std::string s = GetString(elem, attr_name,
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  return StringToBool(s, true_value, false_value,
      (opts & TOpts::CASE_SENSITIVE) != 0, elem, attr_name);
}

/* Note: On entry, leading and trailing whitespace has been trimmed from
   'value'. */
static uint32_t ExtractMultiplier(std::string &value, unsigned int opts) {
  assert(!value.empty());
  uint32_t mult = 1;
  using TOpts = TAttrReader::TOpts;

  /* In the case where 'value' is the letter 'k' or 'm' by itself, don't do
     anything.  Then our caller will handle the invalid input.  Here we depend
     on leading and trailing whitespace being trimmed from 'value'. */
  if (value.size() > 1) {
    switch (value.back()) {
      case 'k':
      case 'K': {
        if (opts & TOpts::ALLOW_K) {
          mult = 1024;
        }
        break;
      }
      case 'm':
      case 'M': {
        if (opts & TOpts::ALLOW_M) {
          mult = 1024 * 1024;
        }
        break;
      }
      default: {
        break;
      }
    }

    if (mult != 1) {
      /* Eliminate trailing 'k' or 'm', and any resulting trailing whitespace.
       */
      value.pop_back();
      boost::algorithm::trim_right(value);
    }
  }

  return mult;
}

static intmax_t AttrToIntMax(const std::string &attr, const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  std::string attr_copy(attr);
  intmax_t mult = ExtractMultiplier(attr_copy, opts);
  intmax_t value = 0;

  try {
    value = ToSigned<intmax_t>(attr_copy);
  } catch (const TInvalidInteger &) {
    throw TInvalidSignedIntegerAttr(elem, attr_name, attr.c_str());
  } catch (const std::range_error &) {
    throw TAttrOutOfRange(elem, attr_name, attr.c_str());
  }

  if ((value < (std::numeric_limits<intmax_t>::min() / mult)) ||
      (value > (std::numeric_limits<intmax_t>::max() / mult))) {
    throw TAttrOutOfRange(elem, attr_name, attr.c_str());
  }

  return value * mult;
}

static uintmax_t AttrToUintMax(const std::string &attr, const DOMElement &elem,
    const char *attr_name, unsigned int allowed_bases, unsigned int opts) {
  std::string attr_copy(attr);
  uintmax_t mult = ExtractMultiplier(attr_copy, opts);
  uintmax_t value = 0;

  try {
    value = ToUnsigned<uintmax_t>(attr_copy, allowed_bases);
  } catch (const TInvalidInteger &) {
    throw TInvalidUnsignedIntegerAttr(elem, attr_name, attr.c_str());
  } catch (const TWrongBase &x) {
    throw TWrongUnsignedIntegerBase(elem, attr_name, attr.c_str(),
        x.GetFound(), x.GetAllowed());
  } catch (const std::range_error &) {
    throw TAttrOutOfRange(elem, attr_name, attr.c_str());
  }

  if (value > (std::numeric_limits<uintmax_t>::max() / mult)) {
    throw TAttrOutOfRange(elem, attr_name, attr.c_str());
  }

  return value * mult;
}

static intmax_t GetIntMaxAttr(const DOMElement &elem, const char *attr_name,
    unsigned int opts) {
  assert(attr_name);
  using TOpts = TAttrReader::TOpts;
  std::string s = TAttrReader::GetString(elem, attr_name,
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  return AttrToIntMax(s, elem, attr_name, opts);
}

static uintmax_t GetUintMaxAttr(const DOMElement &elem, const char *attr_name,
    unsigned int allowed_bases, unsigned int opts) {
  assert(attr_name);
  using TOpts = TAttrReader::TOpts;
  std::string s = TAttrReader::GetString(elem, attr_name,
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  return AttrToUintMax(s, elem, attr_name, allowed_bases, opts);
}

template <>
intmax_t TAttrReader::GetIntHelper<intmax_t>(const DOMElement &elem,
    const char *attr_name, unsigned int /* allowed_bases */,
    unsigned int opts) {
  return GetIntMaxAttr(elem, attr_name, opts);
}

template <>
uintmax_t TAttrReader::GetIntHelper<uintmax_t>(const DOMElement &elem,
    const char *attr_name, unsigned int allowed_bases, unsigned int opts) {
  return GetUintMaxAttr(elem, attr_name, allowed_bases, opts);
}

static std::optional<std::string> GetOptInAttrHelper(const DOMElement &elem,
    const char *attr_name, const char *empty_value_name, unsigned int opts) {
  assert(attr_name);
  using TOpts = TAttrReader::TOpts;
  auto opt_s = TAttrReader::GetOptString(elem, attr_name,
      0 | TOpts::TRIM_WHITESPACE);

  if (!opt_s) {
    if (opts & TOpts::REQUIRE_PRESENCE) {
      throw TMissingAttrValue(elem, attr_name);
    }
  } else {
    if (opt_s->empty()) {
      if (empty_value_name && (opts & TOpts::STRICT_EMPTY_VALUE)) {
        throw TMissingAttrValue(elem, attr_name);
      }
    } else if (!empty_value_name || (*opt_s != empty_value_name)) {
      return opt_s;
    }

    opt_s.reset();
  }

  return opt_s;
}

static std::optional<intmax_t> GetOptIntMaxAttr(const DOMElement &elem,
    const char *attr_name, const char *empty_value_name, unsigned int opts) {
  std::optional<intmax_t> result;
  auto opt_attr_str = GetOptInAttrHelper(elem, attr_name, empty_value_name,
      opts);

  if (opt_attr_str) {
    result.emplace(AttrToIntMax(*opt_attr_str, elem, attr_name, opts));
  }

  return result;
}

static std::optional<uintmax_t> GetOptUintMaxAttr(const DOMElement &elem,
    const char *attr_name, const char *empty_value_name,
    unsigned int allowed_bases, unsigned int opts) {
  std::optional<uintmax_t> result;
  auto opt_attr_str = GetOptInAttrHelper(elem, attr_name, empty_value_name,
      opts);

  if (opt_attr_str) {
    result.emplace(AttrToUintMax(*opt_attr_str, elem, attr_name, allowed_bases,
        opts));
  }

  return result;
}

template <>
std::optional<intmax_t> TAttrReader::GetOptIntHelper<intmax_t>(
    const DOMElement &elem, const char *attr_name,
    const char *empty_value_name, unsigned int /* allowed_bases */,
    unsigned int opts) {
  return GetOptIntMaxAttr(elem, attr_name, empty_value_name, opts);
}

template <>
std::optional<uintmax_t> TAttrReader::GetOptIntHelper<uintmax_t>(
    const DOMElement &elem, const char *attr_name,
    const char *empty_value_name, unsigned int allowed_bases,
    unsigned int opts) {
  return GetOptUintMaxAttr(elem, attr_name, empty_value_name, allowed_bases,
      opts);
}
