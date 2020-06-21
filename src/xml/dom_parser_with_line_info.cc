/* <xml/dom_parser_with_line_info.cc>

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

   Implements <xml/dom_parser_with_line_info.h>.
 */

#include <xml/dom_parser_with_line_info.h>

#include <xercesc/internal/XMLScanner.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/Locator.hpp>

#include <base/no_default_case.h>
#include <xml/xml_input_line_info.h>
#include <xml/xml_string_util.h>

using namespace xercesc;

using namespace Xml;

TDomParserWithLineInfo::TDomParserWithLineInfo(const XMLCh *line_info_key)
    : ErrHandler(new HandlerBase),
      LineInfoKey(MakeXmlStringUniquePtr<const XMLCh>(
          XMLString::replicate(line_info_key))) {
}

TDomParserWithLineInfo::TDomParserWithLineInfo(const char *line_info_key)
    : ErrHandler(new HandlerBase),
      LineInfoKey(GetTranscoded(line_info_key)) {
  setErrorHandler(ErrHandler.get());
}

TDomParserWithLineInfo::TDomParserWithLineInfo()
    : TDomParserWithLineInfo(TXmlInputLineInfo::GetDefaultKey()) {
}

TDomParserWithLineInfo::~TDomParserWithLineInfo() {
  /* Uninstall error handler.  Once our error handler has been destroyed, our
     base class must not try to invoke it. */
  setErrorHandler(nullptr);
}

void TDomParserWithLineInfo::startDocument() {
  XercesDOMParser::startDocument();
  SetUserData();
}

void TDomParserWithLineInfo::startElement(const XMLElementDecl &elemDecl,
    const unsigned int urlId, const XMLCh *const elemPrefix,
    const RefVectorOf<XMLAttr> &attrList, const XMLSize_t attrCount,
    const bool isEmpty, const bool isRoot) {
  XercesDOMParser::startElement(elemDecl, urlId, elemPrefix, attrList,
      attrCount, isEmpty, isRoot);
  SetUserData();
}

void TDomParserWithLineInfo::docCharacters(const XMLCh *const chars,
    const XMLSize_t length, const bool cdataSection) {
  XercesDOMParser::docCharacters(chars, length, cdataSection);
  SetUserData();
}

void TDomParserWithLineInfo::docComment(const XMLCh *const comment) {
  XercesDOMParser::docComment(comment);
  SetUserData();
}

void TDomParserWithLineInfo::docPI(const XMLCh *const target,
    const XMLCh *const data) {
  XercesDOMParser::docPI(target, data);
  SetUserData();
}

TDomParserWithLineInfo::TUserDataHandler &
TDomParserWithLineInfo::TUserDataHandler::The() noexcept {
  static TUserDataHandler singleton;
  return singleton;
}

void TDomParserWithLineInfo::TUserDataHandler::handle(
    DOMOperationType operation, const XMLCh *const /*key*/, void *data,
    const DOMNode * /*src*/, DOMNode * /*dst*/) {
  auto *line_info = static_cast<TXmlInputLineInfo *>(data);

  switch (operation) {
    case NODE_CLONED:
    case NODE_IMPORTED: {
      line_info->AddRef();
      break;
    }
    case NODE_DELETED: {
      if (line_info->RemoveRef() == 0) {
        delete line_info;
      }

      break;
    }
    case NODE_RENAMED:
    case NODE_ADOPTED: {
      break;
    }
    NO_DEFAULT_CASE;
  }
}

void TDomParserWithLineInfo::SetUserData() {
  DOMNode *cur = getCurrentNode();
  const Locator* locator = getScanner()->getLocator();
  TUserDataHandler &handler = TUserDataHandler::The();
  cur->setUserData(LineInfoKey.get(),
      new TXmlInputLineInfo(locator->getLineNumber(),
          locator->getColumnNumber()),
      &handler);
}
