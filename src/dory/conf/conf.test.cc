/* <dory/conf/conf.test.cc>

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

   Unit test for <dory/conf/conf.h>
 */

#include <dory/conf/conf.h>

#include <fstream>

#include <base/tmp_file.h>
#include <dory/compress/compression_type.h>
#include <xml/test/xml_test_initializer.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Conf;
using namespace Xml;
using namespace Xml::Test;

namespace {

  /* The fixture for testing Dory's config file implementation. */
  class TConfTest : public ::testing::Test {
    protected:
    TConfTest() = default;

    ~TConfTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }

    TXmlTestInitializer Initializer;  // initializes Xerces XML library
  };  // TConfTest

  TEST_F(TConfTest, Test1) {
    TTmpFile tmp_file("/tmp/dory_conf_test.XXXXXX",
        true /* delete_on_destroy */);
    std::ofstream ofs(tmp_file.GetName());
    ofs << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
        << "<doryConfig>" << std::endl
        << "<!-- this is a comment -->" << std::endl
        << "    <batching>" << std::endl
        << "        <namedConfigs>" << std::endl
        << "            <config name=\"config1\">" << std::endl
        << "                <time value=\"50\" />" << std::endl
        << "                <messages value=\"100\" />" << std::endl
        << "                <bytes value=\"200\" />" << std::endl
        << "            </config>" << std::endl
        << "            <config name=\"config2\">" << std::endl
        << "                <time value=\"5\" />" << std::endl
        << "                <messages value=\"disable\" />" << std::endl
        << "                <bytes value=\"20k\" />" << std::endl
        << "            </config>" << std::endl
        << "        </namedConfigs>" << std::endl
        << std::endl
        << "        <produceRequestDataLimit value=\"100\" />" << std::endl
        << std::endl
        << "        <messageMaxBytes value=\"200\" />" << std::endl
        << std::endl
        << "        <combinedTopics enable=\"true\" config=\"config1\" />"
        << std::endl
        << std::endl
        << "        <defaultTopic action=\"perTopic\" config=\"config2\" />"
        << std::endl
        << std::endl
        << "        <topicConfigs>" << std::endl
        << "            <topic name=\"topic1\" action=\"perTopic\" "
        << "config=\"config1\" />" << std::endl
        << "            <topic name=\"topic2\" action=\"perTopic\" "
        << "config=\"config2\" />" << std::endl
        << "        </topicConfigs>" << std::endl
        << "    </batching>" << std::endl
        << std::endl
        << "    <compression>" << std::endl
        << "        <namedConfigs>" << std::endl
        << "            <config name=\"noComp\" type=\"none\" />" << std::endl
        << "            <config name=\"snappy1\" type=\"snappy\" "
        << "minSize=\"1024\" />" << std::endl
        << "            <config name=\"snappy2\" type=\"snappy\" "
        << "minSize=\"2k\" />" << std::endl
        << "            <config name=\"gzip1\" type=\"gzip\" "
        << "minSize=\"4096\" />" << std::endl
        << "            <config name=\"gzip2\" type=\"gzip\" level=\"3\" "
        << "minSize=\"8192\" />" << std::endl
        << "            <config name=\"lz4_1\" type=\"lz4\" "
        << "minSize=\"16384\" />" << std::endl
        << "            <config name=\"lz4_2\" type=\"lz4\" level=\"5\" "
        << "minSize=\"32768\" />" << std::endl
        << "        </namedConfigs>" << std::endl
        << std::endl
        << "        <sizeThresholdPercent value=\"75\" />" << std::endl
        << std::endl
        << "        <defaultTopic config=\"snappy1\" />" << std::endl
        << std::endl
        << "        <topicConfigs>" << std::endl
        << "            <topic name=\"topic1\" config=\"noComp\" />"
        << std::endl
        << "            <topic name=\"topic2\" config=\"snappy2\" />"
        << "            <topic name=\"topic3\" config=\"gzip1\" />"
        << "            <topic name=\"topic4\" config=\"gzip2\" />"
        << "            <topic name=\"topic5\" config=\"lz4_1\" />"
        << "            <topic name=\"topic6\" config=\"lz4_2\" />"
        << std::endl
        << "        </topicConfigs>" << std::endl
        << "    </compression>" << std::endl
        << std::endl
        << "    <topicRateLimiting>" << std::endl
        << "        <namedConfigs>" << std::endl
        << "            <config name=\"zero\" interval=\"1\" maxCount=\"0\" />"
        << std::endl
        << "            <config name=\"infinity\" interval=\"1\" "
        << "maxCount=\"unlimited\" />" << std::endl
        << "            <config name=\"config1\" interval=\"10000\" "
        << "maxCount=\"500\" />" << std::endl
        << "            <config name=\"config2\" interval=\"20000\" "
        << "maxCount=\"4k\" />" << std::endl
        << "        </namedConfigs>" << std::endl
        << "" << std::endl
        << "        <defaultTopic config=\"config1\" />" << std::endl
        << "" << std::endl
        << "        <topicConfigs>" << std::endl
        << "            <topic name=\"topic1\" config=\"zero\" />" << std::endl
        << "            <topic name=\"topic2\" config=\"infinity\" />"
        << std::endl
        << "            <topic name=\"topic3\" config=\"config2\" />"
        << std::endl
        << "        </topicConfigs>" << std::endl
        << "    </topicRateLimiting>" << std::endl
        << std::endl
        << "    <initialBrokers>" << std::endl
        << "        <broker host=\"host1\" port=\"9092\" />" << std::endl
        << "        <broker host=\"host2\" port=\"9093\" />" << std::endl
        << "    </initialBrokers>" << std::endl
        << "</doryConfig>" << std::endl;
    ofs.close();
    TConf::TBuilder builder(true);
    TConf conf = builder.Build(tmp_file.GetName().c_str());

    const TBatchConf &batch_conf = conf.GetBatchConf();
    ASSERT_EQ(batch_conf.GetProduceRequestDataLimit(), 100U);
    ASSERT_EQ(batch_conf.GetMessageMaxBytes(), 200U);
    ASSERT_TRUE(batch_conf.CombinedTopicsBatchingIsEnabled());
    TBatchConf::TBatchValues values = batch_conf.GetCombinedTopicsConfig();
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 50U);
    ASSERT_TRUE(values.OptMsgCount.IsKnown());
    ASSERT_EQ(*values.OptMsgCount, 100U);
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 200U);
    ASSERT_TRUE(batch_conf.GetDefaultTopicAction() ==
        TBatchConf::TTopicAction::PerTopic);
    values = batch_conf.GetDefaultTopicConfig();
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 5U);
    ASSERT_FALSE(values.OptMsgCount.IsKnown());
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 20U * 1024U);

    const TBatchConf::TTopicMap &topic_map = batch_conf.GetTopicConfigs();
    ASSERT_EQ(topic_map.size(), 2U);

    auto iter = topic_map.find("topic1");
    ASSERT_TRUE(iter != topic_map.end());
    TBatchConf::TTopicConf topic_conf = iter->second;
    ASSERT_TRUE(topic_conf.Action == TBatchConf::TTopicAction::PerTopic);
    values = topic_conf.BatchValues;
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 50U);
    ASSERT_TRUE(values.OptMsgCount.IsKnown());
    ASSERT_EQ(*values.OptMsgCount, 100U);
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 200U);

    iter = topic_map.find("topic2");
    ASSERT_TRUE(iter != topic_map.end());
    topic_conf = iter->second;
    ASSERT_TRUE(topic_conf.Action == TBatchConf::TTopicAction::PerTopic);
    values = topic_conf.BatchValues;
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 5U);
    ASSERT_FALSE(values.OptMsgCount.IsKnown());
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 20U * 1024U);

    const TCompressionConf &compression_conf = conf.GetCompressionConf();
    ASSERT_EQ(compression_conf.GetSizeThresholdPercent(), 75U);
    const TCompressionConf::TConf &default_topic_compression_conf =
        compression_conf.GetDefaultTopicConfig();
    ASSERT_TRUE(default_topic_compression_conf.Type ==
                TCompressionType::Snappy);
    ASSERT_EQ(default_topic_compression_conf.MinSize, 1024U);
    ASSERT_TRUE(default_topic_compression_conf.Level.IsUnknown());
    const TCompressionConf::TTopicMap &compression_topic_configs =
        compression_conf.GetTopicConfigs();
    ASSERT_EQ(compression_topic_configs.size(), 6U);

    TCompressionConf::TTopicMap::const_iterator comp_topic_iter =
        compression_topic_configs.find("topic1");
    ASSERT_TRUE(comp_topic_iter != compression_topic_configs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::None);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 0U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = compression_topic_configs.find("topic2");
    ASSERT_TRUE(comp_topic_iter != compression_topic_configs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Snappy);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 2048U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = compression_topic_configs.find("topic3");
    ASSERT_TRUE(comp_topic_iter != compression_topic_configs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Gzip);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 4096U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = compression_topic_configs.find("topic4");
    ASSERT_TRUE(comp_topic_iter != compression_topic_configs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Gzip);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 8192U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsKnown());
    ASSERT_EQ(*comp_topic_iter->second.Level, 3);

    comp_topic_iter = compression_topic_configs.find("topic5");
    ASSERT_TRUE(comp_topic_iter != compression_topic_configs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Lz4);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 16384U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = compression_topic_configs.find("topic6");
    ASSERT_TRUE(comp_topic_iter != compression_topic_configs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Lz4);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 32768U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsKnown());
    ASSERT_EQ(*comp_topic_iter->second.Level, 5);

    const TTopicRateConf &topic_rate_conf = conf.GetTopicRateConf();
    const TTopicRateConf::TConf &default_topic_rate_conf =
        topic_rate_conf.GetDefaultTopicConfig();
    ASSERT_EQ(default_topic_rate_conf.Interval, 10000U);
    ASSERT_TRUE(default_topic_rate_conf.MaxCount.IsKnown());
    ASSERT_EQ(*default_topic_rate_conf.MaxCount, 500U);
    const TTopicRateConf::TTopicMap &topic_rate_configs =
        topic_rate_conf.GetTopicConfigs();
    ASSERT_EQ(topic_rate_configs.size(), 3U);
    TTopicRateConf::TTopicMap::const_iterator rate_topic_iter =
        topic_rate_configs.find("topic1");
    ASSERT_TRUE(rate_topic_iter != topic_rate_configs.end());
    ASSERT_EQ(rate_topic_iter->second.Interval, 1U);
    ASSERT_TRUE(rate_topic_iter->second.MaxCount.IsKnown());
    ASSERT_EQ(*rate_topic_iter->second.MaxCount, 0U);
    rate_topic_iter = topic_rate_configs.find("topic2");
    ASSERT_TRUE(rate_topic_iter != topic_rate_configs.end());
    ASSERT_EQ(rate_topic_iter->second.Interval, 1U);
    ASSERT_TRUE(rate_topic_iter->second.MaxCount.IsUnknown());
    rate_topic_iter = topic_rate_configs.find("topic3");
    ASSERT_TRUE(rate_topic_iter != topic_rate_configs.end());
    ASSERT_EQ(rate_topic_iter->second.Interval, 20000U);
    ASSERT_TRUE(rate_topic_iter->second.MaxCount.IsKnown());
    ASSERT_EQ(*rate_topic_iter->second.MaxCount, 4096U);

    const std::vector<TConf::TBroker> &broker_vec = conf.GetInitialBrokers();
    ASSERT_EQ(broker_vec.size(), 2U);
    ASSERT_EQ(broker_vec[0].Host, "host1");
    ASSERT_EQ(broker_vec[0].Port, 9092U);
    ASSERT_EQ(broker_vec[1].Host, "host2");
    ASSERT_EQ(broker_vec[1].Port, 9093U);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
