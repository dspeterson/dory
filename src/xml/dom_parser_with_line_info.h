/* <xml/dom_parser_with_line_info.h>

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

   A DOM Parser that subclasses xercesc::XercesDOMParser and provides source
   line and column information for nodes in DOM tree.
 */

#pragma once

#include <memory>

#include <xercesc/dom/DOMUserDataHandler.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <base/no_copy_semantics.h>

namespace Xml {

  /* DOM parser that attaches source line and column information as "user data"
     to nodes in DOM tree. */
  class TDomParserWithLineInfo : public xercesc::XercesDOMParser {
    NO_COPY_SEMANTICS(TDomParserWithLineInfo);

    public:
    /* Create a parser that attaches line/column information to each DOM tree
       node using 'line_info_key'.  Caller retains ownership of
       'line_info_key'. */
    explicit TDomParserWithLineInfo(const XMLCh *line_info_key);

    /* Create a parser that attaches line/column information to each DOM tree
       node using 'line_info_key'.  Caller retains ownership of
       'line_info_key'. */
    explicit TDomParserWithLineInfo(const char *line_info_key);

    /* Create a parser that attaches line/column information to each DOM tree
       node using the key supplied by TXmlInputLineInfo::GetDefaultKey(). */
    TDomParserWithLineInfo();

    const XMLCh *GetLineInfoKey() const {
      return LineInfoKey.get();
    }

    ~TDomParserWithLineInfo() override;

    /* Called when parser encounters start of document. */
    void startDocument() override;

    /* Called when parser encounters XML element. */
    void startElement(const xercesc::XMLElementDecl &elemDecl,
        const unsigned int urlId, const XMLCh *const elemPrefix,
        const xercesc::RefVectorOf<xercesc::XMLAttr> &attrList,
        const XMLSize_t attrCount, const bool isEmpty,
        const bool isRoot) override;

    /* Called when parser encounters character data in XML document. */
    void docCharacters(const XMLCh *const chars,
        const XMLSize_t length, const bool cdataSection) override;

    /* Called when parser encounters comment in XML document. */
    void docComment(const XMLCh *const comment) override;

    /* Called when parser encounters processing instruction in XML document. */
    void docPI(const XMLCh *const target, const XMLCh *const data) override;

    private:
    class TUserDataHandler final : public xercesc::DOMUserDataHandler {
      NO_COPY_SEMANTICS(TUserDataHandler);

      /* Called by singleton accessor. */
      TUserDataHandler() = default;

      public:
      /* Singleton accessor. */
      static TUserDataHandler &The() noexcept;

      void handle(xercesc::DOMUserDataHandler::DOMOperationType operation,
          const XMLCh *const key, void *data, const xercesc::DOMNode *src,
          xercesc::DOMNode *dst) override;
    };  // TUserDataHandler

    void SetUserData();

    std::unique_ptr<xercesc::ErrorHandler> ErrHandler;

    /* Key for attaching TXmlInputLineInfo objects to DOM tree nodes. */
    std::unique_ptr<const XMLCh, void (*)(const XMLCh *)> LineInfoKey;
  };  // TDomParserWithLineInfo

}  // Xml
