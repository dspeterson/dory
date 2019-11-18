/* <dory/conf/compression_conf.h>

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

   Class representing compression configuration obtained from Dory's config
   file.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <base/opt.h>
#include <dory/compress/compression_type.h>
#include <dory/conf/conf_error.h>

namespace Dory {

  namespace Conf {

    class TCompressionDuplicateNamedConfig final : public TConfError {
      public:
      explicit TCompressionDuplicateNamedConfig(const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TCompressionDuplicateNamedConfig

    class TCompressionDuplicateSizeThresholdPercent final : public TConfError {
      public:
      TCompressionDuplicateSizeThresholdPercent()
          : TConfError(
                "Compression config contains duplicate sizeThresholdPercent "
                "definition") {
      }
    };  // TCompressionDuplicateSizeThresholdPercent

    class TCompressionBadSizeThresholdPercent final : public TConfError {
      public:
      TCompressionBadSizeThresholdPercent()
          : TConfError(
                "Compression config contains bad sizeThresholdPercent value: "
                "must be <= 100") {
      }
    };  // TCompressionBadSizeThresholdPercent

    class TCompressionDuplicateDefaultTopicConfig final : public TConfError {
      public:
      TCompressionDuplicateDefaultTopicConfig()
          : TConfError(
                "Compression config contains duplicate defaultTopic "
                "definition") {
      }
    };  // TCompressionDuplicateDefaultTopicConfig

    class TCompressionUnknownDefaultTopicConfig final : public TConfError {
      public:
      explicit TCompressionUnknownDefaultTopicConfig(
          const std::string &config_name)
          : TConfError(CreateMsg(config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &config_name);
    };  // TCompressionUnknownDefaultTopicConfig

    class TCompressionDuplicateTopicConfig final : public TConfError {
      public:
      explicit TCompressionDuplicateTopicConfig(const std::string &topic)
          : TConfError(CreateMsg(topic)) {
      }

      private:
      static std::string CreateMsg(const std::string &topic);
    };  // TCompressionDuplicateTopicConfig

    class TCompressionUnknownTopicConfig final : public TConfError {
      public:
      TCompressionUnknownTopicConfig(const std::string &topic,
          const std::string &config_name)
          : TConfError(CreateMsg(topic, config_name)) {
      }

      private:
      static std::string CreateMsg(const std::string &topic,
          const std::string &config_name);
    };  // TCompressionUnknownTopicConfig

    class TCompressionMissingDefaultTopic final : public TConfError {
      public:
      TCompressionMissingDefaultTopic()
          : TConfError(
                "Compression config is missing defaultTopic definition") {
      }
    };  // TCompressionMissingDefaultTopic

    struct TCompressionConf final {
      class TBuilder;

      struct TConf final {
        Compress::TCompressionType Type = Compress::TCompressionType::None;

        /* Minimum total size of uncompressed message bodies required for
           compression to be used. */
        size_t MinSize = 0;

        /* Compression level, if specified. */
        Base::TOpt<int> Level;

        TConf() noexcept = default;

        TConf(Compress::TCompressionType type, size_t min_size,
            const Base::TOpt<int> &level) noexcept
            : Type(type),
              MinSize(min_size),
              Level(level) {
        }

        TConf(const TConf &) = default;

        TConf(TConf &&) = default;

        TConf &operator=(const TConf &) = default;

        TConf &operator=(TConf &&) = default;
      };  // TConf

      using TTopicMap = std::unordered_map<std::string, TConf>;

      static bool StringToType(const char *s,
          Compress::TCompressionType &result) noexcept;

      static bool StringToType(const std::string &s,
          Compress::TCompressionType &result) noexcept {
        return StringToType(s.c_str(), result);
      }

      size_t SizeThresholdPercent = 100;

      TConf DefaultTopicConfig;

      TTopicMap TopicConfigs;
    };  // TCompressionConf

    class TCompressionConf::TBuilder final {
      public:
      TBuilder() = default;

      void Reset() {
        assert(this);
        *this = TBuilder();
      }

      void AddNamedConfig(const std::string &name,
          Compress::TCompressionType type, size_t min_size,
          const Base::TOpt<int> &level);

      void SetSizeThresholdPercent(size_t size_threshold_percent);

      void SetDefaultTopicConfig(const std::string &config_name);

      void SetTopicConfig(const std::string &topic,
          const std::string &config_name);

      TCompressionConf Build();

      private:
      std::unordered_map<std::string, TConf> NamedConfigs;

      TCompressionConf BuildResult;

      bool GotSizeThresholdPercent = false;

      bool GotDefaultTopic = false;
    };  // TCompressionConf::TBuilder

  }  // Conf

}  // Dory
