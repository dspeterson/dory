/* <dory/kafka_proto/metadata/v0/metadata_response.test.cc>

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

   Unit tests for <dory/kafka_proto/metadata/v0/metadata_response_reader.h> and
   <dory/kafka_proto/metadata/v0/metadata_response_writer.h>
 */

#include <dory/kafka_proto/metadata/v0/metadata_response_reader.h>
#include <dory/kafka_proto/metadata/v0/metadata_response_writer.h>

#include <string>

#include <base/tmp_file.h>
#include <dory/kafka_proto/request_response.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Metadata::V0;
using namespace ::TestUtil;

namespace {

  /* The fixture for testing classes TMetadataResponseReader and
     TMetadataResponseWriter. */
  class TMetadataResponseTest : public ::testing::Test {
    protected:
    TMetadataResponseTest() = default;

    ~TMetadataResponseTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TMetadataResponseTest

  void WriteMetadataResponse(std::vector<uint8_t> &response_buf,
      const std::string broker_names[], const std::string topic_names[],
      size_t broker_count, size_t topic_count, size_t partition_count,
      size_t replica_count, size_t caught_up_replica_count) {
    TMetadataResponseWriter writer;
    writer.OpenResponse(response_buf, 12345);
    writer.OpenBrokerList();

    for (int32_t broker = 0;
        broker < static_cast<int32_t>(broker_count);
        ++broker) {
      const std::string& broker_name = broker_names[broker];
      writer.AddBroker(broker, broker_name.data(),
          broker_name.data() + broker_name.size(), broker + 50);
    }

    writer.CloseBrokerList();
    writer.OpenTopicList();

    for (size_t topic = 0; topic < topic_count; ++topic) {
      const std::string& topic_name = topic_names[topic];
      writer.OpenTopic(static_cast<int16_t>(topic + 100), topic_name.data(),
          topic_name.data() + topic_name.size());
      writer.OpenPartitionList();

      for (size_t partition = 0; partition < partition_count; ++partition) {
        writer.OpenPartition(static_cast<int16_t>(partition + 150),
            static_cast<int16_t>(partition + 200),
            static_cast<int16_t>(partition + 250));
        writer.OpenReplicaList();

        for (size_t replica = 0; replica < replica_count; ++replica) {
          writer.AddReplica(static_cast<int32_t>(replica + 300));
        }

        writer.CloseReplicaList();
        writer.OpenCaughtUpReplicaList();

        for (size_t caught_up_replica = 0;
            caught_up_replica < caught_up_replica_count;
            ++caught_up_replica) {
          writer.AddCaughtUpReplica(static_cast<int32_t>(caught_up_replica + 350));
        }

        writer.CloseCaughtUpReplicaList();
        writer.ClosePartition();
      }

      writer.ClosePartitionList();
      writer.CloseTopic();
    }

    writer.CloseTopicList();
    writer.CloseResponse();
  }

  void ReadMetadataResponse(const std::vector<uint8_t> & response_buf,
      const std::string broker_names[], const std::string topic_names[],
      size_t broker_count, size_t topic_count, size_t partition_count,
      size_t replica_count, size_t caught_up_replica_count) {
    ASSERT_GE(response_buf.size(), REQUEST_OR_RESPONSE_SIZE_SIZE);
    ASSERT_GE(response_buf.size(), TMetadataResponseReader::MinSize());
    ASSERT_EQ(response_buf.size(), GetRequestOrResponseSize(&response_buf[0]));
    TMetadataResponseReader reader(&response_buf[0], response_buf.size());
    ASSERT_EQ(reader.GetCorrelationId(), 12345);
    ASSERT_EQ(reader.GetBrokerCount(), broker_count);
    std::string broker_host;
    std::string topic_name;

    for (size_t broker = 0; broker < broker_count; ++broker) {
      ASSERT_TRUE(reader.NextBroker());
      ASSERT_EQ(static_cast<size_t>(reader.GetCurrentBrokerNodeId()), broker);
      broker_host.assign(reader.GetCurrentBrokerHostBegin(),
          reader.GetCurrentBrokerHostEnd());
      ASSERT_EQ(broker_host, broker_names[broker]);
      ASSERT_EQ(static_cast<size_t>(reader.GetCurrentBrokerPort()),
          broker + 50);
    }

    ASSERT_FALSE(reader.NextBroker());
    ASSERT_EQ(reader.GetTopicCount(), topic_count);

    for (size_t topic = 0; topic < topic_count; ++topic) {
      ASSERT_TRUE(reader.NextTopic());
      ASSERT_EQ(static_cast<size_t>(reader.GetCurrentTopicErrorCode()),
          topic + 100);
      topic_name.assign(reader.GetCurrentTopicNameBegin(),
          reader.GetCurrentTopicNameEnd());
      ASSERT_EQ(topic_name, topic_names[topic]);
      ASSERT_EQ(reader.GetCurrentTopicPartitionCount(), partition_count);

      for (size_t partition = 0; partition < partition_count; ++partition) {
        ASSERT_TRUE(reader.NextPartitionInTopic());
        ASSERT_EQ(static_cast<size_t>(reader.GetCurrentPartitionErrorCode()),
            partition + 150);
        ASSERT_EQ(static_cast<size_t>(reader.GetCurrentPartitionId()),
            partition + 200);
        ASSERT_EQ(static_cast<size_t>(reader.GetCurrentPartitionLeaderId()),
            partition + 250);
        ASSERT_EQ(
            static_cast<size_t>(reader.GetCurrentPartitionReplicaCount()),
            replica_count);

        for (size_t replica = 0; replica < replica_count; ++replica) {
          ASSERT_TRUE(reader.NextReplicaInPartition());
          ASSERT_EQ(static_cast<size_t>(reader.GetCurrentReplicaNodeId()),
              replica + 300);
        }

        ASSERT_FALSE(reader.NextReplicaInPartition());
        ASSERT_EQ(static_cast<size_t>(
            reader.GetCurrentPartitionCaughtUpReplicaCount()),
            caught_up_replica_count);

        for (size_t caught_up_replica = 0;
            caught_up_replica < caught_up_replica_count;
            ++caught_up_replica) {
          ASSERT_TRUE(reader.NextCaughtUpReplicaInPartition());
          ASSERT_EQ(
              static_cast<size_t>(reader.GetCurrentCaughtUpReplicaNodeId()),
              caught_up_replica + 350);
        }

        ASSERT_FALSE(reader.NextCaughtUpReplicaInPartition());
      }

      ASSERT_FALSE(reader.NextPartitionInTopic());
    }

    ASSERT_FALSE(reader.NextTopic());
  }

  TEST_F(TMetadataResponseTest, Test1) {
    const std::string broker_names[] = {
      "scooby doo",
      "shaggy"
    };

    const std::string topic_names[] = {
      "velma",
      "daphne"
    };

    std::vector<uint8_t> response_buf;

    for (size_t broker_count = 0; broker_count <= 2; ++broker_count) {
      for (size_t topic_count = 0; topic_count <= 2; ++topic_count) {
        size_t partition_max = topic_count ? 2 : 0;

        for (size_t partition_count = 0;
            partition_count <= partition_max;
            ++partition_count) {
          size_t replica_max = (topic_count && partition_count) ? 2 : 0;

          for (size_t replica_count = 0;
              replica_count <= replica_max;
              ++replica_count) {
            for (size_t caught_up_replica_count = 0;
                caught_up_replica_count <= replica_max;
                ++caught_up_replica_count) {
              WriteMetadataResponse(response_buf, broker_names, topic_names,
                  broker_count, topic_count, partition_count,
                  replica_count, caught_up_replica_count);
              ReadMetadataResponse(response_buf, broker_names, topic_names,
                  broker_count, topic_count, partition_count,
                  replica_count, caught_up_replica_count);
            }
          }
        }
      }
    }
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
