/* <dory/batch/global_batch_config.h>

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

   Global batching configuration class.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

#include <dory/batch/combined_topics_batcher.h>
#include <dory/batch/per_topic_batcher.h>

namespace Dory {

  namespace Batch {

    class TGlobalBatchConfig final {
      public:
      TGlobalBatchConfig() = default;

      TGlobalBatchConfig(
          std::shared_ptr<TPerTopicBatcher::TConfig> &&per_topic_config,
          TCombinedTopicsBatcher::TConfig &&combined_topics_config,
          size_t produce_request_data_limit, size_t message_max_bytes)
          : PerTopicConfig(std::move(per_topic_config)),
            CombinedTopicsConfig(std::move(combined_topics_config)),
            ProduceRequestDataLimit(produce_request_data_limit),
            MessageMaxBytes(message_max_bytes) {
      }

      TGlobalBatchConfig(const TGlobalBatchConfig &) = default;

      TGlobalBatchConfig(TGlobalBatchConfig &&) = default;

      TGlobalBatchConfig &operator=(const TGlobalBatchConfig &) = default;

      TGlobalBatchConfig &operator=(TGlobalBatchConfig &&) = default;

      void Clear() {
        *this = TGlobalBatchConfig();
      }

      const std::shared_ptr<TPerTopicBatcher::TConfig> &
      GetPerTopicConfig() const noexcept {
        return PerTopicConfig;
      }

      const TCombinedTopicsBatcher::TConfig &
      GetCombinedTopicsConfig() const noexcept {
        return CombinedTopicsConfig;
      }

      size_t GetProduceRequestDataLimit() const noexcept {
        return ProduceRequestDataLimit;
      }

      size_t GetMessageMaxBytes() const noexcept {
        return MessageMaxBytes;
      }

      private:
      std::shared_ptr<TPerTopicBatcher::TConfig> PerTopicConfig;

      TCombinedTopicsBatcher::TConfig CombinedTopicsConfig;

      size_t ProduceRequestDataLimit = 0;

      size_t MessageMaxBytes = 0;
    };  // TGlobalBatchConfig

  }  // Batch

}  // Dory
