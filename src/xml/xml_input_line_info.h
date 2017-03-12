/* <xml/xml_input_line_info.h>

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

   Class for storing source line and column information for XML content parsed
   by Xerces XML processing library.
 */

#pragma once

#include <cassert>
#include <cstddef>

#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <base/no_copy_semantics.h>

namespace Xml {

  /* This class is for attaching source line and column information to nodes
     created by Xerces DOM parser.  An instance of this class is attached to a
     node as "user data". */
  class TXmlInputLineInfo final {
    NO_COPY_SEMANTICS(TXmlInputLineInfo);

    public:
    static const char *GetDefaultKey();

    /* Return a pointer to the TXmlInputLineInfo object that the given node is
       tagged with, or nullptr if there is no such tag. */
    static const TXmlInputLineInfo *Get(const xercesc::DOMNode &node,
        const XMLCh *line_info_key) {
      assert(&node);
      return static_cast<const TXmlInputLineInfo *>(
          node.getUserData(line_info_key));
    }

    static const TXmlInputLineInfo *Get(const xercesc::DOMNode &node,
        const char *line_info_key);

    /* Same as above, but uses GetDefaultKey() as the key for looking up the
       line info. */
    static const TXmlInputLineInfo *Get(const xercesc::DOMNode &node);

    TXmlInputLineInfo(XMLFileLoc line_num, XMLFileLoc column_num)
        : LineNum(line_num),
          ColumnNum(column_num),
          RefCount(1) {
    }

    XMLFileLoc GetLineNum() const noexcept {
      assert(this);
      return LineNum;
    }

    XMLFileLoc GetColumnNum() const noexcept {
      assert(this);
      return ColumnNum;
    }

    /* Increment reference count on object.  Nodes in DOM tree hold references
       to these objects. */
    void AddRef() noexcept {
      assert(this);
      assert(RefCount);
      ++RefCount;
    }

    /* Decrement reference count on object.  When returned new reference count
       reaches 0, caller must delete object. */
    size_t RemoveRef() noexcept {
      assert(this);
      assert(RefCount);
      return --RefCount;
    }

    private:
    const XMLFileLoc LineNum;

    const XMLFileLoc ColumnNum;

    size_t RefCount;
  };  // TXmlInputLineInfo

}  // Xml
