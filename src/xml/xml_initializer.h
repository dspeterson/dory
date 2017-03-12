/* <xml/xml_initializer.h>

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

   Convenience class for handling initialization and cleanup for Xerces XML
   processing library.  Provides RAII cleanup behavior.  Some of the stuff in
   here may be a bit of overkill, but what the hell...
 */

#pragma once

#include <cassert>

#include <xercesc/util/XMLException.hpp>

#include <base/no_copy_semantics.h>

namespace Xml {

  class TXmlInitializer {
    NO_COPY_SEMANTICS(TXmlInitializer);

    public:
    /* The destructor handles library deinitialization. */
    virtual ~TXmlInitializer() noexcept;

    /* Call this method to manually initialize Xerces (necessary only if the
       constructor was told _not_ to call this method).  On success, return
       true.  On error, the behavior is determined by HandleInitError() below.
       If HandleInitError() throws, this method will propagate the exception.
       Otherwise it returns false. */
    bool Init();

    /* Call this method to deinitialize Xerces.  If you don't call this method,
       the destructor will.  It's questionable whether deinitialization does
       anything useful, but the developers of Xerces provided a method for it,
       so we may as well call it.  On error, HandleCleanupError() or
       HandleUnknownErrorOnCleanup() below is called. */
    bool Cleanup() noexcept;

    /* Return true if we have successfully initialized Xerces, or false
       othewise. */
    bool IsInitialized() const noexcept {
      assert(this);
      return Initialized;
    }

    protected:
    /* If you pass false for 'init_on_construction', you must call Init()
       yourself. */
    explicit TXmlInitializer(bool init_on_construction = true);

    /* Override this to handle an error initializing the library.  You can
       optionally do whatever error reporting you want here, and then choose
       one of the following options:

         1.  Return true to rethrow the passed in exception.
         2.  Return false to avoid throwing any exception.
         3.  Throw some other type of exception. */
    virtual bool HandleInitError(const xercesc::XMLException &x) = 0;

    /* Override this to handle an error cleaning up the library.  You can do
       whatever error reporting you want, but must not let any exceptions
       escape. */
    virtual void HandleCleanupError(
        const xercesc::XMLException &x) noexcept = 0;

    /* Called if the library throws some weird undocumented exception on
       cleanup.  You can add logging here. */
    virtual void HandleUnknownErrorOnCleanup() noexcept = 0;

    private:
    /* true if initialization has been successfully performed, or false
       otherwise. */
    bool Initialized;
  };  // TXmlInitializer

}  // Xml
