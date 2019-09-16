/* <dory/metadata.test.cc>

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

   Unit test for <dory/metadata.h>.
 */

#include <memory>
#include <unordered_set>

#include <dory/metadata.h>
#include <dory/util/misc_util.h>
#include <log_util/init_logging.h>

#include <gtest/gtest.h>

using namespace Dory;
using namespace Dory::Util;
using namespace LogUtil;

namespace {

  int FindBrokerIndex(const std::vector<TMetadata::TBroker> &brokers,
      int32_t broker_id) {
    int i = 0;

    for (; static_cast<size_t>(i) < brokers.size(); ++i) {
      if (brokers[i].GetId() == broker_id) {
        return i;
      }
    }

    return -1;
  }

  /* The fixture for testing class TMetadata. */
  class TMetadataTest : public ::testing::Test {
    protected:
    TMetadataTest() = default;

    ~TMetadataTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TMetadataTest

  TEST_F(TMetadataTest, Test1) {
    TMetadata::TBuilder builder;
    std::unique_ptr<TMetadata> md(builder.Build());
    ASSERT_TRUE(!!md);
    ASSERT_TRUE(md->GetBrokers().empty());
    ASSERT_TRUE(md->GetTopics().empty());
    ASSERT_EQ(md->FindTopicIndex("blah"), -1);
    ASSERT_TRUE(md->SanityCheck());
    ASSERT_EQ(md->NumInServiceBrokers(), 0U);
    std::unique_ptr<TMetadata> md2(builder.Build());
    ASSERT_TRUE(*md2 == *md);
  }

  TEST_F(TMetadataTest, Test2) {
    TMetadata::TBuilder builder;
    builder.OpenBrokerList();
    builder.AddBroker(5, "host1", 101);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(6, 5, true, 9);
    builder.AddPartitionToTopic(3, 2, true, 0);
    builder.AddPartitionToTopic(7, 2, false, 5);  // out of service partition
    builder.AddPartitionToTopic(4, 5, true, 0);
    builder.AddPartitionToTopic(1, 7, false, 6);  // out of service partition
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(8, 3, true, 0);
    builder.AddPartitionToTopic(6, 5, true, 9);
    builder.AddPartitionToTopic(3, 3, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md(builder.Build());
    ASSERT_TRUE(!!md);
    ASSERT_TRUE(md->SanityCheck());
    ASSERT_EQ(md->NumInServiceBrokers(), 3U);
    const std::vector<TMetadata::TTopic> &topics = md->GetTopics();
    const std::vector<TMetadata::TBroker> &brokers = md->GetBrokers();
    ASSERT_EQ(md->GetBrokers().size(), 4U);
    ASSERT_EQ(topics.size(), 3U);

    for (const TMetadata::TBroker &b : brokers) {
      ASSERT_EQ((b.GetId() != 7), b.IsInService());
    }

    ASSERT_EQ(md->FindTopicIndex("blah"), -1);
    int topic1_index = md->FindTopicIndex("topic1");
    ASSERT_GE(topic1_index, 0);
    const TMetadata::TTopic &topic1 = topics[topic1_index];
    const std::vector<TMetadata::TPartition> &topic1_ok_partitions =
        topic1.GetOkPartitions();
    ASSERT_EQ(topic1_ok_partitions.size(), 3U);
    size_t i = 0;

    for (i = 0; i < topic1_ok_partitions.size(); ++i) {
      if (topic1_ok_partitions[i].GetId() == 6) {
        break;
      }
    }

    ASSERT_LT(i, topic1_ok_partitions.size());
    ASSERT_EQ(brokers[topic1_ok_partitions[i].GetBrokerIndex()].GetId(), 5);
    ASSERT_EQ(topic1_ok_partitions[i].GetErrorCode(), 9);

    for (i = 0; i < topic1_ok_partitions.size(); ++i) {
      if (topic1_ok_partitions[i].GetId() == 3) {
        break;
      }
    }

    ASSERT_LT(i, topic1_ok_partitions.size());
    ASSERT_EQ(brokers[topic1_ok_partitions[i].GetBrokerIndex()].GetId(), 2);
    ASSERT_EQ(topic1_ok_partitions[i].GetErrorCode(), 0);

    for (i = 0; i < topic1_ok_partitions.size(); ++i) {
      if (topic1_ok_partitions[i].GetId() == 4) {
        break;
      }
    }

    ASSERT_LT(i, topic1_ok_partitions.size());
    ASSERT_EQ(brokers[topic1_ok_partitions[i].GetBrokerIndex()].GetId(), 5);
    ASSERT_EQ(topic1_ok_partitions[i].GetErrorCode(), 0);

    const std::vector<TMetadata::TPartition> &topic1_bad_partitions =
        topic1.GetOutOfServicePartitions();
    ASSERT_EQ(topic1_bad_partitions.size(), 2U);

    for (i = 0; i < topic1_bad_partitions.size(); ++i) {
      if (topic1_bad_partitions[i].GetId() == 7) {
        break;
      }
    }

    ASSERT_LT(i, topic1_bad_partitions.size());
    ASSERT_EQ(brokers[topic1_bad_partitions[i].GetBrokerIndex()].GetId(), 2);
    ASSERT_EQ(topic1_bad_partitions[i].GetErrorCode(), 5);

    for (i = 0; i < topic1_bad_partitions.size(); ++i) {
      if (topic1_bad_partitions[i].GetId() == 1) {
        break;
      }
    }

    ASSERT_LT(i, topic1_bad_partitions.size());
    ASSERT_EQ(brokers[topic1_bad_partitions[i].GetBrokerIndex()].GetId(), 7);
    ASSERT_EQ(topic1_bad_partitions[i].GetErrorCode(), 6);

    const std::vector<TMetadata::TPartition> &topic1_all_partitions =
        topic1.GetAllPartitions();
    ASSERT_EQ(topic1_all_partitions.size(), 5U);
    ASSERT_EQ(topic1_all_partitions[0].GetId(), 1);
    ASSERT_EQ(topic1_all_partitions[1].GetId(), 3);
    ASSERT_EQ(topic1_all_partitions[2].GetId(), 4);
    ASSERT_EQ(topic1_all_partitions[3].GetId(), 6);
    ASSERT_EQ(topic1_all_partitions[4].GetId(), 7);

    int topic2_index = md->FindTopicIndex("topic2");
    ASSERT_GE(topic2_index, 0);
    const TMetadata::TTopic &topic2 = topics[topic2_index];
    ASSERT_TRUE(topic2.GetOkPartitions().empty());
    ASSERT_TRUE(topic2.GetOutOfServicePartitions().empty());
    ASSERT_TRUE(topic2.GetAllPartitions().empty());
    int topic3_index = md->FindTopicIndex("topic3");
    ASSERT_GE(topic3_index, 0);
    const TMetadata::TTopic &topic3 = topics[topic3_index];
    const std::vector<TMetadata::TPartition> &topic3_ok_partitions =
        topic3.GetOkPartitions();
    ASSERT_EQ(topic3_ok_partitions.size(), 3U);
    ASSERT_TRUE(topic3.GetOutOfServicePartitions().empty());

    for (i = 0; i < topic3_ok_partitions.size(); ++i) {
      if (topic3_ok_partitions[i].GetId() == 8) {
        break;
      }
    }

    ASSERT_LT(i, topic3_ok_partitions.size());
    ASSERT_EQ(brokers[topic3_ok_partitions[i].GetBrokerIndex()].GetId(), 3);
    ASSERT_EQ(topic3_ok_partitions[i].GetErrorCode(), 0);

    for (i = 0; i < topic3_ok_partitions.size(); ++i) {
      if (topic3_ok_partitions[i].GetId() == 6) {
        break;
      }
    }

    ASSERT_LT(i, topic3_ok_partitions.size());
    ASSERT_EQ(brokers[topic3_ok_partitions[i].GetBrokerIndex()].GetId(), 5);
    ASSERT_EQ(topic3_ok_partitions[i].GetErrorCode(), 9);

    for (i = 0; i < topic3_ok_partitions.size(); ++i) {
      if (topic3_ok_partitions[i].GetId() == 3) {
        break;
      }
    }

    ASSERT_LT(i, topic3_ok_partitions.size());
    ASSERT_EQ(brokers[topic3_ok_partitions[i].GetBrokerIndex()].GetId(), 3);
    ASSERT_EQ(topic3_ok_partitions[i].GetErrorCode(), 0);

    const std::vector<TMetadata::TPartition> &topic3_all_partitions =
        topic3.GetAllPartitions();
    ASSERT_EQ(topic3_all_partitions.size(), 3U);
    ASSERT_EQ(topic3_all_partitions[0].GetId(), 3);
    ASSERT_EQ(topic3_all_partitions[1].GetId(), 6);
    ASSERT_EQ(topic3_all_partitions[2].GetId(), 8);

    int index = FindBrokerIndex(brokers, 3);
    ASSERT_GE(index, 0);
    size_t num_choices = 100;
    const int32_t *choices = md->FindPartitionChoices("topic1",
        static_cast<size_t>(index), num_choices);
    ASSERT_TRUE(choices == nullptr);
    ASSERT_EQ(num_choices, 0U);
    index = FindBrokerIndex(brokers, 5);
    ASSERT_GE(index, 0);
    num_choices = 100;
    choices = md->FindPartitionChoices("topic1", static_cast<size_t>(index),
        num_choices);
    ASSERT_TRUE(choices != nullptr);
    ASSERT_EQ(num_choices, 2U);
    std::unordered_set<int32_t> choice_set;

    for (i = 0; i < num_choices; ++i) {
      choice_set.insert(choices[i]);
    }

    std::unordered_set<int32_t> expected_choice_set({ 6, 4 });
    ASSERT_TRUE(choice_set == expected_choice_set);
    index = FindBrokerIndex(brokers, 2);
    ASSERT_GE(index, 0);
    num_choices = 100;
    choices = md->FindPartitionChoices("topic1", static_cast<size_t>(index),
        num_choices);
    ASSERT_TRUE(choices != nullptr);
    ASSERT_EQ(num_choices, 1U);
    choice_set.clear();

    for (i = 0; i < num_choices; ++i) {
      choice_set.insert(choices[i]);
    }

    expected_choice_set.clear();
    expected_choice_set.insert(3);
    ASSERT_TRUE(choice_set == expected_choice_set);
    index = FindBrokerIndex(brokers, 3);
    ASSERT_GE(index, 0);
    num_choices = 100;
    choices = md->FindPartitionChoices("topic2", static_cast<size_t>(index),
        num_choices);
    ASSERT_TRUE(choices == nullptr);
    ASSERT_EQ(num_choices, 0U);
    index = FindBrokerIndex(brokers, 3);
    ASSERT_GE(index, 0);
    num_choices = 100;
    choices = md->FindPartitionChoices("topic3", static_cast<size_t>(index),
        num_choices);
    ASSERT_TRUE(choices != nullptr);
    ASSERT_EQ(num_choices, 2U);
    choice_set.clear();

    for (i = 0; i < num_choices; ++i) {
      choice_set.insert(choices[i]);
    }

    expected_choice_set.clear();
    expected_choice_set.insert(8);
    expected_choice_set.insert(3);
    ASSERT_TRUE(choice_set == expected_choice_set);
    index = FindBrokerIndex(brokers, 5);
    ASSERT_GE(index, 0);
    num_choices = 100;
    choices = md->FindPartitionChoices("topic3", static_cast<size_t>(index),
        num_choices);
    ASSERT_TRUE(choices != nullptr);
    ASSERT_EQ(num_choices, 1U);
    choice_set.clear();

    for (i = 0; i < num_choices; ++i) {
      choice_set.insert(choices[i]);
    }

    expected_choice_set.clear();
    expected_choice_set.insert(6);
    ASSERT_TRUE(choice_set == expected_choice_set);
  }

  TEST_F(TMetadataTest, Test3) {
    TMetadata::TBuilder builder;
    builder.OpenBrokerList();
    builder.AddBroker(5, "host1", 101);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(6, 5, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.AddPartitionToTopic(4, 5, false, 7);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, true, 0);
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md1(builder.Build());
    ASSERT_TRUE(!!md1);
    ASSERT_TRUE(md1->SanityCheck());

    builder.OpenBrokerList();
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(5, "host1", 101);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(4, 5, false, 7);
    builder.AddPartitionToTopic(6, 5, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md2(builder.Build());
    ASSERT_TRUE(!!md2);
    ASSERT_TRUE(md2->SanityCheck());

    ASSERT_TRUE(*md1 == *md2);
    ASSERT_TRUE(*md2 == *md1);
    ASSERT_FALSE(*md1 != *md2);
    ASSERT_FALSE(*md2 != *md1);

    builder.OpenBrokerList();
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(1, "host1", 101);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(4, 1, false, 7);
    builder.AddPartitionToTopic(6, 1, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 1, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md3(builder.Build());
    ASSERT_TRUE(!!md3);
    ASSERT_TRUE(md3->SanityCheck());

    ASSERT_FALSE(*md3 == *md2);

    builder.OpenBrokerList();
    builder.AddBroker(7, "blah", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(5, "host1", 101);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(4, 5, false, 7);
    builder.AddPartitionToTopic(6, 5, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md4(builder.Build());
    ASSERT_TRUE(!!md4);
    ASSERT_TRUE(md4->SanityCheck());

    ASSERT_FALSE(*md4 == *md2);

    builder.OpenBrokerList();
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(5, "host1", 105);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(4, 5, false, 7);
    builder.AddPartitionToTopic(6, 5, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md5(builder.Build());
    ASSERT_TRUE(!!md5);
    ASSERT_TRUE(md5->SanityCheck());

    ASSERT_FALSE(*md5 == *md2);

    builder.OpenBrokerList();
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(5, "host1", 101);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(1, 5, false, 7);
    builder.AddPartitionToTopic(6, 5, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md6(builder.Build());
    ASSERT_TRUE(!!md6);
    ASSERT_TRUE(md6->SanityCheck());

    ASSERT_FALSE(*md6 == *md2);

    builder.OpenBrokerList();
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(5, "host1", 101);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(4, 5, false, 7);
    builder.AddPartitionToTopic(6, 2, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md7(builder.Build());
    ASSERT_TRUE(!!md7);
    ASSERT_TRUE(md7->SanityCheck());

    ASSERT_FALSE(*md7 == *md2);

    builder.OpenBrokerList();
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(5, "host1", 101);
    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(4, 5, false, 7);
    builder.AddPartitionToTopic(6, 5, false, 4);
    builder.AddPartitionToTopic(3, 2, false, 8);
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic2"));
    builder.CloseTopic();
    ASSERT_TRUE(builder.OpenTopic("topic3"));
    builder.AddPartitionToTopic(3, 3, false, 6);
    builder.AddPartitionToTopic(8, 3, true, 9);
    builder.AddPartitionToTopic(6, 5, false, 5);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md8(builder.Build());
    ASSERT_TRUE(!!md8);
    ASSERT_TRUE(md8->SanityCheck());

    ASSERT_FALSE(*md8 == *md2);
  }

  TEST_F(TMetadataTest, Test4) {
    TMetadata::TBuilder builder;
    builder.OpenBrokerList();
    builder.AddBroker(5, "host1", 101);
    builder.AddBroker(2, "host2", 102);
    builder.AddBroker(7, "host3", 103);
    builder.AddBroker(3, "host4", 104);

    /* The builder should ignore this attempt to add a duplicate broker ID. */
    builder.AddBroker(3, "host5", 104);

    builder.CloseBrokerList();
    ASSERT_TRUE(builder.OpenTopic("topic1"));
    builder.AddPartitionToTopic(6, 5, true, 0);

    /* The builder should ignore this attempt to add a duplicate partition. */
    builder.AddPartitionToTopic(6, 2, true, 0);

    /* The builder should ignore this attempt to add a partition with an
       unknown broker ID. */
    builder.AddPartitionToTopic(3, 1, true, 0);

    builder.CloseTopic();

    /* The builder should reject an attempt to add a duplicate topic. */
    ASSERT_FALSE(builder.OpenTopic("topic1"));

    /* We should still be able to add another topic even though our attempt to
       add a duplicate topic was rejected. */
    ASSERT_TRUE(builder.OpenTopic("topic2"));

    builder.AddPartitionToTopic(2, 7, true, 0);
    builder.CloseTopic();
    std::unique_ptr<TMetadata> md(builder.Build());

    /* Make sure the metadata is correct. */

    const auto &broker_vec = md->GetBrokers();
    ASSERT_EQ(broker_vec.size(), 4U);
    ASSERT_EQ(md->NumInServiceBrokers(), 2U);
    std::unordered_set<int32_t> id_set;

    for (const auto &broker : broker_vec) {
      id_set.insert(broker.GetId());
    }

    ASSERT_EQ(id_set.size(), 4U);
    ASSERT_FALSE(id_set.find(5) == id_set.end());
    ASSERT_FALSE(id_set.find(2) == id_set.end());
    ASSERT_FALSE(id_set.find(7) == id_set.end());
    ASSERT_FALSE(id_set.find(3) == id_set.end());
    const auto &topic_vec = md->GetTopics();
    ASSERT_EQ(topic_vec.size(), 2U);

    int index = md->FindTopicIndex("topic1");
    ASSERT_GE(index, 0);
    ASSERT_LT(index, 2);
    const TMetadata::TTopic &topic_1 = topic_vec[index];
    ASSERT_TRUE(topic_1.GetOutOfServicePartitions().empty());
    const auto &topic_1_ok_partitions = topic_1.GetOkPartitions();
    const auto &topic_1_all_partitions = topic_1.GetOkPartitions();
    ASSERT_EQ(topic_1_ok_partitions.size(), 1U);
    ASSERT_EQ(topic_1_all_partitions.size(), 1U);
    ASSERT_EQ(topic_1_ok_partitions[0].GetId(), 6);
    ASSERT_EQ(topic_1_all_partitions[0].GetId(), 6);

    index = md->FindTopicIndex("topic2");
    ASSERT_GE(index, 0);
    ASSERT_LT(index, 2);
    const TMetadata::TTopic &topic_2 = topic_vec[index];
    ASSERT_TRUE(topic_2.GetOutOfServicePartitions().empty());
    const auto &topic_2_ok_partitions = topic_2.GetOkPartitions();
    const auto &topic_2_all_partitions = topic_2.GetOkPartitions();
    ASSERT_EQ(topic_2_ok_partitions.size(), 1U);
    ASSERT_EQ(topic_2_all_partitions.size(), 1U);
    ASSERT_EQ(topic_2_ok_partitions[0].GetId(), 2);
    ASSERT_EQ(topic_2_all_partitions[0].GetId(), 2);
  }

}  // namespace

int main(int argc, char **argv) {
  InitTestLogging(argv[0], std::string() /* file_path */);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
