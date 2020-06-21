/* <dory/conf/batch_conf.cc>

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

   Implements <dory/conf/batch_conf.h>.
 */

#include <dory/conf/batch_conf.h>

#include <cstring>
#include <utility>

using namespace Dory;
using namespace Dory::Conf;

bool TBatchConf::StringToTopicAction(const char *s,
    TTopicAction &result) noexcept {
  assert(s);

  if (!std::strcmp(s, "perTopic")) {
    result = TTopicAction::PerTopic;
    return true;
  }

  if (!std::strcmp(s, "combinedTopics")) {
    result = TTopicAction::CombinedTopics;
    return true;
  }

  if (!std::strcmp(s, "disable")) {
    result = TTopicAction::Disable;
    return true;
  }

  return false;
}

std::string TBatchDuplicateNamedConfig::CreateMsg(
    const std::string &config_name) {
  std::string msg("Batching config contains duplicate named config: [");
  msg += config_name;
  msg += "]";
  return msg;
}

std::string TBatchUnknownCombinedTopicsConfig::CreateMsg(
    const std::string &config_name) {
  std::string msg(
      "Batching config combinedTopics definition references unknown named config: [");
  msg += config_name;
  msg += "]";
  return msg;
}

std::string TBatchUnknownDefaultTopicConfig::CreateMsg(
    const std::string &config_name) {
  std::string msg(
      "Batching config defaultTopic definition references unknown named config: [");
  msg += config_name;
  msg += "]";
  return msg;
}

std::string TBatchDuplicateTopicConfig::CreateMsg(
    const std::string &topic) {
  std::string msg(
      "Batching config contains duplicate specification for topic [");
  msg += topic;
  msg += "]";
  return msg;
}

std::string TBatchUnknownTopicConfig::CreateMsg(
    const std::string &topic, const std::string &config_name) {
  std::string msg("Batching config for topic [");
  msg += topic;
  msg += "] references unknown named config: [";
  msg += config_name;
  msg += "]";
  return msg;
}

void TBatchConf::TBuilder::AddNamedConfig(const std::string &name,
    const TBatchValues &values) {
  const auto result = NamedConfigs.insert(std::make_pair(name, values));

  if (!result.second) {
    throw TBatchDuplicateNamedConfig(name);
  }
}

void TBatchConf::TBuilder::SetProduceRequestDataLimit(size_t limit) {
  if (GotProduceRequestDataLimit) {
    throw TBatchDuplicateProduceRequestDataLimit();
  }

  BuildResult.ProduceRequestDataLimit = limit;
  GotProduceRequestDataLimit = true;
}

void TBatchConf::TBuilder::SetMessageMaxBytes(size_t message_max_bytes) {
  if (GotMessageMaxBytes) {
    throw TBatchDuplicateMessageMaxBytes();
  }

  BuildResult.MessageMaxBytes = message_max_bytes;
  GotMessageMaxBytes = true;
}

void TBatchConf::TBuilder::SetCombinedTopicsConfig(bool enabled,
    const std::string *config_name) {
  assert(!enabled || config_name);

  if (GotCombinedTopics) {
    throw TBatchDuplicateCombinedTopicsConfig();
  }

  BuildResult.CombinedTopicsBatchingEnabled = enabled;

  if (enabled) {
    auto iter = NamedConfigs.find(*config_name);

    if (iter == NamedConfigs.end()) {
      throw TBatchUnknownCombinedTopicsConfig(*config_name);
    }

    BuildResult.CombinedTopicsConfig = iter->second;
  }

  GotCombinedTopics = true;
}

void TBatchConf::TBuilder::SetDefaultTopicConfig(TTopicAction action,
    const std::string *config_name) {
  assert((action != TTopicAction::PerTopic) || config_name);

  if (GotDefaultTopic) {
    throw TBatchDuplicateDefaultTopicConfig();
  }

  BuildResult.DefaultTopicAction = action;

  if (action == TTopicAction::PerTopic) {
    auto iter = NamedConfigs.find(*config_name);

    if (iter == NamedConfigs.end()) {
      throw TBatchUnknownDefaultTopicConfig(*config_name);
    }

    BuildResult.DefaultTopicConfig = iter->second;
  }

  GotDefaultTopic = true;
}

void TBatchConf::TBuilder::SetTopicConfig(const std::string &topic,
    TTopicAction action, const std::string *config_name) {
  assert((action != TTopicAction::PerTopic) || config_name);

  if (BuildResult.TopicConfigs.find(topic) != BuildResult.TopicConfigs.end()) {
    throw TBatchDuplicateTopicConfig(topic);
  }

  TBatchValues values;

  if (action == TTopicAction::PerTopic) {
    auto iter = NamedConfigs.find(*config_name);

    if (iter == NamedConfigs.end()) {
      throw TBatchUnknownTopicConfig(topic, *config_name);
    }

    values = iter->second;
  }

  BuildResult.TopicConfigs.insert(
      std::make_pair(topic, TTopicConf(action, values)));
}

TBatchConf TBatchConf::TBuilder::Build() {
  if (!GotProduceRequestDataLimit) {
    throw TBatchMissingProduceRequestDataLimit();
  }

  if (!GotMessageMaxBytes) {
    throw TBatchMissingMessageMaxBytes();
  }

  if (!GotCombinedTopics) {
    throw TBatchMissingCombinedTopics();
  }

  if (!GotDefaultTopic) {
    throw TBatchMissingDefaultTopic();
  }

  TBatchConf result = std::move(BuildResult);
  Reset();
  return result;
}
