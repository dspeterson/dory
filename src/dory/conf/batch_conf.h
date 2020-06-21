/* <dory/conf/batch_conf.h>

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

   Class representing batching configuration obtained from Dory's config file.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <base/opt.h>
#include <dory/conf/conf_error.h>

namespace Dory {

  namespace Conf {

    class TBatchDuplicateNamedConfig final : public TConfError {
      public:
      explicit TBatchDuplicateNamedConfig(const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TBatchDuplicateNamedConfig

    class TBatchDuplicateProduceRequestDataLimit final : public TConfError {
      public:
      TBatchDuplicateProduceRequestDataLimit()
          : TConfError(
                "Batching config contains duplicate produceRequestDataLimit "
                "definition") {
      }
    };  // TBatchDuplicateProduceRequestDataLimit

    class TBatchDuplicateMessageMaxBytes final : public TConfError {
      public:
      TBatchDuplicateMessageMaxBytes()
          : TConfError(
                "Batching config contains duplicate messageMaxBytes "
                "definition") {
      }
    };  // TBatchDuplicateMessageMaxBytes

    class TBatchDuplicateCombinedTopicsConfig final : public TConfError {
      public:
      TBatchDuplicateCombinedTopicsConfig()
          : TConfError(
                "Batching config contains duplicate combinedTopics "
                "definition") {
      }
    };  // TBatchDuplicateCombinedTopicsConfig

    class TBatchUnknownCombinedTopicsConfig final : public TConfError {
      public:
      explicit TBatchUnknownCombinedTopicsConfig(
          const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TBatchUnknownCombinedTopicsConfig

    class TBatchDuplicateDefaultTopicConfig final : public TConfError {
      public:
      TBatchDuplicateDefaultTopicConfig()
          : TConfError(
                "Batching config contains duplicate defaultTopic "
                "definition") {
      }
    };  // TBatchDuplicateDefaultTopicConfig

    class TBatchUnknownDefaultTopicConfig final : public TConfError {
      public:
      explicit TBatchUnknownDefaultTopicConfig(const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TBatchUnknownDefaultTopicConfig

    class TBatchDuplicateTopicConfig final : public TConfError {
      public:
      explicit TBatchDuplicateTopicConfig(const std::string &topic)
          : TConfError(CreateMsg(topic)) {
      }

      private:
      static std::string CreateMsg(const std::string &topic);
    };  // TBatchDuplicateTopicConfig

    class TBatchUnknownTopicConfig final : public TConfError {
      public:
      TBatchUnknownTopicConfig(const std::string &topic,
          const std::string &config_name)
          : TConfError(CreateMsg(topic, config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &topic,
          const std::string &config_name);
    };  // TBatchUnknownTopicConfig

    class TBatchMissingProduceRequestDataLimit final : public TConfError {
      public:
      TBatchMissingProduceRequestDataLimit()
          : TConfError(
                "Batching config is missing produceRequestDataLimit "
                "definition") {
      }
    };  // TBatchMissingProduceRequestDataLimit

    class TBatchMissingMessageMaxBytes final : public TConfError {
      public:
      TBatchMissingMessageMaxBytes()
          : TConfError(
                "Batching config is missing messageMaxBytes definition") {
      }
    };  // TBatchMissingMessageMaxBytes

    class TBatchMissingCombinedTopics final : public TConfError {
      public:
      TBatchMissingCombinedTopics()
          : TConfError(
                "Batching config is missing combinedTopics definition") {
      }
    };  // TBatchMissingCombinedTopics

    class TBatchMissingDefaultTopic final : public TConfError {
      public:
      TBatchMissingDefaultTopic()
          : TConfError(
                "Batching config is missing defaultTopic definition") {
      }
    };  // TBatchMissingDefaultTopic

    struct TBatchConf final {
      class TBuilder;

      enum class TTopicAction {
        PerTopic,
        CombinedTopics,
        Disable
      };  // TTopicAction

      /* For each member 'm' below, m.IsUnknown() indicates that "disable" was
         specified in the config file. */
      struct TBatchValues final {
        Base::TOpt<size_t> OptTimeLimit;

        Base::TOpt<size_t> OptMsgCount;

        Base::TOpt<size_t> OptByteCount;
      };  // TBatchValues

      struct TTopicConf final {
        TTopicAction Action;

        TBatchValues BatchValues;

        TTopicConf() noexcept
            : Action(TTopicAction::Disable) {
        }

        TTopicConf(TTopicAction action,
            const TBatchValues &batch_values) noexcept
            : Action(action),
              BatchValues(batch_values) {
        }

        TTopicConf(const TTopicConf &) = default;

        TTopicConf(TTopicConf &&) = default;

        TTopicConf &operator=(const TTopicConf &) = default;

        TTopicConf &operator=(TTopicConf &&) = default;
      };  // TTopicConf

      using TTopicMap = std::unordered_map<std::string, TTopicConf>;

      static bool StringToTopicAction(const char *s,
          TTopicAction &result) noexcept;

      static bool StringToTopicAction(const std::string &s,
          TTopicAction &result) noexcept {
        return StringToTopicAction(s.c_str(), result);
      }

      size_t ProduceRequestDataLimit = 0;

      size_t MessageMaxBytes = 0;

      bool CombinedTopicsBatchingEnabled = false;

      TBatchValues CombinedTopicsConfig;

      TTopicAction DefaultTopicAction = TTopicAction::Disable;

      TBatchValues DefaultTopicConfig;

      TTopicMap TopicConfigs;
    };  // TBatchConf

    class TBatchConf::TBuilder final {
      public:
      TBuilder() = default;

      void Reset() {
        *this = TBuilder();
      }

      void AddNamedConfig(const std::string &name, const TBatchValues &values);

      /* A value of 0 for 'limit' means "disable batch combining". */
      void SetProduceRequestDataLimit(size_t limit);

      void SetMessageMaxBytes(size_t message_max_bytes);

      void SetCombinedTopicsConfig(bool enabled,
          const std::string *config_name);

      void SetDefaultTopicConfig(TTopicAction action,
          const std::string *config_name);

      void SetTopicConfig(const std::string &topic, TTopicAction action,
          const std::string *config_name);

      TBatchConf Build();

      private:
      std::unordered_map<std::string, TBatchValues> NamedConfigs;

      TBatchConf BuildResult;

      bool GotProduceRequestDataLimit = false;

      bool GotMessageMaxBytes = false;

      bool GotCombinedTopics = false;

      bool GotDefaultTopic = false;
    };  // TBatchConf::TBuilder

  }  // Conf

}  // Dory
