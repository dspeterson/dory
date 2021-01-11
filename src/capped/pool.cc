/* <capped/pool.cc>

   ----------------------------------------------------------------------------
   Copyright 2013 if(we)

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

   Implements <capped/pool.h>.
 */

#include <capped/pool.h>

#include <algorithm>
#include <cassert>
#include <new>
#include <optional>

#include <base/error_util.h>

using namespace Base;
using namespace Capped;

TPool::TPool(size_t block_size, size_t block_count, TSync sync_policy)
    : BlockSize(std::max(block_size, sizeof(TBlock))),
      BlockCount(block_count),
      Guarded(sync_policy != TSync::Unguarded) {
  /* Allocate enough storage space for all our blocks. */
  size_t size = BlockSize * BlockCount;
  Storage = new char[size];

  /* Walk across the storage space, forming a linked list of free blocks. */
  for (char *ptr = Storage; ptr < Storage + size; ptr += BlockSize) {
    new (ptr) TBlock(FirstFreeBlock);
  }
}

TPool::~TPool() {
  delete [] Storage;
}

void *TPool::Alloc() {
  std::optional<std::lock_guard<std::mutex>> opt_lock;

  if (Guarded) {
    opt_lock.emplace(Mutex);
  }

  auto *result = FirstFreeBlock;

  if (!result) {
    throw TMemoryCapReached();
  }

  return TBlock::Unlink(FirstFreeBlock);
}

TPool::TBlock *TPool::AllocList(size_t block_count) {
  TBlock *first_block = nullptr;

  if (block_count) {
    std::optional<std::lock_guard<std::mutex>> opt_lock;

    if (Guarded) {
      opt_lock.emplace(Mutex);
    }

    for (; block_count; --block_count) {
      if (!FirstFreeBlock) {
        if (first_block) {
          DoFreeList(first_block);
        }

        throw TMemoryCapReached();
      }

      TBlock::Unlink(FirstFreeBlock)->Link(first_block);
    }
  }

  return first_block;
}

void TPool::Free(void *ptr) noexcept {
  if (ptr) {
    std::optional<std::lock_guard<std::mutex>> opt_lock;

    if (Guarded) {
      opt_lock.emplace(Mutex);
    }

    DoFree(ptr);
  }
}

void TPool::FreeList(TBlock *first_block) noexcept {
  if (first_block == nullptr) {
    return;
  }

  std::optional<std::lock_guard<std::mutex>> opt_lock;

  if (Guarded) {
    opt_lock.emplace(Mutex);
  }

  DoFreeList(first_block);
}

void TPool::DoFree(void *ptr) noexcept {
  assert(ptr);
  assert(Storage <= ptr);
  assert(ptr < Storage + BlockSize * BlockCount);
  new (ptr) TBlock(FirstFreeBlock);
}

void TPool::DoFreeList(TBlock *first_block) noexcept {
  assert(first_block);

  do {
    TBlock *block = TBlock::Unlink(first_block);
    assert(block);
    DoFree(block);
  } while (first_block);
}
