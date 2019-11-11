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
#include <memory>
#include <string>
#include <vector>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>

#include <dory/conf/batch_conf.h>
#include <dory/conf/compression_conf.h>
#include <dory/conf/logging_conf.h>
#include <dory/conf/topic_rate_conf.h>
#include <dory/util/host_and_port.h>

namespace Dory {

  namespace Conf {

    class TConf {
      public:
      class TBuilder;

      using TBroker = Util::THostAndPort;

      const TBatchConf &GetBatchConf() const noexcept {
        assert(this);
        return BatchConf;
      }

      const TCompressionConf &GetCompressionConf() const noexcept {
        assert(this);
        return CompressionConf;
      }

      const TTopicRateConf &GetTopicRateConf() const noexcept {
        assert(this);
        return TopicRateConf;
      }

      const TLoggingConf &GetLoggingConf() const noexcept {
        assert(this);
        return LoggingConf;
      }

      const std::vector<TBroker> &GetInitialBrokers() const noexcept {
        assert(this);
        return InitialBrokers;
      }

      TConf() = default;

      TConf(const TConf &) = default;

      TConf(TConf &&) = default;

      TConf &operator=(const TConf &) = default;

      TConf &operator=(TConf &&) = default;

      private:
      TBatchConf BatchConf;

      TCompressionConf CompressionConf;

      TTopicRateConf TopicRateConf;

      TLoggingConf LoggingConf;

      std::vector<TBroker> InitialBrokers;
    };  // TConf

    class TConf::TBuilder {
      public:
      explicit TBuilder(bool enable_lz4 = false);

      TBuilder(const TBuilder &) = default;

      TBuilder(TBuilder &&) = default;

      TBuilder &operator=(const TBuilder &) = default;

      TBuilder &operator=(TBuilder &&) = default;

      TConf Build(const char *config_filename);

      void Reset() {
        assert(this);
        *this = TBuilder(EnableLz4);
      }

      private:
      using TDomDocHandle =
          std::unique_ptr<xercesc::DOMDocument,
              void (*)(xercesc::DOMDocument *)>;

      enum { DEFAULT_BROKER_PORT = 9092 };

      void ProcessSingleBatchingNamedConfig(
          const xercesc::DOMElement &config_elem);

      void ProcessTopicBatchConfig(const xercesc::DOMElement &topic_elem,
          TBatchConf::TTopicAction &action, std::string &config);

      void ProcessBatchingElem(const xercesc::DOMElement &batching_elem);

      void ProcessSingleCompressionNamedConfig(
          const xercesc::DOMElement &config_elem);

      void ProcessCompressionElem(const xercesc::DOMElement &compression_elem);

      void ProcessTopicRateElem(const xercesc::DOMElement &topic_rate_elem);

      void ProcessLoggingElem(const xercesc::DOMElement &logging_elem);

      void ProcessInitialBrokersElem(
          const xercesc::DOMElement &initial_brokers_elem);

      void ProcessRootElem(const xercesc::DOMElement &root_elem);

      bool EnableLz4;

      TDomDocHandle XmlDoc;

      TConf BuildResult;

      TBatchConf::TBuilder BatchingConfBuilder;

      TCompressionConf::TBuilder CompressionConfBuilder;

      TLoggingConf::TBuilder LoggingConfBuilder;

      TTopicRateConf::TBuilder TopicRateConfBuilder;
    };  // TConf::TBuilder

  }  // Conf

}  // Dory
