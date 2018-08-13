/* <xml/config/config_util.test.cc>
 
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
 
   Unit test for <xml/config/config_util.h>.
 */

#include <xml/config/config_util.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/sax/SAXParseException.hpp>

#include <base/opt.h>
#include <xml/dom_document_util.h>
#include <xml/config/config_errors.h>
#include <xml/config/config_util.h>
#include <xml/test/xml_test_initializer.h>
#include <xml/xml_input_line_info.h>

#include <gtest/gtest.h>

using namespace xercesc;

using namespace Base;
using namespace Xml;
using namespace Xml::Config;
using namespace Xml::Test;

namespace {

  /* The fixture for testing XML config file parsing utilities. */
  class TXmlConfigUtilTest : public ::testing::Test {
    protected:
    TXmlConfigUtilTest() = default;

    ~TXmlConfigUtilTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }

    TXmlTestInitializer Initializer;  // initializes Xerces XML library
  };  // TXmlConfigUtilTest

  TEST_F(TXmlConfigUtilTest, EncodingTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <testElement1>   </testElement1>" << std::endl
        << "  <testElement2>   blah    </testElement2>" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    bool caught = false;

    try {
      auto wrong_encoding_doc = MakeDomDocumentUniquePtr(
          ParseXmlConfig(xml.data(), xml.size(), "UTF-8"));
    } catch (const TWrongEncoding &x) {
      caught = true;
      ASSERT_EQ(x.GetEncoding(), "US-ASCII");
    }

    ASSERT_TRUE(caught);

    os.str("");
    os << "<testDocument>" << std::endl
        << "  <testElement1>   </testElement1>" << std::endl
        << "  <testElement2>   blah    </testElement2>" << std::endl
        << "</testDocument>" << std::endl;
    xml = os.str();
    caught = false;

    try {
      auto no_encoding_doc = MakeDomDocumentUniquePtr(
          ParseXmlConfig(xml.data(), xml.size(), "UTF-8"));
    } catch (const TMissingEncoding &) {
      caught = true;
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, ParseErrorTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <noClosingTag>" << std::endl  // bad XML: no closing tag
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeEmptyDomDocumentUniquePtr();
    bool caught = false;

    try {
      doc.reset(ParseXmlConfig(xml.data(), xml.size(), "US-ASCII"));
    } catch (const TSaxParseException &x) {
      caught = true;
      ASSERT_EQ(x.GetLine(), 4U);
      ASSERT_EQ(x.GetColumn(), 3U);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, SuccessfulParseTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <testElement1>   </testElement1>" << std::endl
        << "  <testElement2>   blah    </testElement2>" << std::endl
        << "  <testElement3><testElement3a /></testElement3>" << std::endl
        << "  <testElement4><testElement4a><testElement4aa />"
            "</testElement4a></testElement4>"
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");
    const TXmlInputLineInfo *line_info = TXmlInputLineInfo::Get(*root);
    ASSERT_TRUE(line_info != nullptr);
    ASSERT_EQ(line_info->GetLineNum(), 2U);
    ASSERT_EQ(line_info->GetColumnNum(), 15U);

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "testElement1");
    DOMNode *grandchild = child->getFirstChild();
    ASSERT_TRUE(grandchild != nullptr);
    ASSERT_EQ(grandchild->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(grandchild);
    std::string text(TranscodeToString(text_node->getNodeValue()));
    ASSERT_EQ(text, "   ");
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "testElement2");
    grandchild = child->getFirstChild();
    ASSERT_TRUE(grandchild != nullptr);
    ASSERT_EQ(grandchild->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(grandchild);
    text = TranscodeToString(text_node->getNodeValue());
    ASSERT_EQ(text, "   blah    ");
    ASSERT_FALSE(IsAllWhitespace(*text_node));
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    try {
      /* has child, but child is a text node, not an element */
      RequireNoChildElement(*elem);
    } catch (const TUnknownElement &) {
      ASSERT_TRUE(false);
    }

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "testElement3");
    elem = static_cast<const DOMElement *>(child);
    bool caught = false;

    try {
      RequireLeaf(*elem);
    } catch (const TExpectedLeaf &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "testElement3");
      ASSERT_EQ(x.GetLine(), 5U);
      ASSERT_EQ(x.GetColumn(), 17U);
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      RequireNoChildElement(*elem);
    } catch (const TUnknownElement &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "testElement3a");
      ASSERT_EQ(x.GetLine(), 5U);
      ASSERT_EQ(x.GetColumn(), 34U);
    }

    ASSERT_TRUE(caught);

    try {
      RequireNoGrandchildElement(*elem);
    } catch (const TUnknownElement &) {
      ASSERT_TRUE(false);
    }

    grandchild = child->getFirstChild();
    ASSERT_TRUE(grandchild != nullptr);
    ASSERT_EQ(grandchild->getNodeType(), DOMNode::ELEMENT_NODE);
    const DOMElement *gc_elem = static_cast<const DOMElement *>(grandchild);
    ASSERT_EQ(TranscodeToString(gc_elem->getNodeName()), "testElement3a");

    try {
      RequireNoChildElement(*gc_elem);
    } catch (const TUnknownElement &) {
      ASSERT_TRUE(false);
    }

    try {
      RequireLeaf(*gc_elem);
    } catch (const TExpectedLeaf &) {
      ASSERT_TRUE(false);
    }

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "testElement4");
    elem = static_cast<const DOMElement *>(child);
    caught = false;

    try {
      RequireNoGrandchildElement(*elem);
    } catch (const TUnknownElement &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "testElement4aa");
      ASSERT_EQ(x.GetLine(), 6U);
      ASSERT_EQ(x.GetColumn(), 50U);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, RequireAllChildElementLeavesTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <elem1><elem1a /><elem1b /></elem1>" << std::endl
        << "  <elem2><elem2a /><elem2b>blah</elem2b></elem2>" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "elem1");

    const DOMNode *grandchild = child->getFirstChild();
    ASSERT_TRUE(grandchild != nullptr);
    ASSERT_EQ(grandchild->getNodeType(), DOMNode::ELEMENT_NODE);
    const DOMElement *gc_elem = static_cast<const DOMElement *>(grandchild);
    ASSERT_EQ(TranscodeToString(gc_elem->getNodeName()), "elem1a");
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    try {
      RequireAllChildElementLeaves(*elem);
    } catch (const TExpectedLeaf &) {
      ASSERT_TRUE(false);
    }

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "elem2");

    grandchild = child->getFirstChild();
    ASSERT_TRUE(grandchild != nullptr);
    ASSERT_EQ(grandchild->getNodeType(), DOMNode::ELEMENT_NODE);
    gc_elem = static_cast<const DOMElement *>(grandchild);
    ASSERT_EQ(TranscodeToString(gc_elem->getNodeName()), "elem2a");
    elem = static_cast<const DOMElement *>(child);
    bool caught = false;

    try {
      RequireAllChildElementLeaves(*elem);
    } catch (const TExpectedLeaf &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "elem2b");
      ASSERT_EQ(x.GetLine(), 4U);
      ASSERT_EQ(x.GetColumn(), 28U);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, SubsectionTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <section1>" << std::endl
        << "    <sub1 />" << std::endl
        << "    <sub2 />" << std::endl
        << "    <sub3 />" << std::endl
        << "  </section1>" << std::endl
        << "  <section2>" << std::endl
        << "    <sub1 />" << std::endl
        << "    <sub2 />" << std::endl
        << "    <sub2 />" << std::endl
        << "  </section2>" << std::endl
        << "  <section3>blah<sub1 />" << std::endl
        << "    <sub2 />" << std::endl
        << "  </section3>" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section1");
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    auto result = GetSubsectionElements(*elem,
        {{"sub1", true}, {"sub2", true}, {"sub3", true}}, false);
    ASSERT_EQ(result.size(), 3U);
    const DOMElement *elem2 = result["sub1"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub1");
    elem2 = result["sub2"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub2");
    elem2 = result["sub3"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub3");
    bool caught = false;

    try {
      result = GetSubsectionElements(*elem,
          {{"sub1", true}, {"sub2", true}}, false);
    } catch (const TUnknownElement &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "sub3");
    }

    ASSERT_TRUE(caught);

    try {
      result = GetSubsectionElements(*elem,
          {{"sub1", true}, {"sub2", true}}, true);
    } catch (const TUnknownElement &) {
      ASSERT_TRUE(false);
    }

    ASSERT_EQ(result.size(), 2U);
    elem2 = result["sub1"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub1");
    elem2 = result["sub2"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub2");
    caught = false;

    try {
      result = GetSubsectionElements(*elem,
          {{"sub1", true}, {"sub2", true}, {"sub3", true}, {"sub4", true}},
          false);
    } catch (const TMissingChildElement &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "section1");
      ASSERT_EQ(x.GetChildElementName(), "sub4");
    }

    ASSERT_TRUE(caught);

    try {
      result = GetSubsectionElements(*elem,
          {{"sub1", true}, {"sub2", true}, {"sub3", true}, {"sub4", false}},
          false);
    } catch (const TMissingChildElement &) {
      ASSERT_TRUE(false);
    }

    ASSERT_EQ(result.size(), 3U);
    elem2 = result["sub1"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub1");
    elem2 = result["sub2"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub2");
    elem2 = result["sub3"];
    ASSERT_EQ(TranscodeToString(elem2->getNodeName()), "sub3");

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section2");
    elem = static_cast<const DOMElement *>(child);
    caught = false;

    try {
      result = GetSubsectionElements(*elem,
          {{"sub1", true}, {"sub2", true}}, false);
    } catch (const TDuplicateElement &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "sub2");
      ASSERT_EQ(x.GetLine(), 11U);
    }

    ASSERT_TRUE(caught);

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section3");
    elem = static_cast<const DOMElement *>(child);
    caught = false;

    try {
      result = GetSubsectionElements(*elem,
          {{"sub1", true}, {"sub2", true}}, false);
    } catch (const TUnexpectedText &x) {
      caught = true;
      ASSERT_EQ(x.GetLine(), 13U);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, ItemListTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <section1>" << std::endl
        << "  </section1>" << std::endl
        << "  <section2>" << std::endl
        << "    <item />" << std::endl
        << "    <item />" << std::endl
        << "    <item />" << std::endl
        << "  </section2>" << std::endl
        << "  <section3>" << std::endl
        << "    <item />" << std::endl
        << "    <crap />" << std::endl
        << "    <item />" << std::endl
        << "  </section3>" << std::endl
        << "  <section4>blah<item />" << std::endl
        << "    <item />" << std::endl
        << "  </section4>" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section1");
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    std::vector<const DOMElement *> item_list =
        GetItemListElements(*elem, "item");
    ASSERT_TRUE(item_list.empty());

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section2");
    elem = static_cast<const DOMElement *>(child);

    item_list = GetItemListElements(*elem, "item");
    ASSERT_EQ(item_list.size(), 3U);

    const DOMElement *item = item_list[0];
    ASSERT_EQ(TranscodeToString(item->getNodeName()), "item");
    const TXmlInputLineInfo *line_info = TXmlInputLineInfo::Get(*item);
    ASSERT_TRUE(line_info != nullptr);
    ASSERT_EQ(line_info->GetLineNum(), 6U);
    ASSERT_EQ(line_info->GetColumnNum(), 13U);

    item = item_list[1];
    ASSERT_EQ(TranscodeToString(item->getNodeName()), "item");
    line_info = TXmlInputLineInfo::Get(*item);
    ASSERT_TRUE(line_info != nullptr);
    ASSERT_EQ(line_info->GetLineNum(), 7U);
    ASSERT_EQ(line_info->GetColumnNum(), 13U);

    item = item_list[2];
    ASSERT_EQ(TranscodeToString(item->getNodeName()), "item");
    line_info = TXmlInputLineInfo::Get(*item);
    ASSERT_TRUE(line_info != nullptr);
    ASSERT_EQ(line_info->GetLineNum(), 8U);
    ASSERT_EQ(line_info->GetColumnNum(), 13U);

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section3");
    elem = static_cast<const DOMElement *>(child);
    bool caught = false;

    try {
      item_list = GetItemListElements(*elem, "item");
    } catch (const TUnexpectedElementName &x) {
      caught = true;
      ASSERT_EQ(x.GetElementName(), "crap");
      ASSERT_EQ(x.GetExpectedElementName(), "item");
      ASSERT_EQ(x.GetLine(), 12U);
      ASSERT_EQ(x.GetColumn(), 13U);
    }

    ASSERT_TRUE(caught);
    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "section4");
    elem = static_cast<const DOMElement *>(child);
    caught = false;

    try {
      item_list = GetItemListElements(*elem, "item");
    } catch (const TUnexpectedText &x) {
      caught = true;
      ASSERT_EQ(x.GetLine(), 15U);
      ASSERT_EQ(x.GetColumn(), 17U);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, StringAttrTest) {
    using TOpts = TAttrReader::TOpts;
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <elem attr1=\"\"" << std::endl
        << "      attr2=\"   \"" << std::endl
        << "      attr3=\"   blah \" />" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "elem");
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    ASSERT_TRUE(TAttrReader::GetOptString(*elem, "wrong_attr").IsUnknown());

    TOpt<std::string> opt_str(TAttrReader::GetOptString(*elem, "attr1"));
    ASSERT_TRUE(opt_str.IsKnown());
    ASSERT_TRUE(opt_str->empty());

    opt_str = TAttrReader::GetOptString(*elem, "attr2");
    ASSERT_TRUE(opt_str.IsKnown());
    ASSERT_EQ(*opt_str, "   ");

    opt_str = TAttrReader::GetOptString(*elem, "attr2",
        TOpts::TRIM_WHITESPACE);
    ASSERT_TRUE(opt_str.IsKnown());
    ASSERT_EQ(*opt_str, "");

    opt_str = TAttrReader::GetOptString(*elem, "attr3");
    ASSERT_TRUE(opt_str.IsKnown());
    ASSERT_EQ(*opt_str, "   blah ");

    opt_str = TAttrReader::GetOptString(*elem, "attr3",
        TOpts::TRIM_WHITESPACE);
    ASSERT_TRUE(opt_str.IsKnown());
    ASSERT_EQ(*opt_str, "blah");

    ASSERT_EQ(TAttrReader::GetString(*elem, "attr3"), "   blah ");
    ASSERT_EQ(TAttrReader::GetString(*elem, "attr3", TOpts::TRIM_WHITESPACE),
        "blah");

    bool caught = false;

    try {
      TAttrReader::GetString(*elem, "wrong_attr");
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "wrong_attr");
      ASSERT_EQ(x.GetElementName(), "elem");
      ASSERT_EQ(x.GetLine(), 5U);
      ASSERT_EQ(x.GetColumn(), 26U);
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetString(*elem, "attr1", TOpts::THROW_IF_EMPTY);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "attr1");
      ASSERT_EQ(x.GetElementName(), "elem");
      ASSERT_EQ(x.GetLine(), 5U);
      ASSERT_EQ(x.GetColumn(), 26U);
    }

    ASSERT_TRUE(caught);
    std::string str;

    try {
      str = TAttrReader::GetString(*elem, "attr2", TOpts::THROW_IF_EMPTY);
    } catch (const TMissingAttrValue &) {
      ASSERT_TRUE(false);
    }

    ASSERT_EQ(str, "   ");
    caught = false;

    try {
      TAttrReader::GetString(*elem, "attr2",
          TOpts::THROW_IF_EMPTY | TOpts::TRIM_WHITESPACE);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "attr2");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, BoolAttrTest) {
    using TOpts = TAttrReader::TOpts;
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <elem attr1=\"    \"" << std::endl
        << "      attr2=\"  true   \"" << std::endl
        << "      attr3=\"false\"" << std::endl
        << "      attr4=\"true false\"" << std::endl
        << "      attr5=\"  tRuE   \"" << std::endl
        << "      attr6=\"FALSE\"" << std::endl
        << "      attr7=\"yes\"" << std::endl
        << "      attr8=\"  no   \" />" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "elem");
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    TOpt<bool> opt_bool = TAttrReader::GetOptBool(*elem, "attr1");
    ASSERT_TRUE(opt_bool.IsUnknown());
    opt_bool = TAttrReader::GetOptBool(*elem, "wrong_attr");
    ASSERT_TRUE(opt_bool.IsUnknown());
    opt_bool = TAttrReader::GetOptBool(*elem, "attr2");
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_TRUE(*opt_bool);
    opt_bool = TAttrReader::GetOptBool(*elem, "attr3");
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_FALSE(*opt_bool);
    opt_bool = TAttrReader::GetOptBool(*elem, "attr5");
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_TRUE(*opt_bool);
    opt_bool = TAttrReader::GetOptBool(*elem, "attr6");
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_FALSE(*opt_bool);
    opt_bool = TAttrReader::GetOptBool(*elem, "attr1",
        TOpts::REQUIRE_PRESENCE);
    ASSERT_TRUE(opt_bool.IsUnknown());
    bool caught = false;

    try {
      TAttrReader::GetOptBool(*elem, "wrong_attr", TOpts::REQUIRE_PRESENCE);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "wrong_attr");
      ASSERT_EQ(x.GetElementName(), "elem");
      ASSERT_EQ(x.GetLine(), 10U);
      ASSERT_EQ(x.GetColumn(), 25U);
    }

    ASSERT_TRUE(caught);
    opt_bool = TAttrReader::GetOptBool(*elem, "attr3",
        TOpts::REQUIRE_PRESENCE);
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_FALSE(*opt_bool);
    caught = false;

    try {
      opt_bool = TAttrReader::GetOptBool(*elem, "attr5",
          TOpts::CASE_SENSITIVE);
    } catch (const TInvalidBoolAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetTrueValue(), "true");
      ASSERT_EQ(x.GetFalseValue(), "false");
      ASSERT_EQ(x.GetAttrValue(), "tRuE");
      ASSERT_EQ(x.GetAttrName(), "attr5");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      opt_bool = TAttrReader::GetOptBool(*elem, "attr4");
    } catch (const TInvalidBoolAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetTrueValue(), "true");
      ASSERT_EQ(x.GetFalseValue(), "false");
      ASSERT_EQ(x.GetAttrValue(), "true false");
      ASSERT_EQ(x.GetAttrName(), "attr4");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_TRUE(TAttrReader::GetBool(*elem, "attr2"));
    ASSERT_FALSE(TAttrReader::GetBool(*elem, "attr3"));
    ASSERT_TRUE(TAttrReader::GetBool(*elem, "attr5"));
    ASSERT_FALSE(TAttrReader::GetBool(*elem, "attr6"));
    caught = false;

    try {
      TAttrReader::GetBool(*elem, "attr6", TOpts::CASE_SENSITIVE);
    } catch (const TInvalidBoolAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetTrueValue(), "true");
      ASSERT_EQ(x.GetFalseValue(), "false");
      ASSERT_EQ(x.GetAttrValue(), "FALSE");
      ASSERT_EQ(x.GetAttrName(), "attr6");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    opt_bool = TAttrReader::GetOptNamedBool(*elem, "attr7", "yes", "no");
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_TRUE(*opt_bool);
    opt_bool = TAttrReader::GetOptNamedBool(*elem, "attr8", "yes", "no");
    ASSERT_TRUE(opt_bool.IsKnown());
    ASSERT_FALSE(*opt_bool);
    caught = false;

    try {
      opt_bool = TAttrReader::GetOptNamedBool(*elem, "attr2", "yes", "no");
    } catch (const TInvalidBoolAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetTrueValue(), "yes");
      ASSERT_EQ(x.GetFalseValue(), "no");
      ASSERT_EQ(x.GetAttrValue(), "true");
      ASSERT_EQ(x.GetAttrName(), "attr2");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_TRUE(
        TAttrReader::GetNamedBool(*elem, "attr7", "yes", "no"));
    ASSERT_FALSE(
        TAttrReader::GetNamedBool(*elem, "attr8", "yes", "no"));
    caught = false;

    try {
      TAttrReader::GetNamedBool(*elem, "attr2", "yes", "no");
    } catch (const TInvalidBoolAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetTrueValue(), "yes");
      ASSERT_EQ(x.GetFalseValue(), "no");
      ASSERT_EQ(x.GetAttrValue(), "true");
      ASSERT_EQ(x.GetAttrName(), "attr2");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, OptIntAttrTest) {
    using TOpts = TAttrReader::TOpts;
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <elem attr1=\"    \"" << std::endl
        << "      attr2=\"  5    \"" << std::endl
        << "      attr3=\"  20 k   \"" << std::endl
        << "      attr4=\"  -5m   \"" << std::endl
        << "      attr5=\"  -2 \"" << std::endl
        << "      attr6=\"    unlimited  \" />" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "elem");
    const DOMElement *elem = static_cast<const DOMElement *>(child);

    TOpt<int> opt_int = TAttrReader::GetOptInt<int>(*elem, "attr1");
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt<int>(*elem, "wrong_attr");
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr2");
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, 5);

    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr1",
        TOpts::REQUIRE_PRESENCE);
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr5",
        TOpts::REQUIRE_PRESENCE);
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, -2);
    bool caught = false;

    try {
      TAttrReader::GetOptInt<int>(*elem, "wrong_attr",
          TOpts::REQUIRE_PRESENCE);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "wrong_attr");
      ASSERT_EQ(x.GetElementName(), "elem");
      ASSERT_EQ(x.GetLine(), 8U);
      ASSERT_EQ(x.GetColumn(), 33U);
    }

    ASSERT_TRUE(caught);
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr1", "unlimited");
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "wrong_attr", "unlimited");
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr2", "unlimited");
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, 5);

    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr1", "unlimited",
        TOpts::REQUIRE_PRESENCE);
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr5", "unlimited",
        TOpts::REQUIRE_PRESENCE);
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, -2);
    caught = false;

    try {
      TAttrReader::GetOptInt2<int>(*elem, "wrong_attr", "unlimited",
          TOpts::REQUIRE_PRESENCE);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "wrong_attr");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr6", "unlimited");
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr6", "unlimited",
        TOpts::STRICT_EMPTY_VALUE);
    ASSERT_TRUE(opt_int.IsUnknown());
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr6", "unlimited",
        TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
    ASSERT_TRUE(opt_int.IsUnknown());

    opt_int = TAttrReader::GetOptInt2<int>(*elem, "wrong_attr", "unlimited",
        TOpts::STRICT_EMPTY_VALUE);
    ASSERT_TRUE(opt_int.IsUnknown());
    caught = false;

    try {
      TAttrReader::GetOptInt2<int>(*elem, "wrong_attr", "unlimited",
          TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "wrong_attr");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetOptInt2<int>(*elem, "attr1", "unlimited",
          TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "attr1");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    opt_int = TAttrReader::GetOptInt2<int>(*elem, "attr6", "unlimited");
    ASSERT_TRUE(opt_int.IsUnknown());

    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr3", TOpts::ALLOW_K);
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, 20 * 1024);
    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr4", TOpts::ALLOW_M);
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, -5 * 1024 * 1024);
    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr3",
        TOpts::ALLOW_K | TOpts::ALLOW_M);
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, 20 * 1024);
    opt_int = TAttrReader::GetOptInt<int>(*elem, "attr4",
        TOpts::ALLOW_K | TOpts::ALLOW_M);
    ASSERT_TRUE(opt_int.IsKnown());
    ASSERT_EQ(*opt_int, -5 * 1024 * 1024);
    caught = false;

    try {
      TAttrReader::GetOptInt<int>(*elem, "attr3");
    } catch (const TInvalidSignedIntegerAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "20 k");
      ASSERT_EQ(x.GetAttrName(), "attr3");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetOptInt<unsigned int>(*elem, "attr3", TOpts::ALLOW_M);
    } catch (const TInvalidUnsignedIntegerAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "20 k");
      ASSERT_EQ(x.GetAttrName(), "attr3");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlConfigUtilTest, IntAttrTest) {
    using TOpts = TAttrReader::TOpts;
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <elem attr1=\"    \"" << std::endl
        << "      attr2=\"  5    \"" << std::endl
        << "      attr3=\"60\"" << std::endl
        << "      attr4=\"20k\"" << std::endl
        << "      attr5=\" 16K  \"" << std::endl
        << "      attr6=\"   8  k   \"" << std::endl
        << "      attr7=\"2m\"" << std::endl
        << "      attr8=\"4M\"" << std::endl
        << "      attr9=\"4294967295\"" << std::endl
        << "      attr10=\"4294967296\"" << std::endl
        << "      attr11=\"4194303k\"" << std::endl
        << "      attr12=\"4194304k\"" << std::endl
        << "      attr13=\"999999999999999999999999999999999999\"" << std::endl
        << "      attr14=\"65535\"" << std::endl
        << "      attr15=\"65536\"" << std::endl
        << "      attr16=\"  -2 \"" << std::endl
        << "      attr17=\"127\"" << std::endl
        << "      attr18=\"128\"" << std::endl
        << "      attr19=\"-128\"" << std::endl
        << "      attr20=\"-129\"" << std::endl
        << "      attr21=\"4095  M  \"" << std::endl
        << "      attr22=\"4096m\"" << std::endl
        << "      attr23=\"12345 6789\" />" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    auto doc = MakeDomDocumentUniquePtr(ParseXmlConfig(xml.data(), xml.size(),
        "US-ASCII"));

    const DOMElement *root = doc->getDocumentElement();
    ASSERT_TRUE(root != nullptr);
    ASSERT_EQ(root->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(root->getNodeName()), "testDocument");

    DOMNode *child = root->getFirstChild();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    const DOMText *text_node = static_cast<const DOMText *>(child);
    ASSERT_TRUE(IsAllWhitespace(*text_node));

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "elem");
    const DOMElement *elem = static_cast<const DOMElement *>(child);
    bool caught = false;

    try {
      TAttrReader::GetInt<int>(*elem, "wrong_attr");
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "wrong_attr");
      ASSERT_EQ(x.GetElementName(), "elem");
      ASSERT_EQ(x.GetLine(), 25U);
      ASSERT_EQ(x.GetColumn(), 29U);
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetInt<int>(*elem, "attr1");
    } catch (const TMissingAttrValue &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrName(), "attr1");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr2"), 5);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr3"), 60);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr16"), -2);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr4", TOpts::ALLOW_K),
        20 * 1024);
    ASSERT_EQ(TAttrReader::GetInt<unsigned int>(*elem, "attr4",
        TOpts::ALLOW_K), 20U * 1024U);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr5", TOpts::ALLOW_K),
        16 * 1024);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr6", TOpts::ALLOW_K),
        8 * 1024);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr7", TOpts::ALLOW_M),
        2 * 1024 * 1024);
    ASSERT_EQ(TAttrReader::GetInt<int>(*elem, "attr8", TOpts::ALLOW_M),
        4 * 1024 * 1024);
    caught = false;

    try {
      TAttrReader::GetInt<int>(*elem, "attr23");
    } catch (const TInvalidSignedIntegerAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "12345 6789");
      ASSERT_EQ(x.GetAttrName(), "attr23");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetInt<unsigned int>(*elem, "attr8", TOpts::ALLOW_K);
    } catch (const TInvalidUnsignedIntegerAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "4M");
      ASSERT_EQ(x.GetAttrName(), "attr8");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetInt<int>(*elem, "attr13");
    } catch (const TInvalidSignedIntegerAttr &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "999999999999999999999999999999999999");
      ASSERT_EQ(x.GetAttrName(), "attr13");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_EQ(TAttrReader::GetInt<int8_t>(*elem, "attr17"), 127);
    ASSERT_EQ(TAttrReader::GetInt<int8_t>(*elem, "attr19"), -128);
    caught = false;

    try {
      TAttrReader::GetInt<int8_t>(*elem, "attr18");
    } catch (const TAttrOutOfRange &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "128");
      ASSERT_EQ(x.GetAttrName(), "attr18");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    caught = false;

    try {
      TAttrReader::GetInt<int8_t>(*elem, "attr20");
    } catch (const TAttrOutOfRange &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "-129");
      ASSERT_EQ(x.GetAttrName(), "attr20");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_EQ(TAttrReader::GetInt<uint16_t>(*elem, "attr14"), 65535U);
    caught = false;

    try {
      TAttrReader::GetInt<uint16_t>(*elem, "attr15");
    } catch (const TAttrOutOfRange &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "65536");
      ASSERT_EQ(x.GetAttrName(), "attr15");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_EQ(TAttrReader::GetInt<uint32_t>(*elem, "attr9"), 4294967295U);
    caught = false;

    try {
      TAttrReader::GetInt<uint32_t>(*elem, "attr10");
    } catch (const TAttrOutOfRange &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "4294967296");
      ASSERT_EQ(x.GetAttrName(), "attr10");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_EQ(TAttrReader::GetInt<uint32_t>(*elem, "attr11", TOpts::ALLOW_K),
        4194303U * 1024U);
    caught = false;

    try {
      TAttrReader::GetInt<uint32_t>(*elem, "attr12", TOpts::ALLOW_K);
    } catch (const TAttrOutOfRange &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "4294967296");
      ASSERT_EQ(x.GetAttrName(), "attr12");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
    ASSERT_EQ(TAttrReader::GetInt<uint32_t>(*elem, "attr21", TOpts::ALLOW_M),
        4095U * 1024U * 1024U);
    caught = false;

    try {
      TAttrReader::GetInt<uint32_t>(*elem, "attr22", TOpts::ALLOW_M);
    } catch (const TAttrOutOfRange &x) {
      caught = true;
      ASSERT_EQ(x.GetAttrValue(), "4294967296");
      ASSERT_EQ(x.GetAttrName(), "attr22");
      ASSERT_EQ(x.GetElementName(), "elem");
    }

    ASSERT_TRUE(caught);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
