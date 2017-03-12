/* <xml/xml_string_util.h>

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

   Utilities for working with Xerces XML strings.
 */

#pragma once

#include <memory>
#include <string>
#include <type_traits>

#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/util/XMLString.hpp>

namespace Xml {

  /* This is intended as a custom deleter function to be supplied to smart
     pointer class templates such as std::unique_ptr.  In practice, TCharType
     will be XMLCh, char, or the const variant of either type. */
  template <typename TCharType>
  void DeleteXmlString(TCharType *xml_string) {
    using TBaseCharType = typename std::remove_const<TCharType>::type;
    xercesc::XMLString::release(const_cast<TBaseCharType **>(&xml_string));
  }

  /* Create and return a std::unique_ptr that takes ownership of parameter
     'xml_string'.  The std::unique_ptr will have a custom deleter that invokes
     xercesc::XMLString::release() on the string.  In practice, TCharType will
     be XMLCh, char, or the const variant of either type. */
  template <typename TCharType>
  std::unique_ptr<TCharType, void (*)(TCharType *)>
  MakeXmlStringUniquePtr(TCharType *xml_string) {
    return std::unique_ptr<TCharType, void (*)(TCharType *)>(xml_string,
        DeleteXmlString<TCharType>);
  }

  /* Create and return an empty std::unique_ptr that can take ownership of a
     string of type 'TCharType *'.  In practice, TCharType will be XMLCh, char,
     or the const variant of either type.  The std::unique_ptr will have a
     custom deleter that invokes xercesc::XMLString::release on the string. */
  template <typename TCharType>
  std::unique_ptr<TCharType, void (*)(TCharType *)>
  MakeEmptyXmlStringUniquePtr() {
    return std::unique_ptr<TCharType, void (*)(TCharType *)>(nullptr,
        DeleteXmlString<TCharType>);
  }

  /* Transcode a string from 'const XMLCh *' to a 'const char *' representation
     in the native code page, and return a std::unique_ptr that takes ownership
     of the result, with a custom deleter that invokes
     xercesc::XMLString::release() on the string.  See also
     TranscodeToString(). */
  inline std::unique_ptr<const char, void (*)(const char *)>
  GetTranscoded(const XMLCh *xml_string) {
    return MakeXmlStringUniquePtr<const char>(
        xercesc::XMLString::transcode(xml_string));
  }

  /* Same as above, but transcodes from a 'const char *' representation in the
     native code page to 'const XMLCh *'. */
  inline std::unique_ptr<const XMLCh, void (*)(const XMLCh *)>
  GetTranscoded(const char *str) {
    return MakeXmlStringUniquePtr<const XMLCh>(
        xercesc::XMLString::transcode(str));
  }

  /* Transcode a string from 'const XMLCh *' to a 'const char *' representation
     in the native code page, and return the result as a std::string.  This is
     the same as GetTranscoded(const XMLCh *xml_string) except that it returns
     the result as a std::string.  The advantage of this method is that the
     form of the result is more convenient to work with.  The disadvantage is
     that it is a bit less efficient, since the implementation is built on top
     of GetTranscoded(const XMLCh *xml_string), and requires an extra memory
     allocation and freeing. */
  std::string TranscodeToString(const XMLCh *xml_string);

}  // Xml
