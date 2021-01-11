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

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/XMLException.hpp>

#include <base/to_integer.h>

namespace Xml {

  namespace Config {

    struct TFileLocation {
      explicit TFileLocation(size_t line) noexcept
          : Line(line) {
      }

      TFileLocation(size_t line, size_t column) noexcept
          : Line(line),
            Column(column) {
      }

      size_t Line;

      std::optional<size_t> Column;
    };  // TFileLocation

    class TXmlError : public std::runtime_error {
      public:
      TXmlError(const std::optional<TFileLocation> &location,
          const char *msg)
          : std::runtime_error(BuildMsg(location, msg)),
            Location(location) {
      }

      explicit TXmlError(const xercesc::XMLException &x)
          : TXmlError(BuildInfo(x)) {
      }

      const std::optional<TFileLocation> &GetLocation() const noexcept {
        return Location;
      }

      private:
      static std::pair<TFileLocation, std::string>
      BuildInfo(const xercesc::XMLException &x);

      static std::string BuildMsg(const std::optional<TFileLocation> &location,
          const char *msg);

      explicit TXmlError(const std::pair<TFileLocation, std::string> &info)
          : TXmlError(info.first, info.second.c_str()) {
      }

      std::optional<TFileLocation> Location;
    };  // TXmlError

    class TSaxParseError : public TXmlError {
      public:
      explicit TSaxParseError(const xercesc::SAXParseException &x)
          : TSaxParseError(BuildInfo(x)) {
      }

      private:
      static std::pair<TFileLocation, std::string> BuildInfo(
          const xercesc::SAXParseException &x);

      explicit TSaxParseError(
          const std::pair<TFileLocation, std::string> &info)
          : TXmlError(info.first, info.second.c_str()) {
      }
    };  // TSaxParseError

    class TDomError : public TXmlError {
      public:
      explicit TDomError(const xercesc::DOMException &x)
          : TXmlError(std::nullopt, BuildMsg(x).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMException &x);
    };  // TDomError

    class TDocumentEncodingError : public TXmlError {
      protected:
      explicit TDocumentEncodingError(const char *msg)
          : TXmlError(std::nullopt, msg) {
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
      TWrongEncoding(const char *encoding, const char *expected_encoding)
          : TDocumentEncodingError(
                BuildMsg(encoding, expected_encoding).c_str()),
          Encoding(encoding),
          ExpectedEncoding(expected_encoding) {
      }

      /* Return the document's actual encoding (not the expected one). */
      const std::string &GetEncoding() const noexcept {
        
        return Encoding;
      }

      /* Return the document's expected encoding. */
      const std::string &GetExpectedEncoding() const noexcept {
        
        return ExpectedEncoding;
      }

      private:
      static std::string BuildMsg(const char *encoding,
          const char *expected_encoding);

      std::string Encoding;

      std::string ExpectedEncoding;
    };  // TWrongEncoding

    class TContentError : public TXmlError {
      protected:
      TContentError(const xercesc::DOMNode &node, const char *msg)
          : TXmlError(BuildLocation(node), msg) {
      }

      private:
      static std::optional<TFileLocation> BuildLocation(
          const xercesc::DOMNode &node);
    };  // TContentError

    class TUnexpectedText : public TContentError {
      public:
      explicit TUnexpectedText(const xercesc::DOMNode &node)
          : TContentError(node, "XML document contains unexpected text") {
      }
    };  // TUnexpectedText

    class TElementError : public TContentError {
      public:
      const std::string &GetElementName() const noexcept {
        return ElementName;
      }

      protected:
      TElementError(const xercesc::DOMElement &elem, const char *msg);

      private:
      std::string ElementName;
    };  // TElementError

    class TDuplicateElement : public TElementError {
      public:
      explicit TDuplicateElement(const xercesc::DOMElement &elem)
          : TElementError(elem, BuildMsg(elem).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem);
    };  // TDuplicateElement

    class TUnknownElement : public TElementError {
      public:
      TUnknownElement(const xercesc::DOMElement &elem, const char *msg)
          : TElementError(elem, msg) {
      }

      explicit TUnknownElement(const xercesc::DOMElement &elem)
          : TUnknownElement(elem, BuildMsg(elem).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem);
    };  // TUnknownElement

    class TUnexpectedElementName : public TUnknownElement {
      public:
      TUnexpectedElementName(const xercesc::DOMElement &elem,
          const char *expected_elem_name)
          : TUnknownElement(elem, BuildMsg(elem, expected_elem_name).c_str()),
            ExpectedElementName(expected_elem_name) {
      }

      const std::string &GetExpectedElementName() const noexcept {
        return ExpectedElementName;
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *expected_elem_name);

      std::string ExpectedElementName;
    };  // TUnexpectedElementName

    class TMissingChildElement : public TElementError {
      public:
      TMissingChildElement(const xercesc::DOMElement &elem,
          const char *child_elem_name)
          : TElementError(elem, BuildMsg(elem, child_elem_name).c_str()),
            ChildElementName(child_elem_name) {
      }

      const std::string &GetChildElementName() const noexcept {
        return ChildElementName;
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *child_elem_name);

      std::string ChildElementName;
    };  // TMissingChildElement

    class TExpectedLeaf : public TElementError {
      public:
      explicit TExpectedLeaf(const xercesc::DOMElement &elem)
          : TElementError(elem, BuildMsg(elem).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem);
    };  // TExpectedLeaf

    class TAttrError : public TElementError {
      public:
      const std::string &GetAttrName() const noexcept {
        return AttrName;
      }

      protected:
      TAttrError(const xercesc::DOMElement &elem, const char *attr_name,
          const char *msg)
          : TElementError(elem, msg),
            AttrName(attr_name) {
      }

      private:
      std::string AttrName;
    };  // TAttrError

    class TMissingAttrValue : public TAttrError {
      public:
      TMissingAttrValue(const xercesc::DOMElement &elem, const char *attr_name)
          : TAttrError(elem, attr_name, BuildMsg(elem, attr_name).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name);
    };  // TMissingAttrValue

    class TInvalidAttr : public TAttrError {
      public:
      TInvalidAttr(const xercesc::DOMElement &elem, const char *attr_name,
          const char *attr_value, const char *msg)
          : TAttrError(elem, attr_name, msg),
            AttrValue(attr_value) {
      }

      TInvalidAttr(const xercesc::DOMElement &elem, const char *attr_name,
          const char *attr_value)
          : TInvalidAttr(elem, attr_name, attr_value,
                BuildMsg(elem, attr_name, attr_value).c_str()) {
      }

      const std::string &GetAttrValue() const noexcept {
        return AttrValue;
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value);

      std::string AttrValue;
    };  // TInvalidAttr

    class TInvalidBoolAttr : public TInvalidAttr {
      public:
      TInvalidBoolAttr(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value,
          const char *true_value, const char *false_value)
          : TInvalidAttr(elem, attr_name, attr_value,
                BuildMsg(elem, attr_name, attr_value, true_value,
                    false_value).c_str()),
            TrueValue(true_value),
            FalseValue(false_value) {
      }

      const std::string &GetTrueValue() const noexcept {
        return TrueValue;
      }

      const std::string &GetFalseValue() const noexcept {
        return FalseValue;
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value,
          const char *true_value, const char *false_value);

      std::string TrueValue;

      std::string FalseValue;
    };  // TInvalidBoolAttr

    class TAttrOutOfRange : public TInvalidAttr {
      public:
      TAttrOutOfRange(const xercesc::DOMElement &elem, const char *attr_name,
          const char *attr_value)
          : TInvalidAttr(elem, attr_name, attr_value,
                BuildMsg(elem, attr_name, attr_value).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value);
    };  // TAttrOutOfRange

    class TInvalidIntegerAttr : public TInvalidAttr {
      protected:
      TInvalidIntegerAttr(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value, const char *msg)
          : TInvalidAttr(elem, attr_name, attr_value, msg) {
      }
    };  // TInvalidIntegerAttr

    class TWrongUnsignedIntegerBase : public TInvalidIntegerAttr {
      public:
      TWrongUnsignedIntegerBase(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value, Base::TBase found,
          unsigned int allowed)
          : TInvalidIntegerAttr(elem, attr_name, attr_value,
                BuildMsg(elem, attr_name, attr_value, found, allowed).c_str()),
            Found(found),
            Allowed(allowed) {
      }

      Base::TBase GetFoundBase() const noexcept {
        return Found;
      }

      unsigned int GetAllowedBases() const noexcept {
        return Allowed;
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value, Base::TBase found,
          unsigned int allowed);

      Base::TBase Found;

      /* This is a bitfield of values found in Base::TBase. */
      unsigned int Allowed;
    };

    class TInvalidUnsignedIntegerAttr : public TInvalidIntegerAttr {
      public:
      TInvalidUnsignedIntegerAttr(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value)
          : TInvalidIntegerAttr(elem, attr_name, attr_value,
                BuildMsg(elem, attr_name, attr_value).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value);
    };  // TInvalidUnsignedIntegerAttr

    class TInvalidSignedIntegerAttr : public TInvalidIntegerAttr {
      public:
      TInvalidSignedIntegerAttr(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value)
          : TInvalidIntegerAttr(elem, attr_name, attr_value,
                BuildMsg(elem, attr_name, attr_value).c_str()) {
      }

      private:
      static std::string BuildMsg(const xercesc::DOMElement &elem,
          const char *attr_name, const char *attr_value);
    };  // TInvalidSignedIntegerAttr

  }  // Config

}  // Xml
