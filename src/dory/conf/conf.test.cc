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

#include <sstream>

#include <base/tmp_file.h>
#include <dory/compress/compression_type.h>
#include <log/file_log_writer.h>
#include <log/pri.h>
#include <test_util/test_logging.h>
#include <xml/test/xml_test_initializer.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Conf;
using namespace Log;
using namespace ::TestUtil;
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

  TEST_F(TConfTest, BasicTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
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
        << "    <inputSources>" << std::endl
        << "        <unixDatagram enable=\"true\">" << std::endl
        << "            <path value=\"/var/run/dory/input_d\" />" << std::endl
        << "            <mode value=\"0200\" />" << std::endl
        << "        </unixDatagram>" << std::endl
        << "        <unixStream enable=\"true\">" << std::endl
        << "            <path value=\"/var/run/dory/input_s\" />" << std::endl
        << "            <mode value=\"0020\" />" << std::endl
        << "        </unixStream>" << std::endl
        << "        <tcp enable=\"true\">" << std::endl
        << "            <port value=\"54321\" />" << std::endl
        << "        </tcp>" << std::endl
        << "    </inputSources>" << std::endl
        << std::endl
        << "<inputConfig>" << std::endl
        << "    <maxBuffer value=\"16k\" />" << std::endl
        << "    <maxDatagramMsgSize value=\"32k\" />" << std::endl
        << "    <allowLargeUnixDatagrams value=\"true\" />" << std::endl
        << "    <maxStreamMsgSize value=\"3m\" />" << std::endl
        << "</inputConfig>" << std::endl
        << std::endl
        << "<msgDelivery>" << std::endl
        << "    <topicAutocreate enable=\"true\" />" << std::endl
        << "    <maxFailedDeliveryAttempts value=\"7\" />" << std::endl
        << "    <shutdownMaxDelay value=\"15\" />" << std::endl
        << "    <dispatcherRestartMaxDelay value=\"8000\" />" << std::endl
        << "    <metadataRefreshInterval value=\"25\" />" << std::endl
        << "    <compareMetadataOnRefresh value=\"false\" />" << std::endl
        << "    <kafkaSocketTimeout value=\"75\" />" << std::endl
        << "    <pauseRateLimitInitial value=\"6500\" />" << std::endl
        << "    <pauseRateLimitMaxDouble value=\"3\" />" << std::endl
        << "    <minPauseDelay value=\"4500\" />" << std::endl
        << "</msgDelivery>" << std::endl
        << std::endl
        << "<httpInterface>" << std::endl
        << "    <port value=\"3456\" />" << std::endl
        << "    <loopbackOnly value=\"true\" />" << std::endl
        << "    <discardReportInterval value=\"750\" />" << std::endl
        << "    <badMsgPrefixSize value=\"512\" />" << std::endl
        << "</httpInterface>" << std::endl
        << std::endl
        << "<discardLogging enable=\"true\">" << std::endl
        << "    <path value=\"/discard/logging/path\" />" << std::endl
        << "    <maxFileSize value=\"2m\" />" << std::endl
        << "    <maxArchiveSize value=\"64m\" />" << std::endl
        << "    <badMsgPrefixSize value=\"384\" />" << std::endl
        << "</discardLogging>" << std::endl
        << std::endl
        << "<kafkaConfig>" << std::endl
        << "    <clientId value=\"test client\" />" << std::endl
        << "    <replicationTimeout value=\"9000\" />" << std::endl
        << "</kafkaConfig>" << std::endl
        << std::endl
        << "<msgDebug enable=\"true\">" << std::endl
        << "    <path value=\"/msg/debug/path\" />" << std::endl
        << "    <timeLimit value=\"45\" />" << std::endl
        << "    <byteLimit value=\"512m\" />" << std::endl
        << "</msgDebug>" << std::endl
        << std::endl
        << "<logging>" << std::endl
        << "    <level value=\"INFO\" />" << std::endl
        << "    <stdoutStderr enable=\"true\" />" << std::endl
        << "    <syslog enable=\"false\" />" << std::endl
        << "    <file enable=\"true\">" << std::endl
        << "        <path value=\"/log/file/path\" />" << std::endl
        << "        <mode value=\"0664\" />" << std::endl
        << "    </file>" << std::endl
        << "    <logDiscards enable=\"false\" />" << std::endl
        << "</logging>" << std::endl
        << std::endl
        << "    <initialBrokers>" << std::endl
        << "        <broker host=\"host1\" port=\"9092\" />" << std::endl
        << "        <broker host=\"host2\" port=\"9093\" />" << std::endl
        << "    </initialBrokers>" << std::endl
        << "</doryConfig>" << std::endl;
    TConf::TBuilder builder(true);
    TConf conf = builder.Build(os.str());

    ASSERT_EQ(conf.BatchConf.ProduceRequestDataLimit, 100U);
    ASSERT_EQ(conf.BatchConf.MessageMaxBytes, 200U);
    ASSERT_TRUE(conf.BatchConf.CombinedTopicsBatchingEnabled);
    TBatchConf::TBatchValues values = conf.BatchConf.CombinedTopicsConfig;
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 50U);
    ASSERT_TRUE(values.OptMsgCount.IsKnown());
    ASSERT_EQ(*values.OptMsgCount, 100U);
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 200U);
    ASSERT_TRUE(conf.BatchConf.DefaultTopicAction ==
        TBatchConf::TTopicAction::PerTopic);
    values = conf.BatchConf.DefaultTopicConfig;
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 5U);
    ASSERT_FALSE(values.OptMsgCount.IsKnown());
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 20U * 1024U);

    ASSERT_EQ(conf.BatchConf.TopicConfigs.size(), 2U);

    auto iter = conf.BatchConf.TopicConfigs.find("topic1");
    ASSERT_TRUE(iter != conf.BatchConf.TopicConfigs.end());
    TBatchConf::TTopicConf topic_conf = iter->second;
    ASSERT_TRUE(topic_conf.Action == TBatchConf::TTopicAction::PerTopic);
    values = topic_conf.BatchValues;
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 50U);
    ASSERT_TRUE(values.OptMsgCount.IsKnown());
    ASSERT_EQ(*values.OptMsgCount, 100U);
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 200U);

    iter = conf.BatchConf.TopicConfigs.find("topic2");
    ASSERT_TRUE(iter != conf.BatchConf.TopicConfigs.end());
    topic_conf = iter->second;
    ASSERT_TRUE(topic_conf.Action == TBatchConf::TTopicAction::PerTopic);
    values = topic_conf.BatchValues;
    ASSERT_TRUE(values.OptTimeLimit.IsKnown());
    ASSERT_EQ(*values.OptTimeLimit, 5U);
    ASSERT_FALSE(values.OptMsgCount.IsKnown());
    ASSERT_TRUE(values.OptByteCount.IsKnown());
    ASSERT_EQ(*values.OptByteCount, 20U * 1024U);

    ASSERT_EQ(conf.CompressionConf.SizeThresholdPercent, 75U);
    ASSERT_TRUE(conf.CompressionConf.DefaultTopicConfig.Type ==
                TCompressionType::Snappy);
    ASSERT_EQ(conf.CompressionConf.DefaultTopicConfig.MinSize, 1024U);
    ASSERT_TRUE(conf.CompressionConf.DefaultTopicConfig.Level.IsUnknown());
    ASSERT_EQ(conf.CompressionConf.TopicConfigs.size(), 6U);

    TCompressionConf::TTopicMap::const_iterator comp_topic_iter =
        conf.CompressionConf.TopicConfigs.find("topic1");
    ASSERT_TRUE(comp_topic_iter != conf.CompressionConf.TopicConfigs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::None);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 0U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = conf.CompressionConf.TopicConfigs.find("topic2");
    ASSERT_TRUE(comp_topic_iter != conf.CompressionConf.TopicConfigs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Snappy);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 2048U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = conf.CompressionConf.TopicConfigs.find("topic3");
    ASSERT_TRUE(comp_topic_iter != conf.CompressionConf.TopicConfigs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Gzip);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 4096U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = conf.CompressionConf.TopicConfigs.find("topic4");
    ASSERT_TRUE(comp_topic_iter != conf.CompressionConf.TopicConfigs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Gzip);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 8192U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsKnown());
    ASSERT_EQ(*comp_topic_iter->second.Level, 3);

    comp_topic_iter = conf.CompressionConf.TopicConfigs.find("topic5");
    ASSERT_TRUE(comp_topic_iter != conf.CompressionConf.TopicConfigs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Lz4);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 16384U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsUnknown());

    comp_topic_iter = conf.CompressionConf.TopicConfigs.find("topic6");
    ASSERT_TRUE(comp_topic_iter != conf.CompressionConf.TopicConfigs.end());
    ASSERT_TRUE(comp_topic_iter->second.Type == TCompressionType::Lz4);
    ASSERT_TRUE(comp_topic_iter->second.MinSize == 32768U);
    ASSERT_TRUE(comp_topic_iter->second.Level.IsKnown());
    ASSERT_EQ(*comp_topic_iter->second.Level, 5);

    ASSERT_EQ(conf.TopicRateConf.DefaultTopicConfig.Interval, 10000U);
    ASSERT_TRUE(conf.TopicRateConf.DefaultTopicConfig.MaxCount.IsKnown());
    ASSERT_EQ(*conf.TopicRateConf.DefaultTopicConfig.MaxCount, 500U);
    ASSERT_EQ(conf.TopicRateConf.TopicConfigs.size(), 3U);
    TTopicRateConf::TTopicMap::const_iterator rate_topic_iter =
        conf.TopicRateConf.TopicConfigs.find("topic1");
    ASSERT_TRUE(rate_topic_iter != conf.TopicRateConf.TopicConfigs.end());
    ASSERT_EQ(rate_topic_iter->second.Interval, 1U);
    ASSERT_TRUE(rate_topic_iter->second.MaxCount.IsKnown());
    ASSERT_EQ(*rate_topic_iter->second.MaxCount, 0U);
    rate_topic_iter = conf.TopicRateConf.TopicConfigs.find("topic2");
    ASSERT_TRUE(rate_topic_iter != conf.TopicRateConf.TopicConfigs.end());
    ASSERT_EQ(rate_topic_iter->second.Interval, 1U);
    ASSERT_TRUE(rate_topic_iter->second.MaxCount.IsUnknown());
    rate_topic_iter = conf.TopicRateConf.TopicConfigs.find("topic3");
    ASSERT_TRUE(rate_topic_iter != conf.TopicRateConf.TopicConfigs.end());
    ASSERT_EQ(rate_topic_iter->second.Interval, 20000U);
    ASSERT_TRUE(rate_topic_iter->second.MaxCount.IsKnown());
    ASSERT_EQ(*rate_topic_iter->second.MaxCount, 4096U);

    ASSERT_EQ(conf.InputSourcesConf.UnixDgPath, "/var/run/dory/input_d");
    ASSERT_EQ(conf.InputSourcesConf.UnixDgMode, 0200);
    ASSERT_EQ(conf.InputSourcesConf.UnixStreamPath, "/var/run/dory/input_s");
    ASSERT_EQ(conf.InputSourcesConf.UnixStreamMode, 0020);
    ASSERT_TRUE(conf.InputSourcesConf.LocalTcpPort.IsKnown());
    ASSERT_EQ(*conf.InputSourcesConf.LocalTcpPort, 54321U);

    ASSERT_EQ(conf.InputConfigConf.MaxBuffer, 16U * 1024U);
    ASSERT_EQ(conf.InputConfigConf.MaxDatagramMsgSize, 32U * 1024U);
    ASSERT_TRUE(conf.InputConfigConf.AllowLargeUnixDatagrams);
    ASSERT_EQ(conf.InputConfigConf.MaxStreamMsgSize, 3U * 1024U * 1024U);

    ASSERT_TRUE(conf.MsgDeliveryConf.TopicAutocreate);
    ASSERT_EQ(conf.MsgDeliveryConf.MaxFailedDeliveryAttempts, 7U);
    ASSERT_EQ(conf.MsgDeliveryConf.ShutdownMaxDelay, 15U);
    ASSERT_EQ(conf.MsgDeliveryConf.DispatcherRestartMaxDelay, 8000U);
    ASSERT_EQ(conf.MsgDeliveryConf.MetadataRefreshInterval, 25U);
    ASSERT_FALSE(conf.MsgDeliveryConf.CompareMetadataOnRefresh);
    ASSERT_EQ(conf.MsgDeliveryConf.KafkaSocketTimeout, 75U);
    ASSERT_EQ(conf.MsgDeliveryConf.PauseRateLimitInitial, 6500U);
    ASSERT_EQ(conf.MsgDeliveryConf.PauseRateLimitMaxDouble, 3U);
    ASSERT_EQ(conf.MsgDeliveryConf.MinPauseDelay, 4500U);

    ASSERT_EQ(conf.HttpInterfaceConf.Port, 3456U);
    ASSERT_TRUE(conf.HttpInterfaceConf.LoopbackOnly);
    ASSERT_EQ(conf.HttpInterfaceConf.DiscardReportInterval, 750U);
    ASSERT_EQ(conf.HttpInterfaceConf.BadMsgPrefixSize, 512U);

    ASSERT_EQ(conf.DiscardLoggingConf.Path, "/discard/logging/path");
    ASSERT_EQ(conf.DiscardLoggingConf.MaxFileSize, 2U * 1024U * 1024U);
    ASSERT_EQ(conf.DiscardLoggingConf.MaxArchiveSize, 64U * 1024U * 1024U);
    ASSERT_EQ(conf.DiscardLoggingConf.BadMsgPrefixSize, 384U);

    ASSERT_EQ(conf.KafkaConfigConf.ClientId, "test client");
    ASSERT_EQ(conf.KafkaConfigConf.ReplicationTimeout, 9000U);

    ASSERT_EQ(conf.MsgDebugConf.Path, "/msg/debug/path");
    ASSERT_EQ(conf.MsgDebugConf.TimeLimit, 45U);
    ASSERT_EQ(conf.MsgDebugConf.ByteLimit, 512U * 1024U * 1024U);

    ASSERT_EQ(conf.LoggingConf.Pri, TPri::INFO);
    ASSERT_TRUE(conf.LoggingConf.EnableStdoutStderr);
    ASSERT_FALSE(conf.LoggingConf.EnableSyslog);
    ASSERT_EQ(conf.LoggingConf.FilePath, "/log/file/path");
    ASSERT_EQ(conf.LoggingConf.FileMode, 0664);

    ASSERT_EQ(conf.InitialBrokers.size(), 2U);
    ASSERT_EQ(conf.InitialBrokers[0].Host, "host1");
    ASSERT_EQ(conf.InitialBrokers[0].Port, 9092U);
    ASSERT_EQ(conf.InitialBrokers[1].Host, "host2");
    ASSERT_EQ(conf.InitialBrokers[1].Port, 9093U);
  }

  TEST_F(TConfTest, BasicLoggingTest) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
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
        << "    <logging>" << std::endl
        << "        <level value=\"INFO\" />" << std::endl
        << "        <stdoutStderr enable=\"true\" />" << std::endl
        << "        <syslog enable=\"false\" />" << std::endl
        << std::endl
        << "        <file enable=\"true\">" << std::endl
        << "            <path value=\"/var/log/dory/dory.log\" />" << std::endl
        << "            <mode value=\"0666\" />" << std::endl
        << "" << std::endl
        << "        </file>" << std::endl
        << "    </logging>" << std::endl
        << std::endl
        << "    <initialBrokers>" << std::endl
        << "        <broker host=\"host1\" port=\"9092\" />" << std::endl
        << "        <broker host=\"host2\" port=\"9093\" />" << std::endl
        << "    </initialBrokers>" << std::endl
        << "</doryConfig>" << std::endl;
    TConf::TBuilder builder(true);
    TConf conf = builder.Build(os.str());

    ASSERT_EQ(conf.LoggingConf.Pri, TPri::INFO);
    ASSERT_TRUE(conf.LoggingConf.EnableStdoutStderr);
    ASSERT_FALSE(conf.LoggingConf.EnableSyslog);
    ASSERT_EQ(conf.LoggingConf.FilePath, "/var/log/dory/dory.log");
    ASSERT_EQ(conf.LoggingConf.FileMode, 0666);
  }

  TEST_F(TConfTest, LoggingTestInvalidLevel) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
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
        << "    <logging>" << std::endl
        << "        <level value=\"BLAH\" />" << std::endl
        << "        <stdoutStderr enable=\"true\" />" << std::endl
        << "        <syslog enable=\"false\" />" << std::endl
        << std::endl
        << "        <file enable=\"true\">" << std::endl
        << "            <path value=\"/var/log/dory/dory.log\" />" << std::endl
        << "            <mode value=\"0666\" />" << std::endl
        << "" << std::endl
        << "        </file>" << std::endl
        << "    </logging>" << std::endl
        << std::endl
        << "    <initialBrokers>" << std::endl
        << "        <broker host=\"host1\" port=\"9092\" />" << std::endl
        << "        <broker host=\"host2\" port=\"9093\" />" << std::endl
        << "    </initialBrokers>" << std::endl
        << "</doryConfig>" << std::endl;
    TConf::TBuilder builder(true);
    bool caught = false;

    try {
      TConf conf = builder.Build(os.str());
    } catch (const TLoggingInvalidLevel &) {
      caught = true;
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TConfTest, LoggingTestRelativePath) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
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
        << "    <logging>" << std::endl
        << "        <stdoutStderr enable=\"true\" />" << std::endl
        << "        <syslog enable=\"false\" />" << std::endl
        << std::endl
        << "        <file enable=\"true\">" << std::endl
        << "            <path value=\"dory/dory.log\" />" << std::endl
        << "            <mode value=\"0666\" />" << std::endl
        << "" << std::endl
        << "        </file>" << std::endl
        << "    </logging>" << std::endl
        << std::endl
        << "    <initialBrokers>" << std::endl
        << "        <broker host=\"host1\" port=\"9092\" />" << std::endl
        << "        <broker host=\"host2\" port=\"9093\" />" << std::endl
        << "    </initialBrokers>" << std::endl
        << "</doryConfig>" << std::endl;
    TConf::TBuilder builder(true);
    bool caught = false;

    try {
      TConf conf = builder.Build(os.str());
    } catch (const TLoggingRelativePath &) {
      caught = true;
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TConfTest, LoggingTestInvalidMode) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
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
        << "    <logging>" << std::endl
        << "        <stdoutStderr enable=\"true\" />" << std::endl
        << "        <syslog enable=\"false\" />" << std::endl
        << std::endl
        << "        <file enable=\"true\">" << std::endl
        << "            <path value=\"/var/log/dory/dory.log\" />" << std::endl
        << "            <mode value=\"01000\" />" << std::endl
        << "" << std::endl
        << "        </file>" << std::endl
        << "    </logging>" << std::endl
        << std::endl
        << "    <initialBrokers>" << std::endl
        << "        <broker host=\"host1\" port=\"9092\" />" << std::endl
        << "        <broker host=\"host2\" port=\"9093\" />" << std::endl
        << "    </initialBrokers>" << std::endl
        << "</doryConfig>" << std::endl;
    TConf::TBuilder builder(true);
    bool caught = false;

    try {
      TConf conf = builder.Build(os.str());
    } catch (const TLoggingInvalidFileMode &) {
      caught = true;
    }

    ASSERT_TRUE(caught);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
