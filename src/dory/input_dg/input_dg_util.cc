/* <dory/input_dg/input_dg_util.cc>

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

   Implements <dory/input_dg/input_dg_util.h>.
 */

#include <dory/input_dg/input_dg_util.h>

#include <cassert>
#include <cstdint>

#include <base/counter.h>
#include <base/field_access.h>
#include <dory/input_dg/any_partition/any_partition_util.h>
#include <dory/input_dg/input_dg_common.h>
#include <dory/input_dg/input_dg_constants.h>
#include <dory/input_dg/partition_key/partition_key_util.h>
#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::InputDg;
using namespace Dory::InputDg::AnyPartition;
using namespace Dory::InputDg::PartitionKey;
using namespace Log;

DEFINE_COUNTER(InputAgentDiscardMsgUnsupportedApiKey);

TMsg::TPtr Dory::InputDg::BuildMsgFromDg(const void *dg, size_t dg_size,
    const TCmdLineArgs &args, Capped::TPool &pool,
    TAnomalyTracker &anomaly_tracker, TMsgStateTracker &msg_state_tracker) {
  assert(dg);
  const auto *dg_bytes = reinterpret_cast<const uint8_t *>(dg);
  size_t fixed_part_size = INPUT_DG_SZ_FIELD_SIZE +
      INPUT_DG_API_KEY_FIELD_SIZE + INPUT_DG_API_VERSION_FIELD_SIZE;

  if (dg_size < fixed_part_size) {
    DiscardMalformedMsg(dg_bytes, dg_size, anomaly_tracker, args.NoLogDiscard);
    return TMsg::TPtr();
  }

  int32_t sz = ReadInt32FromHeader(dg_bytes);

  if ((sz < 0) || (static_cast<size_t>(sz) != dg_size)) {
    DiscardMalformedMsg(dg_bytes, dg_size, anomaly_tracker, args.NoLogDiscard);
    return TMsg::TPtr();
  }

  int16_t api_key = ReadInt16FromHeader(dg_bytes + INPUT_DG_SZ_FIELD_SIZE);
  size_t key_part_size = INPUT_DG_SZ_FIELD_SIZE + INPUT_DG_API_KEY_FIELD_SIZE;
  int16_t api_version = ReadInt16FromHeader(dg_bytes + key_part_size);
  const uint8_t *versioned_part_begin = &dg_bytes[fixed_part_size];
  const uint8_t *versioned_part_end = versioned_part_begin +
      (dg_size - fixed_part_size);

  switch (api_key) {
    case 256: {
      return BuildAnyPartitionMsgFromDg(dg_bytes, dg_size, api_version,
          versioned_part_begin, versioned_part_end, pool, anomaly_tracker,
          msg_state_tracker, args.NoLogDiscard);
    }
    case 257: {
      return BuildPartitionKeyMsgFromDg(dg_bytes, dg_size, api_version,
          versioned_part_begin, versioned_part_end, pool, anomaly_tracker,
          msg_state_tracker, args.NoLogDiscard);
    }
    default: {
      break;
    }
  }

  if (!args.NoLogDiscard) {
    LOG_R(TPri::ERR, std::chrono::seconds(30)) <<
        "Discarding message with unsupported API key: " << api_key;
  }

  anomaly_tracker.TrackUnsupportedApiKeyDiscard(dg_bytes, dg_bytes + dg_size,
      api_key);
  InputAgentDiscardMsgUnsupportedApiKey.Increment();
  return TMsg::TPtr();
}
