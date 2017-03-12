/* <xml/dom_document_util.h>

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

   Utilities for working with Xerces DOM documents.
 */

#pragma once

#include <memory>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/util/XercesDefs.hpp>

namespace Xml {

  /* This is intended as a custom deleter function to be supplied to smart
     pointer class templates such as std::unique_ptr. */
  inline void DeleteDomDocument(xercesc::DOMDocument *doc) {
    doc->release();
  }

  /* Create and return a std::unique_ptr that takes ownership of parameter
     'doc'.  The std::unique_ptr will have a custom deleter that invokes
     DOMDocument::release() on the document. */
  inline
  std::unique_ptr<xercesc::DOMDocument, void (*)(xercesc::DOMDocument *)>
  MakeDomDocumentUniquePtr(xercesc::DOMDocument *doc) {
    return std::unique_ptr<xercesc::DOMDocument,
        void (*)(xercesc::DOMDocument *)>(doc, DeleteDomDocument);
  }

  /* Create and return an empty std::unique_ptr that can take ownership of a
     value of type 'xercesc::DOMDocument *'.  The std::unique_ptr will have a
     custom deleter that invokes DOMDocument::release() on the document. */
  inline
  std::unique_ptr<xercesc::DOMDocument, void (*)(xercesc::DOMDocument *)>
  MakeEmptyDomDocumentUniquePtr() {
    return std::unique_ptr<xercesc::DOMDocument,
        void (*)(xercesc::DOMDocument *)>(nullptr, DeleteDomDocument);
  }

}  // Xml
