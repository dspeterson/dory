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

#include <base/no_copy_semantics.h>
#include <dory/conf/batch_conf.h>
#include <dory/conf/compression_conf.h>
#include <dory/conf/topic_rate_conf.h>
#include <dory/util/host_and_port.h>

namespace Dory {

  namespace Conf {

    class TConf {
      public:
      class TBuilder;

      using TBroker = Util::THostAndPort;

      const TBatchConf &GetBatchConf() const {
        assert(this);
        return BatchConf;
      }

      const TCompressionConf &GetCompressionConf() const {
        assert(this);
        return CompressionConf;
      }

      const TTopicRateConf &GetTopicRateConf() const {
        assert(this);
        return TopicRateConf;
      }

      const std::vector<TBroker> &GetInitialBrokers() const {
        assert(this);
        return InitialBrokers;
      }

      private:
      TBatchConf BatchConf;

      TCompressionConf CompressionConf;

      TTopicRateConf TopicRateConf;

      std::vector<TBroker> InitialBrokers;
    };  // TConf

    class TConf::TBuilder {
      NO_COPY_SEMANTICS(TBuilder);

      public:
      TBuilder();

      TConf Build(const char *config_filename);

      void Reset();

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

      void ProcessInitialBrokersElem(
          const xercesc::DOMElement &initial_brokers_elem);

      void ProcessRootElem(const xercesc::DOMElement &root_elem);

      TDomDocHandle XmlDoc;

      TConf BuildResult;

      TBatchConf::TBuilder BatchingConfBuilder;

      TCompressionConf::TBuilder CompressionConfBuilder;

      TTopicRateConf::TBuilder TopicRateConfBuilder;
    };  // TConf::TBuilder

  }  // Conf

}  // Dory
