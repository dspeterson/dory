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

#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <dory/compress/compression_type.h>
#include <dory/conf/conf_error.h>

namespace Dory {

  namespace Conf {

    class TCompressionConf {
      public:
      class TBuilder;

      struct TConf {
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

        TConf &operator=(const TConf &) = default;
      };  // TConf

      using TTopicMap = std::unordered_map<std::string, TConf>;

      static bool StringToType(const char *s,
          Compress::TCompressionType &result) noexcept;

      static bool StringToType(const std::string &s,
          Compress::TCompressionType &result) noexcept {
        return StringToType(s.c_str(), result);
      }

      TCompressionConf() = default;

      TCompressionConf(const TCompressionConf &) = default;

      TCompressionConf(TCompressionConf &&) = default;

      TCompressionConf &operator=(const TCompressionConf &) = default;

      TCompressionConf &operator=(TCompressionConf &&) = default;

      size_t GetSizeThresholdPercent() const noexcept {
        assert(this);
        return SizeThresholdPercent;
      }

      const TConf &GetDefaultTopicConfig() const noexcept {
        assert(this);
        return DefaultTopicConfig;
      }

      const TTopicMap &GetTopicConfigs() const noexcept {
        assert(this);
        return TopicConfigs;
      }

      private:
      size_t SizeThresholdPercent = 100;

      TConf DefaultTopicConfig;

      TTopicMap TopicConfigs;
    };  // TCompressionConf

    class TCompressionConf::TBuilder {
      NO_COPY_SEMANTICS(TBuilder);

      public:
      /* Exception base class. */
      class TErrorBase : public TConfError {
        protected:
        explicit TErrorBase(std::string &&msg)
            : TConfError(std::move(msg)) {
        }
      };  // TErrorBase

      class TDuplicateNamedConfig final : public TErrorBase {
        public:
        explicit TDuplicateNamedConfig(const std::string &config_name)
            : TErrorBase(CreateMsg(config_name)) {
        }

        private:
        static std::string CreateMsg(const std::string &config_name);
      };  // TDuplicateNamedConfig

      class TDuplicateSizeThresholdPercent final : public TErrorBase {
        public:
        TDuplicateSizeThresholdPercent()
            : TErrorBase(
                  "Compression config contains duplicate sizeThresholdPercent definition") {
        }
      };  // TDuplicateSizeThresholdPercent

      class TBadSizeThresholdPercent final : public TErrorBase {
        public:
        TBadSizeThresholdPercent()
            : TErrorBase(
                  "Compression config contains bad sizeThresholdPercent value: must be <= 100") {
        }
      };  // TBadSizeThresholdPercent

      class TDuplicateDefaultTopicConfig final : public TErrorBase {
        public:
        TDuplicateDefaultTopicConfig()
            : TErrorBase(
                  "Compression config contains duplicate defaultTopic definition") {
        }
      };  // TDuplicateDefaultTopicConfig

      class TUnknownDefaultTopicConfig final : public TErrorBase {
        public:
        explicit TUnknownDefaultTopicConfig(const std::string &config_name)
            : TErrorBase(CreateMsg(config_name)) {
        }

        private:
        static std::string CreateMsg(const std::string &config_name);
      };  // TUnknownDefaultTopicConfig

      class TDuplicateTopicConfig final : public TErrorBase {
        public:
        explicit TDuplicateTopicConfig(const std::string &topic)
            : TErrorBase(CreateMsg(topic)) {
        }

        private:
        static std::string CreateMsg(const std::string &topic);
      };  // TDuplicateTopicConfig

      class TUnknownTopicConfig final : public TErrorBase {
        public:
        TUnknownTopicConfig(const std::string &topic,
            const std::string &config_name)
            : TErrorBase(CreateMsg(topic, config_name)) {
        }

        private:
        static std::string CreateMsg(const std::string &topic,
            const std::string &config_name);
      };  // TUnknownTopicConfig

      class TMissingDefaultTopic final : public TErrorBase {
        public:
        TMissingDefaultTopic()
            : TErrorBase(
                  "Compression config is missing defaultTopic definition") {
        }
      };  // TMissingDefaultTopic

      TBuilder()
          : GotSizeThresholdPercent(false),
            GotDefaultTopic(false) {
      }

      void Reset();

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

      bool GotSizeThresholdPercent;

      bool GotDefaultTopic;
    };  // TCompressionConf::TBuilder

  }  // Conf

}  // Dory
