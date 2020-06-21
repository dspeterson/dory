/* <dory/conf/topic_rate_conf.h>

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

   Class representing per-topic message rate limiting configuration obtained
   from Dory's config file.
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

    class TTopicRateDuplicateNamedConfig final : public TConfError {
      public:
      explicit TTopicRateDuplicateNamedConfig(const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TTopicRateDuplicateNamedConfig

    class TTopicRateZeroRateLimitInterval final : public TConfError {
      public:
      explicit TTopicRateZeroRateLimitInterval(const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TTopicRateZeroRateLimitInterval

    class TTopicRateDuplicateDefaultTopicConfig final : public TConfError {
      public:
      TTopicRateDuplicateDefaultTopicConfig()
          : TConfError(
                "Topic rate limiting config contains duplicate defaultTopic "
                "definition") {
      }
    };  // TTopicRateDuplicateDefaultTopicConfig

    class TTopicRateUnknownDefaultTopicConfig final : public TConfError {
      public:
      explicit TTopicRateUnknownDefaultTopicConfig(
          const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TTopicRateUnknownDefaultTopicConfig

    class TTopicRateDuplicateTopicConfig final : public TConfError {
      public:
      explicit TTopicRateDuplicateTopicConfig(const std::string &topic)
          : TConfError(CreateMsg(topic)) {
      }

      private:
      static std::string CreateMsg(const std::string &topic);
    };  // TTopicRateDuplicateTopicConfig

    class TTopicRateUnknownTopicConfig final : public TConfError {
      public:
      TTopicRateUnknownTopicConfig(const std::string &topic,
          const std::string &config_name)
          : TConfError(CreateMsg(topic, config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &topic,
          const std::string &config_name);
    };  // TTopicRateUnknownTopicConfig

    class TTopicRateMissingDefaultTopic final : public TConfError {
      public:
      TTopicRateMissingDefaultTopic()
          : TConfError(
                "Topic rate limiting config is missing defaultTopic "
                "definition") {
      }
    };  // TTopicRateMissingDefaultTopic

    struct TTopicRateConf final {
      class TBuilder;

      struct TConf final {
        /* This number must be > 0.  It specifies a time interval length in
           milliseconds for rate limit enforcement. */
        size_t Interval = 1;

        /* Optional maximum # of allowed messages for a given topic within
           'Interval' above.  Messages that would cause the maximum to be
           exceeded are discarded.  If the optional value is in the unknown
           state then this indicates no maximum (i.e. infinite limit). */
        Base::TOpt<size_t> MaxCount;

        /* Default constructor specifies no limit. */
        TConf() noexcept = default;

        TConf(size_t interval, size_t max_count) noexcept
            : Interval(interval),
              MaxCount(max_count) {
        }

        TConf(const TConf &) = default;

        TConf(TConf &&) = default;

        TConf &operator=(const TConf &) = default;

        TConf &operator=(TConf &&) = default;
      };  // TConf

      using TTopicMap = std::unordered_map<std::string, TConf>;

      TConf DefaultTopicConfig;

      TTopicMap TopicConfigs;
    };  // TTopicRateConf

    class TTopicRateConf::TBuilder final {
      public:
      TBuilder() = default;

      void Reset() {
        *this = TBuilder();
      }

      /* Add a named config with a finite maximum count. */
      void AddBoundedNamedConfig(const std::string &name, size_t interval,
          size_t max_count);

      /* Add a named config with an unlimited maximum count. */
      void AddUnlimitedNamedConfig(const std::string &name);

      void SetDefaultTopicConfig(const std::string &config_name);

      void SetTopicConfig(const std::string &topic,
          const std::string &config_name);

      TTopicRateConf Build();

      private:
      std::unordered_map<std::string, TConf> NamedConfigs;

      TTopicRateConf BuildResult;

      bool GotDefaultTopic = false;
    };  // TTopicRateConf::TBuilder

  }  // Conf

}  // Dory
