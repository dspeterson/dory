/* <dory/batch/single_topic_batcher.cc>

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

   Implements <dory/batch/single_topic_batcher.h>.
 */

#include <dory/batch/single_topic_batcher.h>

#include <base/no_default_case.h>

using namespace Dory;
using namespace Dory::Batch;

std::list<TMsg::TPtr>
TSingleTopicBatcher::DoAddMsg(TMsg::TPtr &&msg, TMsg::TTimestamp now) {
  assert(msg);
  std::list<TMsg::TPtr> result;

  if (!BatchingIsEnabled()) {
    return result;
  }

  switch (CoreState.ProcessNewMsg(now, msg)) {
    case TBatcherCore::TAction::LeaveMsgAndReturnBatch: {
      break;
    }
    case TBatcherCore::TAction::ReturnBatchAndTakeMsg: {
      result = std::move(MsgList);
      MsgList.push_back(std::move(msg));
      return result;
    }
    case TBatcherCore::TAction::TakeMsgAndReturnBatch: {
      MsgList.push_back(std::move(msg));
      break;
    }
    case TBatcherCore::TAction::TakeMsgAndLeaveBatch: {
      MsgList.push_back(std::move(msg));
      return result;
    }
    NO_DEFAULT_CASE;
  }

  result = std::move(MsgList);
  return result;
}
