/* <dory/batch/per_topic_batcher.h>

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

   Per-topic message batcher class.
 */

#pragma once

#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <dory/batch/batch_config.h>
#include <dory/batch/single_topic_batcher.h>
#include <dory/msg.h>

namespace Dory {

  namespace Batch {

    class TPerTopicBatcher final {
      NO_COPY_SEMANTICS(TPerTopicBatcher);

      public:
      class TConfig final {
        public:
        TConfig(const TBatchConfig &default_topic,
            std::unordered_map<std::string, TBatchConfig> &&per_topic)
            : DefaultTopic(default_topic),
              PerTopic(std::move(per_topic)) {
        }

        TConfig(const TConfig &) = default;

        TConfig(TConfig &&) = default;

        TConfig& operator=(const TConfig &) = default;

        TConfig& operator=(TConfig &&) = default;

        const TBatchConfig &Get(const std::string &topic) const noexcept {
          auto iter = PerTopic.find(topic);
          return (iter == PerTopic.end()) ? DefaultTopic : iter->second;
        }

        private:
        TBatchConfig DefaultTopic;

        std::unordered_map<std::string, TBatchConfig> PerTopic;
      };  // TConfig

      explicit TPerTopicBatcher(const std::shared_ptr<TConfig> &config);

      explicit TPerTopicBatcher(std::shared_ptr<TConfig> &&config);

      /* TODO: Eliminate need for clients to call this method. */
      bool IsEnabled() const noexcept {
        return bool(Config);
      }

      const std::shared_ptr<TConfig> &GetConfig() const noexcept {
        return Config;
      }

      std::list<std::list<TMsg::TPtr>>
      AddMsg(TMsg::TPtr &&msg, TMsg::TTimestamp now);

      /* The behavior here is the same as for AddMsg() except that the caller
         has no message to batch. */
      std::list<std::list<TMsg::TPtr>>
      GetCompleteBatches(TMsg::TTimestamp now);

      Base::TOpt<TMsg::TTimestamp> GetNextCompleteTime() const noexcept;

      /* Get all batches, even incomplete ones.  On return, the batcher will
         have no messages.  This is used when dory is shutting down. */
      std::list<std::list<TMsg::TPtr>> GetAllBatches();

      /* Delete all batch state for the given topic and return a list of all
         messages that were batched for that topic. */
      std::list<TMsg::TPtr> DeleteTopic(const std::string &topic);

      /* For testing. */
      bool SanityCheck() const;

      private:
      /* We store these in a multiset.  Each record represents a topic batch
         with a time limit.  The records are ordered by ascending expiry time,
         regardless of topic.  In other words, two records with the same
         timestamp but different topics are considered equal from the
         multiset's point of view.  The multiset contains at most one record
         for a given topic.  No record for a given topic appears in the
         multiset in the case where the batch for that topic is empty or has no
         time limit. */
      class TBatchExpiryRecord final {
        public:
        TBatchExpiryRecord(TMsg::TTimestamp expiry, const std::string &topic)
            : Expiry(expiry),
              Topic(topic) {
        }

        TBatchExpiryRecord(TMsg::TTimestamp expiry, std::string &&topic)
            : Expiry(expiry),
              Topic(std::move(topic)) {
        }

        TBatchExpiryRecord(TBatchExpiryRecord &&that) noexcept
            : Expiry(that.Expiry),
              Topic(std::move(that.Topic)) {
        }

        TBatchExpiryRecord &operator=(TBatchExpiryRecord &&that) noexcept {
          if (this != &that) {
            Expiry = that.Expiry;
            Topic = std::move(that.Topic);
          }

          return *this;
        }

        bool operator<(const TBatchExpiryRecord &that) const noexcept {
          return (Expiry < that.Expiry);
        }

        TMsg::TTimestamp GetExpiry() const noexcept {
          return Expiry;
        }

        const std::string &GetTopic() const noexcept {
          return Topic;
        }

        private:
        /* Batch expiry time. */
        TMsg::TTimestamp Expiry;

        /* Batch topic. */
        std::string Topic;
      };  // TBatchExpiryRecord

      using TExpiryRef = std::multiset<TBatchExpiryRecord>::const_iterator;

      struct TBatchMapEntry {
        /* A batch for a single topic. */
        TSingleTopicBatcher Batcher;

        /* If the batch is nonempty and has a time limit, then this references
           the corresponding entry in 'ExpiryTracker' below.  Otherwise this is
           set to ExpiryTracker.end(). */
        TExpiryRef ExpiryRef;

        TBatchMapEntry(const TBatchConfig &config, TExpiryRef expiry_ref)
            : Batcher(config),
              ExpiryRef(expiry_ref) {
        }

        TBatchMapEntry(TBatchMapEntry &&) = default;

        TBatchMapEntry &operator=(TBatchMapEntry &&) = default;
      };  // TBatchMapEntry

      /* Per-topic batching configuration obtained from a config file. */
      std::shared_ptr<TConfig> Config;

      /* Key is topic and value is batch of messages for topic. */
      std::unordered_map<std::string, TBatchMapEntry> BatchMap;

      /* This contains a record for each nonempty topic batch with a time
         limit.  It lets us efficiently determine the soonest time limit
         expiration. */
      std::multiset<TBatchExpiryRecord> ExpiryTracker;
    };  // TPerTopicBatcher

  }  // Batch

}  // Dory
