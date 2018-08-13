/* <dory/conf/conf.cc>

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

   Implements <dory/conf/conf.h>.
 */

#include <dory/conf/conf.h>

#include <netinet/in.h>

#include <base/file_reader.h>
#include <base/no_default_case.h>
#include <base/opt.h>
#include <dory/compress/compression_type.h>
#include <xml/config/config_errors.h>
#include <xml/config/config_util.h>
#include <xml/dom_document_util.h>
#include <xml/xml_string_util.h>

using namespace xercesc;

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Conf;
using namespace Dory::Util;
using namespace Xml;
using namespace Xml::Config;

using TOpts = TAttrReader::TOpts;

TConf::TBuilder::TBuilder(bool enable_lz4)
    : EnableLz4(enable_lz4),
      XmlDoc(MakeEmptyDomDocumentUniquePtr()) {
}

TConf TConf::TBuilder::Build(const char *config_filename) {
  assert(this);
  assert(config_filename);
  Reset();

  {
    std::string xml = TFileReader(config_filename).ReadIntoString();
    XmlDoc.reset(ParseXmlConfig(xml.data(), xml.size(), "US-ASCII"));
  }

  const DOMElement *root = XmlDoc->getDocumentElement();
  assert(root);
  std::string name = TranscodeToString(root->getNodeName());

  /* For backward compatibility with Bruce, allow root node to be named either
     "doryConfig" or "bruceConfig". */
  if ((name != "doryConfig") && (name != "bruceConfig")) {
    throw TUnexpectedElementName(*root, "doryConfig");
  }

  ProcessRootElem(*root);
  TConf result = std::move(BuildResult);
  Reset();
  return std::move(result);
}

void TConf::TBuilder::Reset() {
  assert(this);
  XmlDoc.reset();
  BuildResult = TConf();
  BatchingConfBuilder.Reset();
  CompressionConfBuilder.Reset();
  TopicRateConfBuilder.Reset();
}

void TConf::TBuilder::ProcessSingleBatchingNamedConfig(
    const DOMElement &config_elem) {
  assert(this);
  std::string name = TAttrReader::GetString(config_elem, "name",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  RequireAllChildElementLeaves(config_elem);
  auto subsection_map = GetSubsectionElements(config_elem,
      {{"time", true}, {"messages", true}, {"bytes", true}}, false);
  TBatchConf::TBatchValues values;
  values.OptTimeLimit = TAttrReader::GetOptInt2<size_t>(
      *subsection_map["time"], "value", "disable",
      TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
  values.OptMsgCount = TAttrReader::GetOptInt2<size_t>(
      *subsection_map["messages"], "value", "disable",
      TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE | TOpts::ALLOW_K);
  values.OptByteCount = TAttrReader::GetOptInt2<size_t>(
      *subsection_map["bytes"], "value", "disable",
      TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE | TOpts::ALLOW_K);

  if (values.OptTimeLimit.IsUnknown() && values.OptMsgCount.IsUnknown() &&
      values.OptByteCount.IsUnknown()) {
    std::string msg("Named batching config [");
    msg += name;
    msg += "] must not have a setting of [disable] for all values";
    throw TElementError(msg.c_str(), config_elem);
  }

  BatchingConfBuilder.AddNamedConfig(name, values);
}

void TConf::TBuilder::ProcessTopicBatchConfig(const DOMElement &topic_elem,
    TBatchConf::TTopicAction &action, std::string &config) {
  assert(this);
  RequireLeaf(topic_elem);
  std::string action_str = TAttrReader::GetString(topic_elem, "action",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

  if (!TBatchConf::StringToTopicAction(action_str, action)) {
    throw TInvalidAttr(topic_elem, "action", action_str.c_str());
  }

  TOpt<std::string> opt_name = TAttrReader::GetOptString(topic_elem, "config",
      TOpts::TRIM_WHITESPACE);

  if (opt_name.IsKnown() && opt_name->empty()) {
    opt_name.Reset();
  }

  switch (action) {
    case TBatchConf::TTopicAction::PerTopic: {
      if (opt_name.IsUnknown()) {
        throw TMissingAttrValue(topic_elem, "config");
      }

      break;
    }
    case TBatchConf::TTopicAction::CombinedTopics: {
      if (opt_name.IsKnown()) {
        throw TAttrError("Attribute value should be missing or empty",
            topic_elem, "config");
      }

      break;
    }
    case TBatchConf::TTopicAction::Disable: {
      break;
    }
    NO_DEFAULT_CASE;
  }

  config.clear();

  if (opt_name.IsKnown()) {
    config = *opt_name;
  }
}

void TConf::TBuilder::ProcessBatchingElem(const DOMElement &batching_elem) {
  assert(this);
  auto subsection_map = GetSubsectionElements(batching_elem,
      {
        {"namedConfigs", false}, {"produceRequestDataLimit", false},
        {"messageMaxBytes", false}, {"combinedTopics", false},
        {"defaultTopic", false}, {"topicConfigs", false}
      },
      false);

  if (subsection_map.count("namedConfigs")) {
    const DOMElement &elem = *subsection_map["namedConfigs"];
    auto item_vec = GetItemListElements(elem, "config");

    for (const auto &item : item_vec) {
      ProcessSingleBatchingNamedConfig(*item);
    }
  }

  if (subsection_map.count("produceRequestDataLimit")) {
    const DOMElement &elem = *subsection_map["produceRequestDataLimit"];
    RequireLeaf(elem);
    BatchingConfBuilder.SetProduceRequestDataLimit(
        TAttrReader::GetInt<size_t>(elem, "value", TOpts::ALLOW_K));
  }

  if (subsection_map.count("messageMaxBytes")) {
    const DOMElement &elem = *subsection_map["messageMaxBytes"];
    RequireLeaf(elem);
    BatchingConfBuilder.SetMessageMaxBytes(
        TAttrReader::GetInt<size_t>(elem, "value", TOpts::ALLOW_K));
  }

  if (subsection_map.count("combinedTopics")) {
    const DOMElement &elem = *subsection_map["combinedTopics"];
    RequireLeaf(elem);
    std::string config;
    bool enable = TAttrReader::GetBool(elem, "enable");

    if (enable) {
      config = TAttrReader::GetString(elem, "config",
          TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    }

    BatchingConfBuilder.SetCombinedTopicsConfig(enable, &config);
  }

  if (subsection_map.count("defaultTopic")) {
    TBatchConf::TTopicAction action = TBatchConf::TTopicAction::Disable;
    std::string config;
    ProcessTopicBatchConfig(*subsection_map["defaultTopic"], action, config);
    BatchingConfBuilder.SetDefaultTopicConfig(action, &config);
  }

  if (subsection_map.count("topicConfigs")) {
    auto item_vec = GetItemListElements(*subsection_map["topicConfigs"],
        "topic");

    for (const auto &item : item_vec) {
      const DOMElement &elem = *item;
      std::string name = TAttrReader::GetString(elem, "name",
          TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
      TBatchConf::TTopicAction action = TBatchConf::TTopicAction::Disable;
      std::string config;
      ProcessTopicBatchConfig(elem, action, config);
      BatchingConfBuilder.SetTopicConfig(name, action, &config);
    }
  }

  BuildResult.BatchConf = BatchingConfBuilder.Build();
}

void TConf::TBuilder::ProcessSingleCompressionNamedConfig(
    const DOMElement &config_elem) {
  assert(this);
  std::string name = TAttrReader::GetString(config_elem, "name",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  TCompressionType type = TCompressionType::None;
  std::string type_str = TAttrReader::GetString(config_elem, "type",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

  if (!TCompressionConf::StringToType(type_str, type)) {
    throw TInvalidAttr("Invalid compression type attribute", config_elem,
        "type", type_str.c_str());
  }

  if (!EnableLz4 && (type == TCompressionType::Lz4)) {
    throw TInvalidAttr("LZ4 compression is not yet supported", config_elem,
        "type", type_str.c_str());
  }

  size_t min_size = 0;

  if (type != TCompressionType::None) {
    min_size = TAttrReader::GetInt<size_t>(config_elem, "minSize",
        TOpts::ALLOW_K);
  }

  CompressionConfBuilder.AddNamedConfig(name, type, min_size,
      TAttrReader::GetOptInt<int>(config_elem, "level"));
}

void TConf::TBuilder::ProcessCompressionElem(
    const DOMElement &compression_elem) {
  assert(this);
  auto subsection_map = GetSubsectionElements(compression_elem,
      {
        {"namedConfigs", false}, {"sizeThresholdPercent", false},
        {"defaultTopic", false}, {"topicConfigs", false}
      }, false);

  if (subsection_map.count("namedConfigs")) {
    const DOMElement &elem = *subsection_map["namedConfigs"];
    RequireAllChildElementLeaves(elem);
    auto item_vec = GetItemListElements(elem, "config");

    for (const auto &item : item_vec) {
      ProcessSingleCompressionNamedConfig(*item);
    }
  }

  if (subsection_map.count("sizeThresholdPercent")) {
    const DOMElement &elem = *subsection_map["sizeThresholdPercent"];
    RequireLeaf(elem);
    CompressionConfBuilder.SetSizeThresholdPercent(
        TAttrReader::GetInt<size_t>(elem, "value"));
  }

  if (subsection_map.count("defaultTopic")) {
    const DOMElement &elem = *subsection_map["defaultTopic"];
    RequireLeaf(elem);
    CompressionConfBuilder.SetDefaultTopicConfig(
        TAttrReader::GetString(elem, "config",
            TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY));
  }

  if (subsection_map.count("topicConfigs")) {
    const DOMElement &topic_configs_elem = *subsection_map["topicConfigs"];
    RequireAllChildElementLeaves(topic_configs_elem);
    auto item_vec = GetItemListElements(topic_configs_elem, "topic");

    for (const auto &item : item_vec) {
      const DOMElement &elem = *item;
      std::string name = TAttrReader::GetString(elem, "name",
          TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
      CompressionConfBuilder.SetTopicConfig(name,
          TAttrReader::GetString(elem, "config",
              TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY));
    }
  }

  BuildResult.CompressionConf = CompressionConfBuilder.Build();
}

void TConf::TBuilder::ProcessTopicRateElem(const DOMElement &topic_rate_elem) {
  assert(this);
  auto subsection_map = GetSubsectionElements(topic_rate_elem,
      {
        {"namedConfigs", true}, {"defaultTopic", false},
        {"topicConfigs", false}
      }, false);
  const DOMElement &named_configs_elem = *subsection_map["namedConfigs"];
  RequireAllChildElementLeaves(named_configs_elem);
  auto item_vec = GetItemListElements(named_configs_elem, "config");

  for (const auto &item : item_vec) {
    const DOMElement &elem = *item;
    std::string name = TAttrReader::GetString(elem, "name",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    TOpt<size_t> opt_max_count = TAttrReader::GetOptInt2<size_t>(elem,
        "maxCount", "unlimited",
        TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE | TOpts::ALLOW_K);

    if (opt_max_count.IsKnown()) {
      TopicRateConfBuilder.AddBoundedNamedConfig(name,
          TAttrReader::GetInt<size_t>(elem, "interval"), *opt_max_count);
    } else {
      TopicRateConfBuilder.AddUnlimitedNamedConfig(name);
    }
  }

  if (subsection_map.count("defaultTopic")) {
    const DOMElement &elem = *subsection_map["defaultTopic"];
    RequireLeaf(elem);
    TopicRateConfBuilder.SetDefaultTopicConfig(TAttrReader::GetString(
        elem, "config", TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY));
  }

  if (subsection_map.count("topicConfigs")) {
    const DOMElement &elem = *subsection_map["topicConfigs"];
    RequireAllChildElementLeaves(elem);
    auto topic_item_vec = GetItemListElements(elem, "topic");

    for (const auto &item : topic_item_vec) {
      const DOMElement &topic_elem = *item;
      std::string name = TAttrReader::GetString(topic_elem, "name",
          TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
      TopicRateConfBuilder.SetTopicConfig(name,
          TAttrReader::GetString(topic_elem, "config",
              TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY));
    }
  }

  BuildResult.TopicRateConf = TopicRateConfBuilder.Build();
}

void TConf::TBuilder::ProcessInitialBrokersElem(
    const DOMElement &initial_brokers_elem) {
  assert(this);
  std::vector<TBroker> broker_vec;
  RequireAllChildElementLeaves(initial_brokers_elem);
  auto broker_elem_vec = GetItemListElements(initial_brokers_elem, "broker");

  for (const auto &item : broker_elem_vec) {
    const DOMElement &elem = *item;
    std::string host = TAttrReader::GetString(elem, "host",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    TOpt<in_port_t> opt_port = TAttrReader::GetOptInt<in_port_t>(elem, "port");
    in_port_t port = opt_port.IsKnown() ?
        *opt_port : in_port_t(DEFAULT_BROKER_PORT);
    broker_vec.emplace_back(std::move(host), port);
  }

  if (broker_vec.empty()) {
    throw TElementError("Initial brokers missing", initial_brokers_elem);
  }

  BuildResult.InitialBrokers = std::move(broker_vec);
}

void TConf::TBuilder::ProcessRootElem(const DOMElement &root_elem) {
  assert(this);
  auto subsection_map = GetSubsectionElements(root_elem,
      {
        {"batching", true}, {"compression", true},
        {"topicRateLimiting", false}, {"initialBrokers", true}
      },
      false);

  ProcessBatchingElem(*subsection_map["batching"]);
  ProcessCompressionElem(*subsection_map["compression"]);

  if (subsection_map.count("topicRateLimiting")) {
    ProcessTopicRateElem(*subsection_map["topicRateLimiting"]);
  } else {
    /* The config file has no <topicRateLimiting> element, so create a default
       config that imposes no rate limit on any topic. */
    TopicRateConfBuilder.AddUnlimitedNamedConfig("unlimited");
    TopicRateConfBuilder.SetDefaultTopicConfig("unlimited");
    BuildResult.TopicRateConf = TopicRateConfBuilder.Build();
  }

  ProcessInitialBrokersElem(*subsection_map["initialBrokers"]);
}
