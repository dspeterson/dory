/* <xml/dom_parser_with_line_info.test.cc>
 
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
 
   Unit test for <xml/dom_parser_with_line_info.h>.
 */

#include <xml/dom_parser_with_line_info.h>

#include <sstream>

#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/sax/SAXParseException.hpp>

#include <xml/dom_document_util.h>
#include <xml/test/xml_test_initializer.h>
#include <xml/xml_input_line_info.h>
#include <xml/xml_string_util.h>

#include <gtest/gtest.h>

using namespace xercesc;

using namespace Xml;
using namespace Xml::Test;

namespace {

  /* The fixture for testing class TDomParserWithLineInfo. */
  class TXmlParserTest : public ::testing::Test {
    protected:
    TXmlParserTest() = default;

    ~TXmlParserTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }

    TXmlTestInitializer Initializer;  // initializes Xerces XML library
  };  // TXmlParserTest

  TEST_F(TXmlParserTest, LineInfoKeyTest) {
    static const char line_info_blurb[] = "line_info";
    TDomParserWithLineInfo parser1(line_info_blurb);
    std::string line_info_str(TranscodeToString(parser1.GetLineInfoKey()));
    ASSERT_EQ(line_info_str, line_info_blurb);
    TDomParserWithLineInfo parser2;
    line_info_str = TranscodeToString(parser2.GetLineInfoKey());
    ASSERT_EQ(line_info_str, TXmlInputLineInfo::GetDefaultKey());
  }

  TEST_F(TXmlParserTest, ParseErrorTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <noClosingTag>" << std::endl  // bad XML: no closing tag
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    MemBufInputSource input_source(
        reinterpret_cast<const XMLByte *>(xml.data()), xml.size(), "bufId");
    TDomParserWithLineInfo parser;
    bool caught = false;

    try {
      parser.parse(input_source);
    } catch (const SAXParseException &x) {
      caught = true;
      ASSERT_EQ(x.getLineNumber(), 4U);
      ASSERT_EQ(x.getColumnNumber(), 3U);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TXmlParserTest, SuccessfulParseTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<testDocument>" << std::endl
        << "  <testElement value=\"blah\" />" << std::endl
        << "</testDocument>" << std::endl;
    std::string xml(os.str());
    MemBufInputSource input_source(
        reinterpret_cast<const XMLByte *>(xml.data()), xml.size(), "bufId");
    TDomParserWithLineInfo parser;
    parser.parse(input_source);
    auto doc = MakeDomDocumentUniquePtr(parser.adoptDocument());

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
    line_info = TXmlInputLineInfo::Get(*child);
    ASSERT_TRUE(line_info != nullptr);
    ASSERT_EQ(line_info->GetLineNum(), 3U);
    ASSERT_EQ(line_info->GetColumnNum(), 3U);

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::ELEMENT_NODE);
    ASSERT_EQ(TranscodeToString(child->getNodeName()), "testElement");
    line_info = TXmlInputLineInfo::Get(*child);
    ASSERT_TRUE(line_info != nullptr);
    ASSERT_EQ(line_info->GetLineNum(), 3U);
    ASSERT_EQ(line_info->GetColumnNum(), 31U);

    DOMNamedNodeMap *attr_map = child->getAttributes();
    ASSERT_TRUE(attr_map != nullptr);
    XMLSize_t len = attr_map->getLength();
    ASSERT_EQ(len, 1U);
    auto *attr = static_cast<DOMAttr *>(attr_map->item(0));
    ASSERT_TRUE(attr != nullptr);
    ASSERT_EQ(TranscodeToString(attr->getName()), "value");
    ASSERT_EQ(TranscodeToString(attr->getValue()), "blah");

    child = child->getNextSibling();
    ASSERT_TRUE(child != nullptr);
    ASSERT_EQ(child->getNodeType(), DOMNode::TEXT_NODE);
    line_info = TXmlInputLineInfo::Get(*child);
    ASSERT_EQ(line_info->GetLineNum(), 4U);
    ASSERT_EQ(line_info->GetColumnNum(), 1U);

    child = child->getNextSibling();
    ASSERT_TRUE(child == nullptr);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
