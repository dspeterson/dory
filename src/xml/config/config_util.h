/* <xml/config/config_util.h>

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

   Utilities for working with config files using Xerces XML processing library.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMText.hpp>

#include <base/opt.h>
#include <xml/config/config_errors.h>
#include <xml/xml_string_util.h>

namespace Xml {

  namespace Config {

    /* Parse the given buffer of XML content.  'expected_encoding' should be
       something like "US-ASCII".  Caller is responsible for destroying
       returned object by calling its release() method. */
    xercesc::DOMDocument *ParseXmlConfig(const void *buf, size_t buf_size,
        const char *expected_encoding);

    /* Return true if the text associated with this node contains no
       non-whitespace characters, or false otherwise. */
    bool IsAllWhitespace(const xercesc::DOMText &node);

    /* Treat 'parent' as the root of a subtree with child elements representing
       subsections.  Return a hash where the keys are subsection element names
       and and the values are pointers to their corresponding elements.
       'subsection_vec' describes the subsections we expect to find, where the
       first item of a pair is the subsection name, and the second item
       indicates whether the subsection is required.  true indicates required
       and false indicates optional. */
    std::unordered_map<std::string, const xercesc::DOMElement *>
    GetSubsectionElements(const xercesc::DOMElement &parent,
        const std::vector<std::pair<std::string, bool>> &subsection_vec,
        bool allow_unknown_subsection);

    std::vector<const xercesc::DOMElement *>
    GetItemListElements(const xercesc::DOMElement &parent,
        const char *item_name);

    /* Verify that 'elem' has no child elements.  If a child element is found,
       throw a TUnknownElement exception that references a found child element.
       A nonelement child node (for instance, a DOMCharacterData node) will
       _not_ cause this function to throw. */
    void RequireNoChildElement(const xercesc::DOMElement &elem);

    /* Verify that 'elem' has no grandchild elements.  If a grandchild element
       is found, throw a TUnknownElement exception that references a found
       grandchild element.  A nonelement grandchild node (for instance, a
       DOMCharacterData node) will _not_ cause this function to throw.  This
       is a convenient alternative to calling RequireNoChildElement() on every
       child element of 'elem'. */
    void RequireNoGrandchildElement(const xercesc::DOMElement &elem);

    /* Verify that 'elem' is a leaf (i.e. has no child nodes of any type).
       If a child is found, throw a TExpectedLeaf exception that references
       'elem'. */
    void RequireLeaf(const xercesc::DOMElement &elem);

    /* Verify that every child element of 'elem' is a leaf (i.e. has no child
       nodes of any type).  If a nonleaf child is found, throw a TExpectedLeaf
       exception that references the nonleaf child.  This is a convenient
       alternative to calling RequireLeaf() on every child element of 'elem'.
     */
    void RequireAllChildElementLeaves(const xercesc::DOMElement &elem);

    /* Class for reading attributes from XML elements.  All methods are static,
       and class can not be instantiated.  The only purpose for creating a
       class here is to hide implementation details as private methods. */
    class TAttrReader {
      TAttrReader() = delete;

      public:
      /* Options for reading attribute values. */
      enum TOpts {
        /* Require the attribute to at least be present, even if its value is
           the empty string.  Throw TMissingAttrValue if attribute is not
           present. */
        REQUIRE_PRESENCE = 1U << 0,

        /* For GetOptInt2() method, require that either a valid integer or
           'empty_value_name' is provided.  Throw TMissingAttrValue if
           attribute value is the empty string or all whitespace. */
        STRICT_EMPTY_VALUE = 1U << 1,

        /* Trim leading and trailing whitespace from string values.  This is
           always done for integer and boolean values. */
        TRIM_WHITESPACE = 1U << 2,

        /* For GetString(), throw TMissingAttrValue if the attribute value is
           the empty string (after trimming whitespace if 'TRIM_WHITESPACE' was
           specified). */
        THROW_IF_EMPTY = 1U << 3,

        /* Use case-sensitive string matching when looking for boolean
           attribute values.  For instance, you may want to allow "true" but
           not "TRUE". */
        CASE_SENSITIVE = 1U << 4,

        /* Allow syntax like '4k' as shorthand for (4 * 1024). */
        ALLOW_K = 1U << 5,

        /* Allow syntax like '4m' as shorthand for (4 * 1024 * 1024). */
        ALLOW_M = 1U << 6
      };  // TOpts

      /* See if 'elem' has an attribute named 'attr_name'.  If not, return the
         unknown value.  Otherwise, return the attribute value.
         allowed opts: TRIM_WHITESPACE */
      static Base::TOpt<std::string> GetOptString(
          const xercesc::DOMElement &elem, const char *attr_name,
          unsigned int opts = 0);

      /* Return the value of the attribute of 'elem' with name 'attr_name'.
         Throw TMissingAttrValue if no such attribute exists.
         allowed opts: TRIM_WHITESPACE, THROW_IF_EMPTY */
      static std::string GetString(const xercesc::DOMElement &elem,
          const char *attr_name, unsigned int opts = 0);

      /* Get an optional boolean value, specifying string constants for true
         and false.  For instance, you might pass "yes" for 'true_value' and
         "no" for 'false_value'.  Throw TInvalidBoolAttr if the attribute is
         not a valid boolean value.
         allowed opts: REQUIRE_PRESENCE, CASE_SENSITIVE */
      static Base::TOpt<bool> GetOptNamedBool(const xercesc::DOMElement &elem,
          const char *attr_name, const char *true_value,
          const char *false_value,
          unsigned int opts = 0);

      /* Get an optional boolean value, using "true" and "false" as the
         expected string literals.  Throw TInvalidBoolAttr if the attribute is
         not a valid boolean value.
         allowed opts: REQUIRE_PRESENCE, CASE_SENSITIVE */
      static inline Base::TOpt<bool> GetOptBool(
          const xercesc::DOMElement &elem, const char *attr_name,
          unsigned int opts = 0) {
        return GetOptNamedBool(elem, attr_name, "true", "false", opts);
      }

      /* Get a required boolean value, specifying string constants for true
         and false.  For instance, you might pass "yes" for 'true_value' and
         "no" for 'false_value'.  Throw TMissingAttrValue if no such attribute
         exists.  Throw TInvalidBoolAttr if the attribute is not a valid
         boolean value.
         allowed opts: CASE_SENSITIVE */
      static bool GetNamedBool(const xercesc::DOMElement &elem,
          const char *attr_name, const char *true_value,
          const char *false_value, unsigned int opts = 0);

      /* Get a required boolean value, using "true" and "false" as the
         expected string literals.  Throw TMissingAttrValue if no such
         attribute exists.  Throw TInvalidBoolAttr if the attribute is not a
         valid boolean value.
         allowed opts: CASE_SENSITIVE */
      static inline bool GetBool(const xercesc::DOMElement &elem,
          const char *attr_name, unsigned int opts = 0) {
        return GetNamedBool(elem, attr_name, "true", "false", opts);
      }

      /* Get an optional integer value, whose type is specified by T.  Throw
         TInvalidSignedIntegerAttr or TInvalidUnsignedIntegerAttr if value is
         not a valid integer.  Throw TAttrOutOfRange if value is out of range
         for integer type T.
         allowed opts: REQUIRE_PRESENCE, ALLOW_K, ALLOW_M */
      template <typename T>
      static typename Base::TOpt<T> GetOptInt(const xercesc::DOMElement &elem,
          const char *attr_name, unsigned int opts = 0) {
        assert(opts == (opts & (REQUIRE_PRESENCE | ALLOW_K | ALLOW_M)));
        return GetOptIntHelper<T>(elem, attr_name, nullptr, opts);
      }

      /* Get an optional integer value, whose type is specified by T.
         'empty_value_name' allows a string literal such as "unlimited" or
         "disabled" to explicitly indicate a missing value.  Throw
         TInvalidSignedIntegerAttr or TInvalidUnsignedIntegerAttr if value is
         not a valid integer.  Throw TAttrOutOfRange if value is out of range
         for integer type T.
         allowed opts: REQUIRE_PRESENCE, STRICT_EMPTY_VALUE, ALLOW_K, ALLOW_M
       */
      template <typename T>
      static typename Base::TOpt<T> GetOptInt2(const xercesc::DOMElement &elem,
          const char *attr_name, const char *empty_value_name,
          unsigned int opts = 0) {
        assert(empty_value_name);
        assert(opts == (opts &
            (REQUIRE_PRESENCE | STRICT_EMPTY_VALUE | ALLOW_K | ALLOW_M)));
        return GetOptIntHelper<T>(elem, attr_name, empty_value_name, opts);
      }

      /* Get a required integer value, whose type is specified by T.  Throw
         TInvalidSignedIntegerAttr or TInvalidUnsignedIntegerAttr if value is
         not a valid integer.  Throw TAttrOutOfRange if value is out of range
         for integer type T.  Throw TMissingAttrValue if value is missing.
         allowed opts: ALLOW_K, ALLOW_M */
      template <typename T>
      static T GetInt(const xercesc::DOMElement &elem, const char *attr_name,
          unsigned int opts = 0) {
        static_assert(std::is_integral<T>::value &&
            !std::is_same<T, bool>::value,
            "Type parameter must be integral and not bool");

        return GetIntHelper<T>(elem, attr_name, opts);
      }

      private:
      template <typename T, bool SIGNED = std::is_signed<T>::value>
      struct TIntegerTraits {
        using TWidest = intmax_t;
      };

      template <typename T>
      struct TIntegerTraits<T, false> {
        using TWidest = uintmax_t;
      };

      template <typename NARROW, typename WIDE>
      static NARROW narrow_cast(WIDE wide, const xercesc::DOMElement &elem,
          const char *attr_name) {
        NARROW result = static_cast<NARROW>(wide);

        if (static_cast<WIDE>(result) != wide) {
          throw TAttrOutOfRange(elem, attr_name,
              boost::lexical_cast<std::string>(wide).c_str());
        }

        return result;
      }

      // Base template handles integral types other than intmax_t and
      // uintmax_t.
      template <typename T>
      static T GetIntHelper(const xercesc::DOMElement &elem,
          const char *attr_name, unsigned int opts) {
        using TWidest = typename TIntegerTraits<T>::TWidest;

        // This calls a specialization.  It does not recurse.
        TWidest value = GetIntHelper<TWidest>(elem, attr_name, opts);

        return narrow_cast<T>(value, elem, attr_name);
      }

      // Base template handles integral types other than intmax_t and
      // uintmax_t.
      template <typename T>
      static typename Base::TOpt<T> GetOptIntHelper(
          const xercesc::DOMElement &elem, const char *attr_name,
          const char *empty_value_name, unsigned int opts = 0) {
        static_assert(std::is_integral<T>::value &&
            !std::is_same<T, bool>::value,
            "Type parameter must be integral and not bool");
        using TWidest = typename TIntegerTraits<T>::TWidest;

        // This calls a specialization.  It does not recurse.
        Base::TOpt<TWidest> opt_value = GetOptIntHelper<TWidest>(elem,
            attr_name, empty_value_name, opts);

        typename Base::TOpt<T> result;

        if (opt_value.IsKnown()) {
          result.MakeKnown(narrow_cast<T>(*opt_value, elem, attr_name));
        }

        return result;
      }
    };  // TAttrReader

    template <>
    intmax_t TAttrReader::GetIntHelper<intmax_t>(
        const xercesc::DOMElement &elem, const char *attr_name,
        unsigned int opts);

    template <>
    uintmax_t TAttrReader::GetIntHelper<uintmax_t>(
        const xercesc::DOMElement &elem, const char *attr_name,
        unsigned int opts);

    template <>
    Base::TOpt<intmax_t> TAttrReader::GetOptIntHelper<intmax_t>(
        const xercesc::DOMElement &elem, const char *attr_name,
        const char *empty_value_name, unsigned int opts);

    template <>
    Base::TOpt<uintmax_t> TAttrReader::GetOptIntHelper<uintmax_t>(
        const xercesc::DOMElement &elem, const char *attr_name,
        const char *empty_value_name, unsigned int opts);

  }  // Config

}  // Xml
