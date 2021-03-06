/* <dory/input_dg/partition_key/partition_key_util.cc>

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

   Implements <dory/input_dg/partition_key/partition_key_util.h>.
 */

#include <cassert>

#include <base/counter.h>
#include <dory/input_dg/partition_key/partition_key_util.h>
#include <dory/input_dg/partition_key/v0/v0_input_dg_reader.h>
#include <log/log.h>

using namespace Capped;
using namespace Dory;
using namespace Dory::InputDg;
using namespace Dory::InputDg::PartitionKey;
using namespace Log;

DEFINE_COUNTER(InputAgentDiscardPartitionKeyMsgUnsupportedApiVersion);
DEFINE_COUNTER(InputAgentProcessPartitionKeyMsg);

TMsg::TPtr Dory::InputDg::PartitionKey::BuildPartitionKeyMsgFromDg(
    const uint8_t *dg_bytes, size_t dg_size, int16_t api_version,
    const uint8_t *versioned_part_begin, const uint8_t *versioned_part_end,
    TPool &pool, TAnomalyTracker &anomaly_tracker,
    TMsgStateTracker &msg_state_tracker, bool log_discard) {
  assert(dg_bytes);
  assert(versioned_part_begin > dg_bytes);
  assert(versioned_part_end > versioned_part_begin);
  InputAgentProcessPartitionKeyMsg.Increment();

  switch (api_version) {
    case 0: {
      return V0::TV0InputDgReader(dg_bytes, versioned_part_begin,
          versioned_part_end, pool, anomaly_tracker,
          msg_state_tracker, log_discard).BuildMsg();
    }
    default: {
      break;
    }
  }

  anomaly_tracker.TrackUnsupportedMsgVersionDiscard(dg_bytes,
      dg_bytes + dg_size, api_version);
  InputAgentDiscardPartitionKeyMsgUnsupportedApiVersion.Increment();

  if (log_discard) {
    LOG_R(TPri::ERR, std::chrono::seconds(30))
        << "Discarding PartitionKey message with unsupported API version: "
        << api_version;
  }

  return TMsg::TPtr();
}
