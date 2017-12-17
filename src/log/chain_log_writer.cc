/* <log/chain_log_writer.cc>

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

   Implements <log/chain_log_writer.h>.
 */

#include <log/chain_log_writer.h>

#include <utility>

using namespace Log;

TChainLogWriter::TChainLogWriter()
    : Chain(new TItemList) {
}

void TChainLogWriter::WriteEntry(TLogEntryAccessApi &entry) {
  assert(this);
  bool got_error = false;
  auto item_list_ptr = GetChain();
  const TItemList &item_list = *item_list_ptr;

  for (const TItemPtr &item_ptr : item_list) {
    try {
      item_ptr->WriteEntry(entry);
    } catch (const std::runtime_error &) {
      /* Don't let an error at one location prevent us from processing the rest
         of the chain.  Just remember that we got an error, so we can report it
         when we are done. */
      got_error = true;
    }
  }

  if (got_error) {
    throw TLogWriteError();
  }
}

void TChainLogWriter::PushFront(const TItemPtr &item) {
  assert(this);
  std::unique_ptr<TItemList> new_item_list(GetChainCopy());
  new_item_list->push_front(item);
  SetChain(std::shared_ptr<const TItemList>(new_item_list.release()));
}

void TChainLogWriter::PushBack(const TItemPtr &item) {
  assert(this);
  std::unique_ptr<TItemList> new_item_list(GetChainCopy());
  new_item_list->push_back(item);
  SetChain(std::shared_ptr<const TItemList>(new_item_list.release()));
}

TChainLogWriter::TItemPtr TChainLogWriter::PopFront() {
  assert(this);
  TItemPtr result;
  std::unique_ptr<TItemList> new_item_list(GetChainCopy());

  if (!new_item_list->empty()) {
    result = std::move(new_item_list->front());
    new_item_list->pop_front();
    SetChain(std::shared_ptr<const TItemList>(new_item_list.release()));
  }

  return std::move(result);
}

TChainLogWriter::TItemPtr TChainLogWriter::PopBack() {
  assert(this);
  TItemPtr result;
  std::unique_ptr<TItemList> new_item_list(GetChainCopy());

  if (!new_item_list->empty()) {
    result = std::move(new_item_list->back());
    new_item_list->pop_back();
    SetChain(std::shared_ptr<const TItemList>(new_item_list.release()));
  }

  return std::move(result);
}
