/* <dory/conf/conf.h>

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

   Class representing configuration obtained from Dory's config file.
 */

#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>

#include <base/opt.h>
#include <dory/conf/batch_conf.h>
#include <dory/conf/compression_conf.h>
#include <dory/conf/conf_error.h>
#include <dory/conf/discard_logging_conf.h>
#include <dory/conf/http_interface_conf.h>
#include <dory/conf/input_config_conf.h>
#include <dory/conf/input_sources_conf.h>
#include <dory/conf/kafka_config_conf.h>
#include <dory/conf/logging_conf.h>
#include <dory/conf/msg_debug_conf.h>
#include <dory/conf/msg_delivery_conf.h>
#include <dory/conf/topic_rate_conf.h>
#include <dory/util/host_and_port.h>

namespace Dory {

  namespace Conf {

    class TLoggingInvalidLevel final : public TConfError {
      public:
      TLoggingInvalidLevel()
          : TConfError("Log level must be one of {\"ERR\", \"WARNING\", "
                "\"NOTICE\", \"INFO\", \"DEBUG\"}") {
      }
    };  // TLoggingInvalidLevel

    class TDiscardLoggingInvalidMaxFileSize final : public TConfError {
      public:
      TDiscardLoggingInvalidMaxFileSize()
          : TConfError(
                "If discard logging is enabled, discard_log_max_file_size "
                "must be at least twice the maximum input datagram or stream "
                "message size.") {
      }
    };  // TDiscardLoggingInvalidMaxFileSize

    struct TConf final {
      class TBuilder;

      using TBroker = Util::THostAndPort;

      TBatchConf BatchConf;

      TCompressionConf CompressionConf;

      TTopicRateConf TopicRateConf;

      TInputSourcesConf InputSourcesConf;

      TInputConfigConf InputConfigConf;

      TMsgDeliveryConf MsgDeliveryConf;

      THttpInterfaceConf HttpInterfaceConf;

      TDiscardLoggingConf DiscardLoggingConf;

      TKafkaConfigConf KafkaConfigConf;

      TMsgDebugConf MsgDebugConf;

      TLoggingConf LoggingConf;

      std::vector<TBroker> InitialBrokers;
    };  // TConf

    class TConf::TBuilder final {
      public:
      explicit TBuilder(bool allow_input_bind_ephemeral, bool enable_lz4);

      TConf Build(const void *buf, size_t buf_size);

      TConf Build(const char *xml) {
        return Build(xml, std::strlen(xml));
      }

      TConf Build(const std::string &xml) {
        return Build(xml.data(), xml.size());
      }

      void Reset() {
        assert(this);
        *this = TBuilder(AllowInputBindEphemeral, EnableLz4);
      }

      private:
      using TDomDocHandle =
          std::unique_ptr<xercesc::DOMDocument,
              void (*)(xercesc::DOMDocument *)>;

      void ProcessSingleBatchingNamedConfig(
          const xercesc::DOMElement &config_elem);

      void ProcessTopicBatchConfig(const xercesc::DOMElement &topic_elem,
          TBatchConf::TTopicAction &action, std::string &config);

      void ProcessBatchingElem(const xercesc::DOMElement &batching_elem);

      void ProcessSingleCompressionNamedConfig(
          const xercesc::DOMElement &config_elem);

      void ProcessCompressionElem(const xercesc::DOMElement &compression_elem);

      void ProcessTopicRateElem(const xercesc::DOMElement &topic_rate_elem);

      std::pair<std::string, Base::TOpt<mode_t>>
      ProcessFileSectionElem(const xercesc::DOMElement &elem);

      void ProcessInputSourcesElem(
          const xercesc::DOMElement &input_sources_elem);

      void ProcessInputConfigElem(
          const xercesc::DOMElement &input_config_elem);

      void ProcessMsgDeliveryElem(
          const xercesc::DOMElement &msg_delivery_elem);

      void ProcessHttpInterfaceElem(
          const xercesc::DOMElement &http_interface_elem);

      void ProcessDiscardLoggingElem(
          const xercesc::DOMElement &discard_logging_elem);

      void ProcessKafkaConfigElem(
          const xercesc::DOMElement &kafka_config_elem);

      void ProcessMsgDebugElem(const xercesc::DOMElement &msg_debug_elem);

      void ProcessLoggingElem(const xercesc::DOMElement &logging_elem);

      void ProcessInitialBrokersElem(
          const xercesc::DOMElement &initial_brokers_elem);

      void ProcessRootElem(const xercesc::DOMElement &root_elem);

      bool AllowInputBindEphemeral;

      bool EnableLz4;

      TDomDocHandle XmlDoc;

      TConf BuildResult;

      TBatchConf::TBuilder BatchingConfBuilder;

      TCompressionConf::TBuilder CompressionConfBuilder;

      TTopicRateConf::TBuilder TopicRateConfBuilder;
    };  // TConf::TBuilder

  }  // Conf

}  // Dory
