/* <log/chain_log_writer.h>

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

   A log writer that delegates to a list of lower level log writers.
 */

#pragma once

#include <cassert>
#include <list>
#include <memory>
#include <mutex>

#include <base/no_copy_semantics.h>
#include <log/log_writer_api.h>

namespace Log {

  class TChainLogWriter : public TLogWriterApi {
    NO_COPY_SEMANTICS(TChainLogWriter);

    public:
    using TItemPtr = std::shared_ptr<TLogWriterApi>;

    using TItemList = std::list<TItemPtr>;

    TChainLogWriter();

    ~TChainLogWriter() noexcept override = default;

    /* Write 'entry'. */
    void WriteEntry(TLogEntryAccessApi &entry) override;

    /* Get a reference to the current chain, which is immutable.  To make
       changes, you must create a new chain (possibly based on a copy of the
       current chain), and pass the new chain to SetChain().  Modifying the
       chain can be slow, since modifications are rare and the chain will be
       short in practice.  Traversal should be fast and as free from locking as
       possible, since this is the common case. */
    std::shared_ptr<const TItemList> GetChain() const {
      assert(this);
      std::lock_guard<std::mutex> lock(Lock);
      return Chain;
    }

    /* Return a private mutable copy of the current chain. */
    TItemList *GetChainCopy() const {
      assert(this);
      return new TItemList(*GetChain());
    }

    /* Replace the current chain with 'new_chain'. */
    void SetChain(const std::shared_ptr<const TItemList> &new_chain) {
      assert(this);
      std::lock_guard<std::mutex> lock(Lock);
      Chain = new_chain;
    }

    /* Convenience function for replacing the current chain with a new chain
       that has 'item' inserted at the front.

       Warning: It is assumed that only one thread at a time is modifying the
       chain.  Although concurrent modifications would not corrupt the chain,
       they might interfere with each other.  For instance:

           Threads t1 and t2 concurrently call PushFront(), with t1 trying to
           insert item1 and t2 trying to insert item2.  Each makes its own
           private copy of the chain, with the item it is trying to insert
           prepended.  t1 commits its change first, followed by t2.  t2's
           commit causes item1 to be lost. */
    void PushFront(const TItemPtr &item);

    /* Convenience function for replacing the current chain with a new chain
       that has 'item' inserted at the back.

       Warning: It is assumed that only one thread at a time is modifying the
       chain.  Although concurrent modifications would not corrupt the chain,
       they might interfere with each other.  For instance:

           Threads t1 and t2 concurrently call PushFront(), with t1 trying to
           insert item1 and t2 trying to insert item2.  Each makes its own
           private copy of the chain, with the item it is trying to insert
           prepended.  t1 commits its change first, followed by t2.  t2's
           commit causes item1 to be lost. */
    void PushBack(const TItemPtr &item);

    /* Convenience function that replaces the current chain with a new chain
       that has the front item removed.  Returns the former front item.  If
       chain was previously empty, returned TItemPtr will be empty.

       Warning: It is assumed that only one thread at a time is modifying the
       chain.  Although concurrent modifications would not corrupt the chain,
       they might interfere with each other.  For instance:

           Threads t1 and t2 concurrently call PushFront(), with t1 trying to
           insert item1 and t2 trying to insert item2.  Each makes its own
           private copy of the chain, with the item it is trying to insert
           prepended.  t1 commits its change first, followed by t2.  t2's
           commit causes item1 to be lost. */
    TItemPtr PopFront();

    /* Convenience function that replaces the current chain with a new chain
       that has the back item removed.  Returns the former back item.  If chain
       was previously empty, returned TItemPtr will be empty.

       Warning: It is assumed that only one thread at a time is modifying the
       chain.  Although concurrent modifications would not corrupt the chain,
       they might interfere with each other.  For instance:

           Threads t1 and t2 concurrently call PushFront(), with t1 trying to
           insert item1 and t2 trying to insert item2.  Each makes its own
           private copy of the chain, with the item it is trying to insert
           prepended.  t1 commits its change first, followed by t2.  t2's
           commit causes item1 to be lost. */
    TItemPtr PopBack();

    private:
    mutable std::mutex Lock;

    std::shared_ptr<const TItemList> Chain;
  };  // TChainLogWriter

}  // Log
