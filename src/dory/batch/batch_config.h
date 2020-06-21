/* <dory/batch/batch_config.h>

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

   Batching configuration class.
 */

#pragma once

#include <cstddef>

namespace Dory {

  namespace Batch {

    struct TBatchConfig {
      TBatchConfig() noexcept = default;

      TBatchConfig(size_t time_limit, size_t msg_count,
          size_t byte_count) noexcept
          : TimeLimit(time_limit),
            MsgCount(msg_count),
            ByteCount(byte_count) {
      }

      TBatchConfig(const TBatchConfig &) noexcept = default;

      TBatchConfig& operator=(const TBatchConfig &) noexcept = default;

      void Clear() noexcept {
        *this = TBatchConfig();
      }

      size_t TimeLimit = 0;  // milliseconds

      size_t MsgCount = 0;

      size_t ByteCount = 0;
    };  // TBatchConfig

    inline bool BatchingIsEnabled(const TBatchConfig &config) noexcept {
      return config.TimeLimit || config.MsgCount || config.ByteCount;
    }

    inline bool TimeLimitIsEnabled(const TBatchConfig &config) noexcept {
      return (config.TimeLimit != 0);
    }

    inline bool MsgCountLimitIsEnabled(const TBatchConfig &config) noexcept {
      return (config.MsgCount != 0);
    }

    inline bool ByteCountLimitIsEnabled(const TBatchConfig &config) noexcept {
      return (config.ByteCount != 0);
    }

  }  // Batch

}  // Dory
