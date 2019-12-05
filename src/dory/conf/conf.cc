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

#include <limits>
#include <stdexcept>

#include <netinet/in.h>

#include <base/file_reader.h>
#include <base/no_default_case.h>
#include <base/to_integer.h>
#include <dory/compress/compression_type.h>
#include <log/pri.h>
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
using namespace Log;
using namespace Xml;
using namespace Xml::Config;

using TOpts = TAttrReader::TOpts;

TConf::TBuilder::TBuilder(bool allow_input_bind_ephemeral, bool enable_lz4)
    : AllowInputBindEphemeral(allow_input_bind_ephemeral),
      EnableLz4(enable_lz4),
      XmlDoc(MakeEmptyDomDocumentUniquePtr()) {
}

TConf TConf::TBuilder::Build(const void *buf, size_t buf_size) {
  assert(this);
  assert(buf);
  Reset();
  XmlDoc.reset(ParseXmlConfig(buf, buf_size, "US-ASCII"));
  const DOMElement *root = XmlDoc->getDocumentElement();
  assert(root);
  const std::string name = TranscodeToString(root->getNodeName());

  if (name != "doryConfig") {
    throw TUnexpectedElementName(*root, "doryConfig");
  }

  ProcessRootElem(*root);
  TConf result = std::move(BuildResult);
  Reset();
  return result;
}

void TConf::TBuilder::ProcessSingleBatchingNamedConfig(
    const DOMElement &config_elem) {
  assert(this);
  const std::string name = TAttrReader::GetString(config_elem, "name",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  RequireAllChildElementLeaves(config_elem);
  const auto subsection_map = GetSubsectionElements(config_elem,
      {{"time", false}, {"messages", false}, {"bytes", false}}, false);
  TBatchConf::TBatchValues values;

  if (subsection_map.count("time")) {
    values.OptTimeLimit = TAttrReader::GetOptUnsigned<decltype(
        values.OptTimeLimit)::TValueType>(
            *subsection_map.at("time"), "value", "disable", 0 | TBase::DEC,
            TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
  }

  if (subsection_map.count("messages")) {
    values.OptMsgCount = TAttrReader::GetOptUnsigned<decltype(
        values.OptMsgCount)::TValueType>(
            *subsection_map.at("messages"), "value", "disable", 0 | TBase::DEC,
            TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE |
            TOpts::ALLOW_K);
  }

  if (subsection_map.count("bytes")) {
    values.OptByteCount = TAttrReader::GetOptUnsigned<decltype(
        values.OptByteCount)::TValueType>(
            *subsection_map.at("bytes"), "value", "disable", 0 | TBase::DEC,
            TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE |
            TOpts::ALLOW_K);
  }

  if (values.OptTimeLimit.IsUnknown() && values.OptMsgCount.IsUnknown() &&
      values.OptByteCount.IsUnknown()) {
    std::string msg("Named batching config [");
    msg += name;
    msg += "] must specify at least one of {time, messages, bytes}";
    throw TInvalidBatchingConfig(config_elem, msg.c_str());
  }

  try {
    BatchingConfBuilder.AddNamedConfig(name, values);
  } catch (const TBatchDuplicateNamedConfig &x) {
    throw TInvalidAttr(config_elem, "name", name.c_str(),
        "Batching config contains duplicate named config");
  }
}

void TConf::TBuilder::ProcessTopicBatchConfig(const DOMElement &topic_elem,
    TBatchConf::TTopicAction &action, std::string &config) {
  assert(this);
  RequireLeaf(topic_elem);
  const std::string action_str = TAttrReader::GetString(topic_elem, "action",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

  if (!TBatchConf::StringToTopicAction(action_str, action)) {
    throw TInvalidAttr(topic_elem, "action", action_str.c_str());
  }

  TOpt<std::string> opt_name = TAttrReader::GetOptString(topic_elem, "config",
      0 | TOpts::TRIM_WHITESPACE);

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
        throw TInvalidAttr(topic_elem, "config", opt_name->c_str(),
            "Attribute value should be missing or empty");
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

void TConf::TBuilder::ProcessBatchingTopicConfigsElem(
  const DOMElement &topic_configs_elem) {
  assert(this);
  const auto item_vec = GetItemListElements(topic_configs_elem, "topic");

  for (const auto &item : item_vec) {
    const DOMElement &elem = *item;
    const std::string name = TAttrReader::GetString(elem, "name",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    TBatchConf::TTopicAction action = TBatchConf::TTopicAction::Disable;
    std::string config;
    ProcessTopicBatchConfig(elem, action, config);

    try {
      BatchingConfBuilder.SetTopicConfig(name, action, &config);
    } catch (const TBatchDuplicateTopicConfig &) {
      std::string msg("Value [");
      msg += name;
      msg += "] for attribute [name] of XML element [";
      msg += GetTranscoded(elem.getNodeName()).get();
      msg += "] is a duplicate";
      throw TInvalidAttr(elem, "name", name.c_str(), msg.c_str());
    } catch (const TBatchUnknownTopicConfig &) {
      throw TInvalidAttr(elem, "config", config.c_str());
    }
  }
}

void TConf::TBuilder::ProcessBatchingElem(const DOMElement &batching_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(batching_elem,
      {
        {"namedConfigs", false}, {"produceRequestDataLimit", true},
        {"messageMaxBytes", true}, {"combinedTopics", true},
        {"defaultTopic", true}, {"topicConfigs", false}
      },
      false);

  if (subsection_map.count("namedConfigs")) {
    const DOMElement &elem = *subsection_map.at("namedConfigs");
    const auto item_vec = GetItemListElements(elem, "config");

    for (const auto &item : item_vec) {
      ProcessSingleBatchingNamedConfig(*item);
    }
  }

  {
    const DOMElement &elem = *subsection_map.at("produceRequestDataLimit");
    RequireLeaf(elem);

    try {
      BatchingConfBuilder.SetProduceRequestDataLimit(
          TAttrReader::GetUnsigned<size_t>(elem, "value", 0 | TBase::DEC,
              0 | TOpts::ALLOW_K));
    } catch (const TBatchDuplicateProduceRequestDataLimit &) {
      throw TDuplicateElement(elem);
    }
  }

  {
    const DOMElement &elem = *subsection_map.at("messageMaxBytes");
    RequireLeaf(elem);

    try {
      BatchingConfBuilder.SetMessageMaxBytes(
          TAttrReader::GetUnsigned<size_t>(elem, "value", 0 | TBase::DEC,
              0 | TOpts::ALLOW_K));
    } catch (const TBatchDuplicateMessageMaxBytes &) {
      throw TDuplicateElement(elem);
    }
  }

  {
    const DOMElement &elem = *subsection_map.at("combinedTopics");
    RequireLeaf(elem);
    std::string config;
    bool enable = TAttrReader::GetBool(elem, "enable");

    if (enable) {
      config = TAttrReader::GetString(elem, "config",
          TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    }

    try {
      BatchingConfBuilder.SetCombinedTopicsConfig(enable, &config);
    } catch (const TBatchDuplicateCombinedTopicsConfig &) {
      throw TDuplicateElement(elem);
    } catch (const TBatchUnknownCombinedTopicsConfig &) {
      throw TInvalidAttr(elem, "config", config.c_str());
    }
  }

  {
    TBatchConf::TTopicAction action = TBatchConf::TTopicAction::Disable;
    std::string config;
    const DOMElement &elem = *subsection_map.at("defaultTopic");
    ProcessTopicBatchConfig(elem, action, config);

    try {
      BatchingConfBuilder.SetDefaultTopicConfig(action, &config);
    } catch (const TBatchUnknownDefaultTopicConfig &) {
      throw TInvalidAttr(elem, "config", config.c_str());
    }
  }

  if (subsection_map.count("topicConfigs")) {
    ProcessBatchingTopicConfigsElem(*subsection_map.at("topicConfigs"));
  }

  try {
    BuildResult.BatchConf = BatchingConfBuilder.Build();
  } catch (const TBatchMissingProduceRequestDataLimit &) {
    throw TMissingChildElement(batching_elem, "produceRequestDataLimit");
  } catch (const TBatchMissingMessageMaxBytes &) {
    throw TMissingChildElement(batching_elem, "messageMaxBytes");
  } catch (const TBatchMissingCombinedTopics &) {
    throw TMissingChildElement(batching_elem, "combinedTopics");
  } catch (const TBatchMissingDefaultTopic &) {
    throw TMissingChildElement(batching_elem, "defaultTopic");
  }
}

void TConf::TBuilder::ProcessSingleCompressionNamedConfig(
    const DOMElement &config_elem) {
  assert(this);
  const std::string name = TAttrReader::GetString(config_elem, "name",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  TCompressionType type = TCompressionType::None;
  const std::string type_str = TAttrReader::GetString(config_elem, "type",
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

  if (!TCompressionConf::StringToType(type_str, type)) {
    throw TInvalidAttr(config_elem, "type", type_str.c_str());
  }

  if (!EnableLz4 && (type == TCompressionType::Lz4)) {
    throw TInvalidAttr(config_elem, "type", type_str.c_str());
  }

  size_t min_size = 0;

  if (type != TCompressionType::None) {
    min_size = TAttrReader::GetUnsigned<size_t>(config_elem, "minSize",
        0 | TBase::DEC, 0 | TOpts::ALLOW_K);
  }

  try {
    CompressionConfBuilder.AddNamedConfig(name, type, min_size,
        TAttrReader::GetOptSigned<int>(config_elem, "level", nullptr));
  } catch (const TCompressionDuplicateNamedConfig &) {
    throw TInvalidAttr(config_elem, "name", name.c_str(),
        "Compression config contains duplicate named config");
  }
}

void TConf::TBuilder::ProcessCompressionTopicConfigsElem(
    const DOMElement &topic_configs_elem) {
  assert(this);
  RequireAllChildElementLeaves(topic_configs_elem);
  const auto item_vec = GetItemListElements(topic_configs_elem, "topic");

  for (const auto &item : item_vec) {
    const DOMElement &elem = *item;
    const std::string name = TAttrReader::GetString(elem, "name",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    const std::string config = TAttrReader::GetString(elem, "config",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

    try {
      CompressionConfBuilder.SetTopicConfig(name, config);
    } catch (const TBatchDuplicateTopicConfig &) {
      std::string msg("Value [");
      msg += name;
      msg += "] for attribute [name] of XML element [";
      msg += GetTranscoded(elem.getNodeName()).get();
      msg += "] is a duplicate";
      throw TInvalidAttr(elem, "name", name.c_str(), msg.c_str());
    } catch (const TBatchUnknownTopicConfig &) {
      throw TInvalidAttr(elem, "config", config.c_str());
    }
  }
}

void TConf::TBuilder::ProcessCompressionElem(
    const DOMElement &compression_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(compression_elem,
      {
        {"namedConfigs", true}, {"sizeThresholdPercent", false},
        {"defaultTopic", true}, {"topicConfigs", false}
      }, false);

  {
    const DOMElement &elem = *subsection_map.at("namedConfigs");
    RequireAllChildElementLeaves(elem);
    const auto item_vec = GetItemListElements(elem, "config");

    for (const auto &item : item_vec) {
      ProcessSingleCompressionNamedConfig(*item);
    }

    if (CompressionConfBuilder.IsNamedConfigsEmpty()) {
      throw TMissingChildElement(elem, "config");
    }
  }

  if (subsection_map.count("sizeThresholdPercent")) {
    const DOMElement &elem = *subsection_map.at("sizeThresholdPercent");
    RequireLeaf(elem);
    size_t size_threshold_percent = 0;

    try {
      size_threshold_percent = TAttrReader::GetUnsigned<size_t>(elem, "value",
          0 | TBase::DEC);
      CompressionConfBuilder.SetSizeThresholdPercent(size_threshold_percent);
    } catch (const TCompressionDuplicateSizeThresholdPercent &) {
      throw TDuplicateElement(elem);
    } catch (const TCompressionBadSizeThresholdPercent &) {
      throw TInvalidAttr(elem, "value",
          std::to_string(size_threshold_percent).c_str(),
          "Compression size threshold percent must be <= 100");
    }
  }

  {
    const DOMElement &elem = *subsection_map.at("defaultTopic");
    RequireLeaf(elem);
    const std::string config = TAttrReader::GetString(elem, "config",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

    try {
      CompressionConfBuilder.SetDefaultTopicConfig(config);
    } catch (const TCompressionUnknownDefaultTopicConfig &) {
      throw TInvalidAttr(elem, "config", config.c_str());
    }
  }

  if (subsection_map.count("topicConfigs")) {
    ProcessCompressionTopicConfigsElem(*subsection_map.at("topicConfigs"));
  }

  try {
    BuildResult.CompressionConf = CompressionConfBuilder.Build();
  } catch (const TCompressionMissingDefaultTopic &) {
    throw TMissingChildElement(compression_elem, "defaultTopic");
  }
}

void TConf::TBuilder::ProcessTopicRateTopicConfigsElem(
    const DOMElement &topic_configs_elem) {
  assert(this);
  RequireAllChildElementLeaves(topic_configs_elem);
  const auto topic_item_vec = GetItemListElements(topic_configs_elem,
      "topic");

  for (const auto &item : topic_item_vec) {
    const DOMElement &topic_elem = *item;
    const std::string name = TAttrReader::GetString(topic_elem, "name",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    const std::string config = TAttrReader::GetString(topic_elem, "config",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

    try {
      TopicRateConfBuilder.SetTopicConfig(name, config);
    } catch (const TTopicRateDuplicateTopicConfig &) {
      std::string msg("Value [");
      msg += name;
      msg += "] for attribute [name] of XML element [";
      msg += GetTranscoded(topic_elem.getNodeName()).get();
      msg += "] is a duplicate";
      throw TInvalidAttr(topic_configs_elem, "name", name.c_str(),
          msg.c_str());
    } catch (const TTopicRateUnknownTopicConfig &) {
      throw TInvalidAttr(topic_configs_elem, "config", config.c_str());
    }
  }
}

void TConf::TBuilder::ProcessTopicRateElem(const DOMElement &topic_rate_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(topic_rate_elem,
      {
        {"namedConfigs", true}, {"defaultTopic", true}, {"topicConfigs", false}
      }, false);

  {
    const DOMElement &named_configs_elem = *subsection_map.at("namedConfigs");
    RequireAllChildElementLeaves(named_configs_elem);
    const auto item_vec = GetItemListElements(named_configs_elem, "config");

    for (const auto &item : item_vec) {
      const DOMElement &elem = *item;
      const std::string name = TAttrReader::GetString(elem, "name",
          TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
      const TOpt<size_t> opt_max_count = TAttrReader::GetOptUnsigned<size_t>(
          elem, "maxCount", "unlimited", 0 | TBase::DEC,
          TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE |
              TOpts::ALLOW_K);

      if (opt_max_count.IsKnown()) {
        const auto interval = TAttrReader::GetUnsigned<size_t>(elem,
            "interval", 0 | TBase::DEC);

        try {
          TopicRateConfBuilder.AddBoundedNamedConfig(name, interval,
              *opt_max_count);
        } catch (const TTopicRateZeroRateLimitInterval &) {
          throw TInvalidAttr(elem, "interval",
              std::to_string(interval).c_str(),
              "Value of 0 not allowed for topic rate limiting config "
              "interval");
        } catch (const TTopicRateDuplicateNamedConfig &) {
          throw TInvalidAttr(elem, "name", name.c_str(),
              "Topic rate limiting config contains duplicate named config");
        }
      } else {
        try {
          TopicRateConfBuilder.AddUnlimitedNamedConfig(name);
        } catch (const TTopicRateDuplicateNamedConfig &) {
          throw TInvalidAttr(elem, "name", name.c_str(),
              "Topic rate limiting config contains duplicate named config");
        }
      }
    }
  }

  {
    const DOMElement &elem = *subsection_map.at("defaultTopic");
    RequireLeaf(elem);
    const std::string config = TAttrReader::GetString(elem, "config",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);

    try {
      TopicRateConfBuilder.SetDefaultTopicConfig(config);
    } catch (const TTopicRateUnknownDefaultTopicConfig &) {
      throw TInvalidAttr(elem, "config", config.c_str());
    }
  }

  if (subsection_map.count("topicConfigs")) {
    ProcessTopicRateTopicConfigsElem(*subsection_map.at("topicConfigs"));
  }

  try {
    BuildResult.TopicRateConf = TopicRateConfBuilder.Build();
  } catch (const TTopicRateMissingDefaultTopic &) {
    throw TMissingChildElement(topic_rate_elem, "defaultTopic");
  }
}

std::pair<std::string, TOpt<mode_t>>
TConf::TBuilder::ProcessFileSectionElem(const DOMElement &elem,
    std::unordered_map<std::string, const DOMElement *> &subsection_map) {
  assert(this);
  const bool enable = TAttrReader::GetBool(elem, "enable");
  RequireAllChildElementLeaves(elem);
  subsection_map = GetSubsectionElements(elem,
      {{"path", true}, {"mode", false}}, false);
  const DOMElement &path_elem = *subsection_map.at("path");

  /* Make sure path element has a value attribute, even if enable is false. */
  std::string path = TAttrReader::GetString(path_elem, "value");

  if (!enable) {
    path.clear();
  }

  TOpt<mode_t> mode;

  if (subsection_map.count("mode")) {
    const DOMElement &mode_elem = *subsection_map.at("mode");
    mode = TAttrReader::GetOptUnsigned<mode_t>(mode_elem, "value",
        "unspecified", 0 | TBase::BIN | TBase::OCT,
        TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE);
  }

  return std::make_pair(std::move(path), mode);
}

void TConf::TBuilder::ProcessInputSourcesElem(
    const DOMElement &input_sources_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(input_sources_elem,
      {
          {"unixDatagram", false}, {"unixStream", false}, {"tcp", false}
      }, false);

  bool source_specified = false;

  if (subsection_map.count("unixDatagram")) {
    const DOMElement &elem = *subsection_map.at("unixDatagram");
    std::unordered_map<std::string, const DOMElement *> subm;
    const auto unix_dg_conf = ProcessFileSectionElem(elem, subm);

    if (!unix_dg_conf.first.empty()) {
      source_specified = true;
    }

    try {
      BuildResult.InputSourcesConf.SetUnixDgConf(unix_dg_conf.first,
          unix_dg_conf.second);
    } catch (const TInputSourcesRelativeUnixDgPath &) {
      const DOMElement &path_elem = *subm.at("path");
      throw TInvalidAttr(path_elem, "value",
          TAttrReader::GetString(path_elem, "value").c_str(),
          "UNIX datagram socket path must be absolute");
    } catch (const TInputSourcesInvalidUnixDgFileMode &) {
      const DOMElement &mode_elem = *subm.at("mode");
      throw TInvalidAttr(mode_elem, "value",
          TAttrReader::GetString(mode_elem, "value").c_str(),
          "UNIX datagram socket mode must be <= 0777");
    }
  }

  if (subsection_map.count("unixStream")) {
    const DOMElement &elem = *subsection_map.at("unixStream");
    std::unordered_map<std::string, const DOMElement *> subm;
    const auto unix_stream_conf = ProcessFileSectionElem(elem, subm);

    if (!unix_stream_conf.first.empty()) {
      source_specified = true;
    }

    try {
      BuildResult.InputSourcesConf.SetUnixStreamConf(unix_stream_conf.first,
          unix_stream_conf.second);
    } catch (const TInputSourcesRelativeUnixStreamPath &) {
      const DOMElement &path_elem = *subm.at("path");
      throw TInvalidAttr(path_elem, "value",
          TAttrReader::GetString(path_elem, "value").c_str(),
          "UNIX stream socket path must be absolute");
    } catch (const TInputSourcesInvalidUnixStreamFileMode &) {
      const DOMElement &mode_elem = *subm.at("mode");
      throw TInvalidAttr(mode_elem, "value",
          TAttrReader::GetString(mode_elem, "value").c_str(),
          "UNIX stream socket mode must be <= 0777");
    }
  }

  if (subsection_map.count("tcp")) {
    const DOMElement &tcp_elem = *subsection_map.at("tcp");
    const bool enable = TAttrReader::GetBool(tcp_elem, "enable");
    RequireAllChildElementLeaves(tcp_elem);
    const auto tcp_subsection_map = GetSubsectionElements(tcp_elem,
        {{"port", true}}, false);
    const DOMElement &port_elem = *tcp_subsection_map.at("port");
    TOpt<in_port_t> port = TAttrReader::GetOptUnsigned<in_port_t>(port_elem,
        "value", nullptr, 0 | TBase::DEC);

    if (!enable) {
      port.Reset();
    }

    if (port.IsKnown()) {
      source_specified = true;
    }

    try {
      BuildResult.InputSourcesConf.SetTcpConf(port, AllowInputBindEphemeral);
    } catch (const TInvalidTcpInputPort &) {
      throw TInvalidAttr(port_elem, "value", std::to_string(*port).c_str(),
          "TCP input port must be > 0");
    }
  }

  if (!source_specified) {
    throw TNoInputSource(input_sources_elem);
  }
}

void TConf::TBuilder::ProcessInputConfigElem(
    const DOMElement &input_config_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(input_config_elem,
      {
          {"maxBuffer", false}, {"maxDatagramMsgSize", false},
          {"allowLargeUnixDatagrams", false}, {"maxStreamMsgSize", false}
      }, false);
  RequireAllChildElementLeaves(input_config_elem);

  if (subsection_map.count("maxBuffer")) {
    BuildResult.InputConfigConf.MaxBuffer = TAttrReader::GetUnsigned<decltype(
            BuildResult.InputConfigConf.MaxBuffer)>(
        *subsection_map.at("maxBuffer"), "value", 0 | TBase::DEC,
        TOpts::ALLOW_K | TOpts::ALLOW_M);
  }

  if (subsection_map.count("maxDatagramMsgSize")) {
    BuildResult.InputConfigConf.MaxDatagramMsgSize =
        TAttrReader::GetUnsigned<decltype(
                BuildResult.InputConfigConf.MaxDatagramMsgSize)>(
            *subsection_map.at("maxDatagramMsgSize"), "value", 0 | TBase::DEC,
            0 | TOpts::ALLOW_K | TOpts::ALLOW_M);
  }

  if (subsection_map.count("allowLargeUnixDatagrams")) {
    BuildResult.InputConfigConf.AllowLargeUnixDatagrams = TAttrReader::GetBool(
        *subsection_map.at("allowLargeUnixDatagrams"), "value");
  }

  if (subsection_map.count("maxStreamMsgSize")) {
    BuildResult.InputConfigConf.MaxStreamMsgSize =
        TAttrReader::GetUnsigned<decltype(
                BuildResult.InputConfigConf.MaxStreamMsgSize)>(
            *subsection_map.at("maxStreamMsgSize"), "value", 0 | TBase::DEC,
            0 | TOpts::ALLOW_K | TOpts::ALLOW_M);
  }
}

void TConf::TBuilder::ProcessMsgDeliveryElem(
    const DOMElement &msg_delivery_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(msg_delivery_elem,
      {
          {"topicAutocreate", false}, {"maxFailedDeliveryAttempts", false},
          {"shutdownMaxDelay", false}, {"dispatcherRestartMaxDelay", false},
          {"metadataRefreshInterval", false},
          {"compareMetadataOnRefresh", false}, {"kafkaSocketTimeout", false},
          {"pauseRateLimitInitial", false}, {"pauseRateLimitMaxDouble", false},
          {"minPauseDelay", false}
      }, false);
  RequireAllChildElementLeaves(msg_delivery_elem);

  if (subsection_map.count("topicAutocreate")) {
    BuildResult.MsgDeliveryConf.TopicAutocreate = TAttrReader::GetBool(
        *subsection_map.at("topicAutocreate"), "enable");
  }

  if (subsection_map.count("maxFailedDeliveryAttempts")) {
    BuildResult.MsgDeliveryConf.MaxFailedDeliveryAttempts =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.MaxFailedDeliveryAttempts)>(
            *subsection_map.at("maxFailedDeliveryAttempts"), "value",
            0 | TBase::DEC);
  }

  if (subsection_map.count("shutdownMaxDelay")) {
    BuildResult.MsgDeliveryConf.ShutdownMaxDelay =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.ShutdownMaxDelay)>(
            *subsection_map.at("shutdownMaxDelay"), "value", 0 | TBase::DEC);
  }

  if (subsection_map.count("dispatcherRestartMaxDelay")) {
    BuildResult.MsgDeliveryConf.DispatcherRestartMaxDelay =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.DispatcherRestartMaxDelay)>(
            *subsection_map.at("dispatcherRestartMaxDelay"), "value",
            0 | TBase::DEC);
  }

  if (subsection_map.count("metadataRefreshInterval")) {
    BuildResult.MsgDeliveryConf.MetadataRefreshInterval =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.MetadataRefreshInterval)>(
            *subsection_map.at("metadataRefreshInterval"), "value",
            0 | TBase::DEC);
  }

  if (subsection_map.count("compareMetadataOnRefresh")) {
    BuildResult.MsgDeliveryConf.CompareMetadataOnRefresh =
        TAttrReader::GetBool(
            *subsection_map.at("compareMetadataOnRefresh"), "value");
  }

  if (subsection_map.count("kafkaSocketTimeout")) {
    BuildResult.MsgDeliveryConf.KafkaSocketTimeout =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.KafkaSocketTimeout)>(
            *subsection_map.at("kafkaSocketTimeout"), "value", 0 | TBase::DEC);
  }

  if (subsection_map.count("pauseRateLimitInitial")) {
    BuildResult.MsgDeliveryConf.PauseRateLimitInitial =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.PauseRateLimitInitial)>(
            *subsection_map.at("pauseRateLimitInitial"), "value",
            0 | TBase::DEC);
  }

  if (subsection_map.count("pauseRateLimitMaxDouble")) {
    BuildResult.MsgDeliveryConf.PauseRateLimitMaxDouble =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.PauseRateLimitMaxDouble)>(
            *subsection_map.at("pauseRateLimitMaxDouble"), "value",
            0 | TBase::DEC);
  }

  if (subsection_map.count("minPauseDelay")) {
    BuildResult.MsgDeliveryConf.MinPauseDelay =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDeliveryConf.MinPauseDelay)>(
            *subsection_map.at("minPauseDelay"), "value", 0 | TBase::DEC);
  }
}

void TConf::TBuilder::ProcessHttpInterfaceElem(
    const DOMElement &http_interface_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(http_interface_elem,
      {
          {"port", false}, {"loopbackOnly", false},
          {"discardReportInterval", false}, {"badMsgPrefixSize", false}
      }, false);
  RequireAllChildElementLeaves(http_interface_elem);

  if (subsection_map.count("port")) {
    const DOMElement &elem = *subsection_map.at("port");
    const auto port =
        TAttrReader::GetUnsigned<decltype(BuildResult.HttpInterfaceConf.Port)>(
            elem, "value", 0 | TBase::DEC);

    try {
      BuildResult.HttpInterfaceConf.SetPort(port);
    } catch (const THttpInterfaceInvalidPort &) {
      throw TInvalidAttr(elem, "value", std::to_string(port).c_str(),
          "HTTP interface port must be > 0");
    }
  }

  if (subsection_map.count("loopbackOnly")) {
    BuildResult.HttpInterfaceConf.LoopbackOnly = TAttrReader::GetBool(
        *subsection_map.at("loopbackOnly"), "value");
  }

  if (subsection_map.count("discardReportInterval")) {
    const DOMElement &elem = *subsection_map.at("discardReportInterval");
    const auto interval = TAttrReader::GetUnsigned<decltype(
        BuildResult.HttpInterfaceConf.DiscardReportInterval)>(elem, "value",
            0 | TBase::DEC);

    try {
      BuildResult.HttpInterfaceConf.SetDiscardReportInterval(interval);
    } catch (const THttpInterfaceInvalidDiscardReportInterval &) {
      throw TInvalidAttr(elem, "value", std::to_string(interval).c_str(),
          "Discard report interval must be > 0");
    }
  }

  if (subsection_map.count("badMsgPrefixSize")) {
    BuildResult.HttpInterfaceConf.BadMsgPrefixSize =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.HttpInterfaceConf.BadMsgPrefixSize)>(
            *subsection_map.at("badMsgPrefixSize"), "value", 0 | TBase::DEC);
  }
}

void TConf::TBuilder::ProcessDiscardLoggingElem(
    const DOMElement &discard_logging_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(discard_logging_elem,
      {
          {"path", true}, {"maxFileSize", false},
          {"maxArchiveSize", false}, {"maxMsgPrefixSize", false}
      }, false);
  const bool enable = TAttrReader::GetBool(discard_logging_elem, "enable");
  RequireAllChildElementLeaves(discard_logging_elem);
  const DOMElement &path_elem = *subsection_map.at("path");
  std::string path = TAttrReader::GetString(path_elem, "value");

  if (subsection_map.count("maxFileSize")) {
    BuildResult.DiscardLoggingConf.MaxFileSize =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.DiscardLoggingConf.MaxFileSize)>(
            *subsection_map.at("maxFileSize"), "value", 0 | TBase::DEC,
            TOpts::ALLOW_K | TOpts::ALLOW_M);
  }

  if (subsection_map.count("maxArchiveSize")) {
    BuildResult.DiscardLoggingConf.MaxArchiveSize =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.DiscardLoggingConf.MaxArchiveSize)>(
            *subsection_map.at("maxArchiveSize"), "value", 0 | TBase::DEC,
            TOpts::ALLOW_K | TOpts::ALLOW_M);
  }

  if (subsection_map.count("maxMsgPrefixSize")) {
    using t_max_prefix_type =
        decltype(BuildResult.DiscardLoggingConf.MaxMsgPrefixSize);
    const TOpt<size_t> opt_max_size =
        TAttrReader::GetOptUnsigned<t_max_prefix_type>(
            *subsection_map.at("maxMsgPrefixSize"), "value", "unlimited",
            0 | TBase::DEC, TOpts::REQUIRE_PRESENCE |
                TOpts::STRICT_EMPTY_VALUE | TOpts::ALLOW_K);
    BuildResult.DiscardLoggingConf.MaxMsgPrefixSize =
        opt_max_size.IsKnown() ?
            *opt_max_size : std::numeric_limits<t_max_prefix_type>::max();
  }

  if (!enable) {
    path.clear();
  }

  try {
    BuildResult.DiscardLoggingConf.SetPath(path);
  } catch (const TDiscardLoggingRelativePath &) {
    throw TInvalidAttr(path_elem, "value", path.c_str(),
        "Discard logging path must be absolute");
  }
}

void TConf::TBuilder::ProcessKafkaConfigElem(
    const DOMElement &kafka_config_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(kafka_config_elem,
      {
          {"clientId", false}, {"replicationTimeout", false}
      }, false);
  RequireAllChildElementLeaves(kafka_config_elem);

  if (subsection_map.count("clientId")) {
    BuildResult.KafkaConfigConf.ClientId = TAttrReader::GetString(
        *subsection_map.at("clientId"), "value");
  }

  if (subsection_map.count("replicationTimeout")) {
    const DOMElement &elem = *subsection_map.at("replicationTimeout");
    const auto timeout = TAttrReader::GetUnsigned<decltype(
        BuildResult.KafkaConfigConf.ReplicationTimeout)>(elem, "value",
            0 | TBase::DEC);

    try {
      BuildResult.KafkaConfigConf.SetReplicationTimeout(timeout);
    } catch (const TKafkaConfigInvalidReplicationTimeout &) {
      throw TInvalidAttr(elem, "value", std::to_string(timeout).c_str(),
        "Invalid replication timeout");
    }
  }
}

void TConf::TBuilder::ProcessMsgDebugElem(const DOMElement &msg_debug_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(msg_debug_elem,
      {
          {"path", true}, {"timeLimit", false},
          {"byteLimit", false}
      }, false);
  const bool enable = TAttrReader::GetBool(msg_debug_elem, "enable");
  RequireAllChildElementLeaves(msg_debug_elem);
  const DOMElement &path_elem = *subsection_map.at("path");
  std::string path = TAttrReader::GetString(path_elem, "value");

  if (subsection_map.count("timeLimit")) {
    BuildResult.MsgDebugConf.TimeLimit =
        TAttrReader::GetUnsigned<decltype(
            BuildResult.MsgDebugConf.TimeLimit)>(
            *subsection_map.at("timeLimit"), "value", 0 | TBase::DEC);
  }

  if (subsection_map.count("byteLimit")) {
    BuildResult.MsgDebugConf.ByteLimit =
        TAttrReader::GetUnsigned<decltype(BuildResult.MsgDebugConf.ByteLimit)>(
            *subsection_map.at("byteLimit"), "value", 0 | TBase::DEC,
            TOpts::ALLOW_K | TOpts::ALLOW_M);
  }

  if (!enable) {
    path.clear();
  }

  try {
    BuildResult.MsgDebugConf.SetPath(path);
  } catch (const TMsgDebugRelativePath &) {
    throw TInvalidAttr(path_elem, "value", path.c_str(),
        "Message debug path must be absolute");
  }
}

void TConf::TBuilder::ProcessLoggingElem(const DOMElement &logging_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(logging_elem,
      {
          {"level", false}, {"stdoutStderr", false}, {"syslog", false},
          {"file", false}, {"logDiscards", false}
      }, false);

  if (subsection_map.count("level")) {
    const DOMElement &elem = *subsection_map.at("level");
    RequireLeaf(elem);
    const std::string level = TAttrReader::GetString(elem, "value");

    try {
      BuildResult.LoggingConf.Pri = ToPri(level);
    } catch (const std::range_error &) {
      throw TInvalidAttr(elem, "value", level.c_str(),
          "Logging level must be one of {EMERG, ALERT, CRIT, ERR, WARNING, "
          "NOTICE, INFO, DEBUG}");
    }
  }

  if (subsection_map.count("stdoutStderr")) {
    const DOMElement &elem = *subsection_map.at("stdoutStderr");
    RequireLeaf(elem);
    BuildResult.LoggingConf.EnableStdoutStderr = TAttrReader::GetBool(
        elem, "enable");
  }

  if (subsection_map.count("syslog")) {
    const DOMElement &elem = *subsection_map.at("syslog");
    RequireLeaf(elem);
    BuildResult.LoggingConf.EnableSyslog =
        TAttrReader::GetBool(elem, "enable");
  }

  if (subsection_map.count("file")) {
    const DOMElement &elem = *subsection_map.at("file");
    std::unordered_map<std::string, const DOMElement *> subm;
    const auto file_conf = ProcessFileSectionElem(elem, subm);

    try {
      BuildResult.LoggingConf.SetFileConf(file_conf.first, file_conf.second);
    } catch (const TLoggingRelativePath &) {
      const DOMElement &path_elem = *subm.at("path");
      throw TInvalidAttr(path_elem, "value",
          TAttrReader::GetString(path_elem, "value").c_str(),
          "Logfile path must be absolute");
    } catch (const TLoggingInvalidFileMode &) {
      const DOMElement &mode_elem = *subm.at("mode");
      throw TInvalidAttr(mode_elem, "value",
          TAttrReader::GetString(mode_elem, "value").c_str(),
          "Logfile mode must be <= 0777");
    }
  }

  if (subsection_map.count("logDiscards")) {
    const DOMElement &elem = *subsection_map.at("logDiscards");
    RequireLeaf(elem);
    BuildResult.LoggingConf.LogDiscards = TAttrReader::GetBool(elem, "enable");
  }
}

void TConf::TBuilder::ProcessInitialBrokersElem(
    const DOMElement &initial_brokers_elem) {
  assert(this);
  std::vector<TBroker> broker_vec;
  RequireAllChildElementLeaves(initial_brokers_elem);
  const auto broker_elem_vec = GetItemListElements(initial_brokers_elem,
      "broker");

  for (const auto &item : broker_elem_vec) {
    const DOMElement &elem = *item;
    std::string host = TAttrReader::GetString(elem, "host",
        TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
    const TOpt<in_port_t> opt_port = TAttrReader::GetOptUnsigned<in_port_t>(
        elem, "port", nullptr, 0 | TBase::DEC);
    const in_port_t port = opt_port.IsKnown() ? *opt_port : in_port_t(9092);
    broker_vec.emplace_back(std::move(host), port);
  }

  if (broker_vec.empty()) {
    throw TInitialBrokersMissing(initial_brokers_elem);
  }

  BuildResult.InitialBrokers = std::move(broker_vec);
}

void TConf::TBuilder::ProcessRootElem(const DOMElement &root_elem) {
  assert(this);
  const auto subsection_map = GetSubsectionElements(root_elem,
      {
        {"batching", false}, {"compression", false},
        {"topicRateLimiting", false}, {"inputSources", true},
        {"inputConfig", false}, {"msgDelivery", false},
        {"httpInterface", false}, {"discardLogging", false},
        {"kafkaConfig", false}, {"msgDebug", false}, {"logging", false},
        {"initialBrokers", true}
      },
      false);

  if (subsection_map.count("batching")) {
    ProcessBatchingElem(*subsection_map.at("batching"));
  }

  if (subsection_map.count("compression")) {
    ProcessCompressionElem(*subsection_map.at("compression"));
  }

  if (subsection_map.count("topicRateLimiting")) {
    ProcessTopicRateElem(*subsection_map.at("topicRateLimiting"));
  } else {
    /* The config file has no <topicRateLimiting> element, so create a default
       config that imposes no rate limit on any topic. */
    TopicRateConfBuilder.AddUnlimitedNamedConfig("unlimited");
    TopicRateConfBuilder.SetDefaultTopicConfig("unlimited");
    BuildResult.TopicRateConf = TopicRateConfBuilder.Build();
  }

  ProcessInputSourcesElem(*subsection_map.at("inputSources"));

  if (subsection_map.count("inputConfig")) {
    ProcessInputConfigElem(*subsection_map.at("inputConfig"));
  }

  if (subsection_map.count("msgDelivery")) {
    ProcessMsgDeliveryElem(*subsection_map.at("msgDelivery"));
  }

  if (subsection_map.count("httpInterface")) {
    ProcessHttpInterfaceElem(*subsection_map.at("httpInterface"));
  }

  if (subsection_map.count("discardLogging")) {
    const DOMElement &discard_logging_elem =
        *subsection_map.at("discardLogging");
    ProcessDiscardLoggingElem(discard_logging_elem);

    /* Here we rely on the inputConfig section having already been processed
       above. */
    const size_t max_input_msg_size = std::max(
      BuildResult.InputConfigConf.MaxDatagramMsgSize,
      BuildResult.InputConfigConf.MaxStreamMsgSize);

    if (!BuildResult.DiscardLoggingConf.Path.empty() &&
        ((BuildResult.DiscardLoggingConf.MaxFileSize) <
            (2 * max_input_msg_size))) {
      const DOMElement *max_file_size_elem =
          TryGetChildElement(discard_logging_elem, "maxFileSize");
      throw TDiscardLoggingInvalidMaxFileSize(
          max_file_size_elem ? *max_file_size_elem : discard_logging_elem);
    }
  }

  if (subsection_map.count("kafkaConfig")) {
    ProcessKafkaConfigElem(*subsection_map.at("kafkaConfig"));
  }

  if (subsection_map.count("msgDebug")) {
    ProcessMsgDebugElem(*subsection_map.at("msgDebug"));
  }

  if (subsection_map.count("logging")) {
    ProcessLoggingElem(*subsection_map.at("logging"));
  }

  ProcessInitialBrokersElem(*subsection_map.at("initialBrokers"));
}
