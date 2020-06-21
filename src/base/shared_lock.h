/* <base/shared_lock.h>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   An RAII object for holding a shared lock on an asset.
 */

#pragma once

#include <base/no_copy_semantics.h>

namespace Base {

  /* An RAII object for holding a shared lock on an asset. */
  template <typename TAsset>
  class TSharedLock {
    NO_COPY_SEMANTICS(TSharedLock);

    public:
    /* Will not return until the lock is granted. */
    TSharedLock(const TAsset &asset)
        : Asset(asset) {
      asset.AcquireShared();
    }

    /* Releases the lock. */
    ~TSharedLock() {
      Asset.ReleaseShared();
    }

    private:
    /* The asset we're locking. */
    const TAsset &Asset;
  };  // TSharedLock

}  // Base
