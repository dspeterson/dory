/* <dory/conf/compression_conf.cc>

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

   Implements <dory/conf/compression_conf.h>.
 */

#include <dory/conf/compression_conf.h>

#include <utility>

#include <strings.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Conf;

bool TCompressionConf::StringToType(const char *s,
    TCompressionType &result) noexcept {
  assert(s);

  if (!strcasecmp(s, "none")) {
    result = TCompressionType::None;
    return true;
  }

  if (!strcasecmp(s, "gzip")) {
    result = TCompressionType::Gzip;
    return true;
  }

  if (!strcasecmp(s, "snappy")) {
    result = TCompressionType::Snappy;
    return true;
  }

  if (!strcasecmp(s, "lz4")) {
    result = TCompressionType::Lz4;
    return true;
  }

  return false;
}

std::string TCompressionDuplicateNamedConfig::CreateMsg(
    const std::string &config_name) {
  std::string msg("Compression config contains duplicate named config: [");
  msg += config_name;
  msg += "]";
  return msg;
}

std::string TCompressionUnknownDefaultTopicConfig::CreateMsg(
    const std::string &config_name) {
  std::string msg(
      "Compression config defaultTopic definition references unknown named config: [");
  msg += config_name;
  msg += "]";
  return msg;
}

std::string TCompressionDuplicateTopicConfig::CreateMsg(
    const std::string &topic) {
  std::string msg(
      "Compression config contains duplicate specification for topic [");
  msg += topic;
  msg += "]";
  return msg;
}

std::string TCompressionUnknownTopicConfig::CreateMsg(const std::string &topic,
    const std::string &config_name) {
  std::string msg("Compression config for topic [");
  msg += topic;
  msg += "] references unknown named config: [";
  msg += config_name;
  msg += "]";
  return msg;
}

void TCompressionConf::TBuilder::AddNamedConfig(const std::string &name,
    TCompressionType type, size_t min_size, const TOpt<int> &level) {
  assert(this);

  if (type == TCompressionType::None) {
    min_size = 0;
  }

  auto result =
      NamedConfigs.insert(std::make_pair(name, TConf(type, min_size, level)));

  if (!result.second) {
    throw TCompressionDuplicateNamedConfig(name);
  }
}

void TCompressionConf::TBuilder::SetSizeThresholdPercent(
    size_t size_threshold_percent) {
  assert(this);

  if (GotSizeThresholdPercent) {
    throw TCompressionDuplicateSizeThresholdPercent();
  }

  if (size_threshold_percent > 100) {
    throw TCompressionBadSizeThresholdPercent();
  }

  BuildResult.SizeThresholdPercent = size_threshold_percent;
  GotSizeThresholdPercent = true;
}

void TCompressionConf::TBuilder::SetDefaultTopicConfig(
    const std::string &config_name) {
  assert(this);

  if (GotDefaultTopic) {
    throw TCompressionDuplicateDefaultTopicConfig();
  }

  auto iter = NamedConfigs.find(config_name);

  if (iter == NamedConfigs.end()) {
    throw TCompressionUnknownDefaultTopicConfig(config_name);
  }

  BuildResult.DefaultTopicConfig = iter->second;
  GotDefaultTopic = true;
}

void TCompressionConf::TBuilder::SetTopicConfig(const std::string &topic,
    const std::string &config_name) {
  assert(this);

  if (BuildResult.TopicConfigs.find(topic) != BuildResult.TopicConfigs.end()) {
    throw TCompressionDuplicateTopicConfig(topic);
  }

  const auto iter = NamedConfigs.find(config_name);

  if (iter == NamedConfigs.end()) {
    throw TCompressionUnknownTopicConfig(topic, config_name);
  }

  BuildResult.TopicConfigs.insert(std::make_pair(topic, iter->second));
}

TCompressionConf TCompressionConf::TBuilder::Build() {
  assert(this);

  if (!GotDefaultTopic) {
    throw TCompressionMissingDefaultTopic();
  }

  TCompressionConf result = std::move(BuildResult);
  Reset();
  return result;
}
