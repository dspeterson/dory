/* <dory/batch/batcher_core.h>

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

   Implementation class for core batching logic.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <limits>

#include <base/opt.h>
#include <dory/batch/batch_config.h>
#include <dory/msg.h>

namespace Dory {

  namespace Batch {

    class TBatcherCore final {
      public:
      enum class TAction {
        LeaveMsgAndReturnBatch,
        ReturnBatchAndTakeMsg,
        TakeMsgAndReturnBatch,
        TakeMsgAndLeaveBatch
      };  // TAction

      TBatcherCore() noexcept = default;

      explicit TBatcherCore(const TBatchConfig &config) noexcept
          : Config(config) {
      }

      TBatcherCore(const TBatcherCore &) noexcept = default;

      TBatcherCore &operator=(const TBatcherCore &) noexcept = default;

      bool BatchingIsEnabled() const noexcept {
        assert(this);
        return Dory::Batch::BatchingIsEnabled(Config);
      }

      const TBatchConfig &GetConfig() const noexcept {
        assert(this);
        return Config;
      }

      bool IsEmpty() const noexcept {
        assert(this);
        return (MsgCount == 0);
      }

      size_t GetMsgCount() const noexcept {
        assert(this);
        return MsgCount;
      }

      size_t GetByteCount() const noexcept {
        assert(this);
        return ByteCount;
      }

      Base::TOpt<TMsg::TTimestamp> GetNextCompleteTime() const noexcept;

      TAction ProcessNewMsg(TMsg::TTimestamp now,
          const TMsg::TPtr &msg) noexcept;

      void ClearState() noexcept;

      private:
      bool TestTimeLimit(TMsg::TTimestamp now,
          TMsg::TTimestamp new_msg_timestamp =
              std::numeric_limits<TMsg::TTimestamp>::max()) const noexcept;

      bool TestMsgCount(bool adding_msg = false) const noexcept;

      bool TestByteCount(size_t bytes_to_add = 0) const noexcept;

      bool TestByteCountExceeded(size_t bytes_to_add) const noexcept;

      bool TestAllLimits(TMsg::TTimestamp now) const noexcept {
        assert(this);
        return TestTimeLimit(now) || TestMsgCount() || TestByteCount();
      }

      void UpdateState(TMsg::TTimestamp timestamp, size_t body_size) noexcept;

      TBatchConfig Config;

      TMsg::TTimestamp MinTimestamp =
          std::numeric_limits<TMsg::TTimestamp>::max();

      size_t MsgCount = 0;

      size_t ByteCount = 0;
    };  // TBatcherCore

  }  // Batch

}  // Dory
