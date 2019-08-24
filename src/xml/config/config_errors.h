/* <xml/config/config_errors.h>

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

   Exception classes related to working with config files and Xerces XML
   processing library.  Some of these classes correspond to exceptions defined
   by Xerces, but are derived from std::runtime_error.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/XMLException.hpp>
#include <base/to_integer.h>

namespace Xml {

  namespace Config {

    class TXmlConfigError : public std::runtime_error {
      protected:
      explicit TXmlConfigError(const char *msg)
          : std::runtime_error(msg) {
      }
    };  // TXmlConfigError

    class TErrorLineInfo {
      public:
      virtual size_t GetLine() const noexcept = 0;

      virtual ~TErrorLineInfo() = default;
    };  // TErrorLineInfo

    class TErrorLineOnlyInfo : public TErrorLineInfo {
      public:
      ~TErrorLineOnlyInfo() override = default;
    };  // TErrorLineOnlyInfo

    class TErrorLineAndColumnInfo : public TErrorLineInfo {
      public:
      virtual size_t GetColumn() const noexcept = 0;
    };  // TErrorLineInfo

    class TXmlException : public TXmlConfigError,
        public TErrorLineOnlyInfo {
      public:
      explicit TXmlException(const xercesc::XMLException &x);

      size_t GetLine() const noexcept override;

      private:
      size_t Line;
    };  // TXmlException

    class TSaxParseException : public TXmlConfigError,
        public TErrorLineAndColumnInfo {
      public:
      explicit TSaxParseException(const xercesc::SAXParseException &x);

      size_t GetLine() const noexcept override;

      size_t GetColumn() const noexcept override;

      private:
      size_t Line;

      size_t Column;
    };  // TSaxParseException

    class TDomException : public TXmlConfigError {
      public:
      explicit TDomException(const xercesc::DOMException &x);
    };  // TDomException

    class TDocumentEncodingError : public TXmlConfigError {
      protected:
      explicit TDocumentEncodingError(const char *msg)
          : TXmlConfigError(msg) {
      }
    };  // TDocumentEncodingError

    class TMissingEncoding : public TDocumentEncodingError {
      public:
      TMissingEncoding()
          : TDocumentEncodingError("XML document must specify encoding") {
      }
    };  // TMissingEncoding

    class TWrongEncoding : public TDocumentEncodingError {
      public:
      TWrongEncoding(const char *encoding, const char *expected_encoding);

      /* Return the document's actual encoding (not the expected one). */
      const std::string &GetEncoding() const noexcept {
        assert(this);
        return Encoding;
      }

      /* Return the document's expected encoding. */
      const std::string &GetExpectedEncoding() const noexcept {
        assert(this);
        return ExpectedEncoding;
      }

      private:
      std::string Encoding;

      std::string ExpectedEncoding;
    };  // TWrongEncoding

    class TContentError : public TXmlConfigError,
        public TErrorLineAndColumnInfo {
      public:
      size_t GetLine() const noexcept override {
        assert(this);
        return Line;
      }

      size_t GetColumn() const noexcept override {
        assert(this);
        return Column;
      }

      TContentError(const char *msg, const xercesc::DOMNode &location);

      private:
      size_t Line;

      size_t Column;
    };  // TContentError

    class TUnexpectedText : public TContentError {
      public:
      explicit TUnexpectedText(const xercesc::DOMNode &location)
          : TContentError("XML document contains unexpected text", location) {
      }
    };  // TUnexpectedText

    class TElementError : public TContentError {
      public:
      const std::string &GetElementName() const noexcept {
        assert(this);
        return ElementName;
      }

      TElementError(const char *msg, const xercesc::DOMElement &location);

      private:
      std::string ElementName;
    };  // TElementError

    class TDuplicateElement : public TElementError {
      public:
      explicit TDuplicateElement(const xercesc::DOMElement &location)
          : TElementError("XML document contains unexpected duplicate element",
                location) {
      }
    };  // TDuplicateElement

    class TUnknownElement : public TElementError {
      public:
      TUnknownElement(const char *msg, const xercesc::DOMElement &location)
          : TElementError(msg, location) {
      }

      explicit TUnknownElement(const xercesc::DOMElement &location)
          : TUnknownElement("XML document contains unknown element",
                location) {
      }
    };  // TUnknownElement

    class TUnexpectedElementName : public TUnknownElement {
      public:
      TUnexpectedElementName(const xercesc::DOMElement &location,
          const char *expected_element_name)
          : TUnknownElement("XML element is not of expected type", location),
            ExpectedElementName(expected_element_name) {
      }

      const std::string &GetExpectedElementName() const noexcept {
        assert(this);
        return ExpectedElementName;
      }

      private:
      std::string ExpectedElementName;
    };  // TUnexpectedElementName

    class TMissingChildElement : public TElementError {
      public:
      TMissingChildElement(const xercesc::DOMElement &location,
          const char *child_element_name)
          : TElementError("XML element is missing child element", location),
            ChildElementName(child_element_name) {
      }

      const std::string &GetChildElementName() const noexcept {
        assert(this);
        return ChildElementName;
      }

      private:
      std::string ChildElementName;
    };  // TMissingChildElement

    class TExpectedLeaf : public TElementError {
      public:
      explicit TExpectedLeaf(const xercesc::DOMElement &location)
          : TElementError("XML element must be a leaf node", location) {
      }
    };  // TExpectedLeaf

    class TAttrError : public TElementError {
      public:
      const std::string &GetAttrName() const noexcept {
        assert(this);
        return AttrName;
      }

      TAttrError(const char *msg, const xercesc::DOMElement &location,
          const char *attr_name)
          : TElementError(msg, location),
            AttrName(attr_name) {
      }

      private:
      std::string AttrName;
    };  // TAttrError

    class TMissingAttrValue : public TAttrError {
      public:
      TMissingAttrValue(const xercesc::DOMElement &location,
          const char *attr_name)
          : TAttrError("XML element is missing attribute value", location,
                attr_name) {
      }
    };  // TMissingAttrValue

    class TInvalidAttr : public TAttrError {
      public:
      TInvalidAttr(const char *msg, const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value)
          : TAttrError(msg, location, attr_name),
            AttrValue(attr_value) {
      }

      TInvalidAttr(const xercesc::DOMElement &location, const char *attr_name,
          const char *attr_value)
          : TInvalidAttr("XML attribute value is invalid", location, attr_name,
              attr_value) {
      }

      const std::string &GetAttrValue() const noexcept {
        assert(this);
        return AttrValue;
      }

      private:
      std::string AttrValue;
    };  // TInvalidAttr

    class TInvalidBoolAttr : public TInvalidAttr {
      public:
      TInvalidBoolAttr(const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value,
          const char *true_value, const char *false_value);

      const std::string &GetTrueValue() const noexcept {
        assert(this);
        return TrueValue;
      }

      const std::string &GetFalseValue() const noexcept {
        assert(this);
        return FalseValue;
      }

      private:
      std::string TrueValue;

      std::string FalseValue;
    };  // TInvalidBoolAttr

    class TInvalidIntegerAttr : public TInvalidAttr {
      public:
      TInvalidIntegerAttr(const char *msg, const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value)
          : TInvalidAttr(msg, location, attr_name, attr_value) {
      }
    };  // TInvalidIntegerAttr

    class TInvalidUnsignedIntegerAttr : public TInvalidIntegerAttr {
      public:
      TInvalidUnsignedIntegerAttr(const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value)
          : TInvalidIntegerAttr(
              "XML attribute is not a valid unsigned integer", location,
              attr_name, attr_value) {
      }
    };  // TInvalidUnsignedIntegerAttr

    class TInvalidSignedIntegerAttr : public TInvalidIntegerAttr {
      public:
      TInvalidSignedIntegerAttr(const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value)
          : TInvalidIntegerAttr(
              "XML attribute is not a valid signed integer", location,
              attr_name, attr_value) {
      }
    };  // TInvalidSignedIntegerAttr

    class TWrongUnsignedIntegerBase : public TInvalidIntegerAttr {
      public:
      TWrongUnsignedIntegerBase(const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value, Base::TBase found,
          unsigned int allowed)
          : TInvalidIntegerAttr(
                "XML attribute unsigned integer value has unsupported base",
                location, attr_name, attr_value),
            Found(found),
            Allowed(allowed) {
      }

      Base::TBase GetFoundBase() const noexcept {
        assert(this);
        return Found;
      }

      unsigned int GetAllowedBases() const noexcept {
        assert(this);
        return Allowed;
      }

      private:
      Base::TBase Found;

      /* This is a bitfield of values found in Base::TBase. */
      unsigned int Allowed;
    };

    class TAttrOutOfRange : public TInvalidAttr {
      public:
      TAttrOutOfRange(const xercesc::DOMElement &location,
          const char *attr_name, const char *attr_value)
          : TInvalidAttr("XML attribute value is out of range", location,
              attr_name, attr_value) {
      }
    };  // TAttrOutOfRange

  }  // Config

}  // Xml
