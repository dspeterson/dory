/* <dory/util/msg_util.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Implements <dory/util/msg_util.h>.
 */

#include <dory/util/msg_util.h>

#include <algorithm>
#include <cassert>

#include <capped/blob.h>

using namespace Capped;
using namespace Dory;
using namespace Dory::Util;

size_t Dory::Util::GetDataSize(const std::list<TMsg::TPtr> &batch) {
  size_t total_size = 0;

  for (const TMsg::TPtr &msg_ptr : batch) {
    assert(msg_ptr);
    total_size += msg_ptr->GetKeyAndValue().Size();
  }

  return total_size;
}

void Dory::Util::WriteKey(std::vector<uint8_t> &dst, size_t offset,
    const TMsg &msg) {
  size_t key_size = msg.GetKeySize();
  dst.resize(std::max(dst.size(), offset + key_size));

  /* Copy the key into the buffer. */
  if (key_size) {
    TReader reader(&msg.GetKeyAndValue());
    reader.Read(&dst[offset], key_size);
  }
}

/* This function becomes trivially simple when we get rid of the old output
   format. */
size_t Dory::Util::WriteValue(std::vector<uint8_t> &dst, size_t offset,
    const TMsg &msg) {
  size_t value_size = msg.GetValueSize();
  dst.resize(std::max(dst.size(), offset + value_size));

  if (value_size) {
    /* Copy the value into the buffer. */
    TReader reader(&msg.GetKeyAndValue());
    reader.Skip(msg.GetKeySize());
    reader.Read(&dst[offset], value_size);
  }

  return value_size;
}

void Dory::Util::WriteValue(uint8_t *dst, const TMsg &msg) {
  /* Copy the value into the buffer. */
  TReader reader(&msg.GetKeyAndValue());
  reader.Skip(msg.GetKeySize());
  reader.Read(dst, msg.GetValueSize());
}
