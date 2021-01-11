/* <dory/batch/single_topic_batcher.test.cc>

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

   Unit test for <dory/batch/single_topic_batcher.h>
 */

#include <dory/batch/single_topic_batcher.h>

#include <optional>
#include <string>

#include <base/tmp_file.h>
#include <dory/msg.h>
#include <dory/msg_creator.h>
#include <dory/test_util/misc_util.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Batch;
using namespace Dory::TestUtil;
using namespace ::TestUtil;

namespace {

  /* The fixture for testing class TSingleTopicBatcher. */
  class TSingleTopicBatcherTest : public ::testing::Test {
    protected:
    TSingleTopicBatcherTest() = default;

    ~TSingleTopicBatcherTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TSingleTopicBatcherTest

  TEST_F(TSingleTopicBatcherTest, Test1) {
    TTestMsgCreator mc;  // create this first since it contains buffer pool
    TBatchConfig config;
    TSingleTopicBatcher batcher(config);
    const TBatchConfig &cfg = batcher.GetConfig();
    ASSERT_EQ(cfg.TimeLimit, config.TimeLimit);
    ASSERT_EQ(cfg.MsgCount, config.MsgCount);
    ASSERT_EQ(cfg.ByteCount, config.ByteCount);
    ASSERT_FALSE(batcher.BatchingIsEnabled());
    ASSERT_TRUE(batcher.IsEmpty());
    auto opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    TMsg::TPtr msg = mc.NewMsg("Bugs Bunny", "Elmer Fudd", 100);
    ASSERT_TRUE(!!msg);
    std::list<TMsg::TPtr> msg_list =
        SetProcessed(batcher.AddMsg(std::move(msg), 100));
    ASSERT_TRUE(!!msg);
    SetProcessed(msg);
    ASSERT_TRUE(msg_list.empty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg_list = batcher.TakeBatch();
    ASSERT_TRUE(msg_list.empty());
  }

  TEST_F(TSingleTopicBatcherTest, Test2) {
    TTestMsgCreator mc;  // create this first since it contains buffer pool
    TBatchConfig config(100, 0, 0);
    TSingleTopicBatcher batcher(config);
    ASSERT_TRUE(batcher.BatchingIsEnabled());
    ASSERT_TRUE(batcher.IsEmpty());
    TMsg::TPtr msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    std::list<TMsg::TPtr> msg_list =
        SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    auto opt_ts = batcher.GetNextCompleteTime();
    ASSERT_TRUE(opt_ts.has_value());
    ASSERT_EQ(*opt_ts, 100);
    msg = mc.NewMsg("Bugs Bunny", "wabbits", *opt_ts);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 99));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_TRUE(opt_ts.has_value());
    ASSERT_EQ(*opt_ts, 100);
    msg = mc.NewMsg("Bugs Bunny", "wabbits", *opt_ts);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 100));
    ASSERT_FALSE(!!msg);
    ASSERT_EQ(msg_list.size(), 3U);
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    ASSERT_TRUE(batcher.IsEmpty());
  }

  TEST_F(TSingleTopicBatcherTest, Test3) {
    TTestMsgCreator mc;  // create this first since it contains buffer pool
    TBatchConfig config(0, 3, 0);
    TSingleTopicBatcher batcher(config);
    ASSERT_TRUE(batcher.BatchingIsEnabled());
    ASSERT_TRUE(batcher.IsEmpty());
    TMsg::TPtr msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    std::list<TMsg::TPtr> msg_list =
        SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    auto opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_EQ(msg_list.size(), 3U);
    ASSERT_TRUE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg_list = SetProcessed(batcher.TakeBatch());
    ASSERT_EQ(msg_list.size(), 1U);
    ASSERT_TRUE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
  }

  TEST_F(TSingleTopicBatcherTest, Test4) {
    TTestMsgCreator mc;
    std::string msg_body("wabbits");
    TBatchConfig config(0, 0, 3 * msg_body.size());
    TSingleTopicBatcher batcher(config);
    ASSERT_TRUE(batcher.BatchingIsEnabled());
    ASSERT_TRUE(batcher.IsEmpty());
    TMsg::TPtr msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    std::list<TMsg::TPtr> msg_list =
        SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    auto opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_EQ(msg_list.size(), 3U);
    ASSERT_TRUE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 5));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
    msg_list = SetProcessed(batcher.TakeBatch());
    ASSERT_EQ(msg_list.size(), 1U);
    ASSERT_TRUE(batcher.IsEmpty());
    opt_ts = batcher.GetNextCompleteTime();
    ASSERT_FALSE(opt_ts.has_value());
  }

  TEST_F(TSingleTopicBatcherTest, Test5) {
    TTestMsgCreator mc;  // create this first since it contains buffer pool
    TBatchConfig config(10, 3, 8);
    TSingleTopicBatcher batcher(config);
    ASSERT_TRUE(batcher.BatchingIsEnabled());
    ASSERT_TRUE(batcher.IsEmpty());
    TMsg::TPtr msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    std::list<TMsg::TPtr> msg_list =
        SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "x", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_EQ(msg_list.size(), 2U);
    ASSERT_TRUE(ValueEquals(msg_list.front(), "wabbits"));
    msg_list.pop_front();
    ASSERT_TRUE(ValueEquals(msg_list.front(), "x"));
    ASSERT_TRUE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "xx", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_EQ(msg_list.size(), 1U);
    ASSERT_TRUE(ValueEquals(msg_list.front(), "wabbits"));
    ASSERT_FALSE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "y", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 10));
    ASSERT_TRUE(!!msg);
    SetProcessed(msg);
    ASSERT_EQ(msg_list.size(), 2U);
    ASSERT_TRUE(ValueEquals(msg_list.front(), "xx"));
    msg_list.pop_front();
    ASSERT_TRUE(ValueEquals(msg_list.front(), "y"));
    ASSERT_TRUE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "wabbits", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "12345678", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_TRUE(!!msg);
    SetProcessed(msg);
    ASSERT_TRUE(batcher.IsEmpty());
    ASSERT_EQ(msg_list.size(), 1U);
    ASSERT_TRUE(ValueEquals(msg_list.front(), "wabbits"));
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_TRUE(!!msg);
    SetProcessed(msg);
    ASSERT_TRUE(batcher.IsEmpty());
    ASSERT_TRUE(msg_list.empty());
  }

  TEST_F(TSingleTopicBatcherTest, Test6) {
    TTestMsgCreator mc;  // create this first since it contains buffer pool

    /* Configure a size limit of 2 bytes, with no time or message count limits.
       Then batch two empty messages.  The size of an empty message is counted
       as 1 byte to prevent batching an infinite number of empty messages.
       Therefore the batcher should accept the first message, and then return
       both messages when we batch the second one. */
    TBatchConfig config(0, 0, 2);
    TSingleTopicBatcher batcher(config);
    ASSERT_TRUE(batcher.BatchingIsEnabled());
    ASSERT_TRUE(batcher.IsEmpty());
    TMsg::TPtr msg = mc.NewMsg("Bugs Bunny ", "", 0);
    std::list<TMsg::TPtr> msg_list =
        SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_TRUE(msg_list.empty());
    ASSERT_FALSE(batcher.IsEmpty());
    msg = mc.NewMsg("Bugs Bunny", "", 0);
    msg_list = SetProcessed(batcher.AddMsg(std::move(msg), 0));
    ASSERT_FALSE(!!msg);
    ASSERT_EQ(msg_list.size(), 2U);
    ASSERT_TRUE(batcher.IsEmpty());
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
