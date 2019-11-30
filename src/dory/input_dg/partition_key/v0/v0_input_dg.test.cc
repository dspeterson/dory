/* <dory/input_dg/partition_key/v0/v0_input_dg.test.cc>

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

   Unit test for <dory/input_dg/input_dg_util.h>,
   <dory/input_dg/partition_key/v0/v0_write_dg.h>, and
   <dory/input_dg/partition_key/v0/v0_write_msg.h>.
 */

#include <dory/input_dg/input_dg_util.h>
#include <dory/input_dg/partition_key/v0/v0_write_msg.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <base/tmp_file.h>
#include <capped/pool.h>
#include <capped/reader.h>
#include <dory/anomaly_tracker.h>
#include <dory/client/status_codes.h>
#include <dory/msg.h>
#include <dory/msg_state_tracker.h>
#include <dory/test_util/misc_util.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::InputDg;
using namespace Dory::TestUtil;
using namespace ::TestUtil;

namespace {

  struct TTestConfig {
    std::unique_ptr<TPool> Pool;

    TDiscardFileLogger DiscardFileLogger;

    TAnomalyTracker AnomalyTracker;

    TMsgStateTracker MsgStateTracker;

    TTestConfig();
  };  // TTestConfig

  TTestConfig::TTestConfig()
      : Pool(new TPool(128, 16384, TPool::TSync::Mutexed)),
        AnomalyTracker(DiscardFileLogger, 0,
                       std::numeric_limits<size_t>::max()) {
  }

  /* The fixture for testing reading/writing of v0 PartitionKey input
     datagrams. */
  class TV0InputDgTest : public ::testing::Test {
    protected:
    TV0InputDgTest() = default;

    ~TV0InputDgTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TV0InputDgTest

  TEST_F(TV0InputDgTest, Test1) {
    TTestConfig cfg;
    int64_t timestamp = 8675309;
    int32_t partition_key = 0xabcd1234;
    std::string topic("dumb jokes");
    std::string key("Why did the chicken cross the road?");
    std::string value("Because he got bored writing unit tests.");
    std::vector<uint8_t> buf;
    size_t dg_size = 0;
    int result = input_dg_p_key_v0_compute_msg_size(&dg_size, topic.size(),
        key.size(), value.size());
    ASSERT_EQ(result, DORY_OK);
    buf.resize(dg_size);
    input_dg_p_key_v0_write_msg(&buf[0], timestamp, partition_key,
        topic.data(), topic.data() + topic.size(), key.data(),
        key.data() + key.size(), value.data(), value.data() + value.size());
    TMsg::TPtr msg = BuildMsgFromDg(&buf[0], buf.size(),
        false /* log_discard */, *cfg.Pool, cfg.AnomalyTracker,
        cfg.MsgStateTracker);
    ASSERT_TRUE(!!msg);
    SetProcessed(msg);
    ASSERT_EQ(msg->GetTimestamp(), timestamp);
    ASSERT_EQ(msg->GetTopic(), topic);
    ASSERT_EQ(msg->GetPartitionKey(), partition_key);
    ASSERT_TRUE(KeyEquals(msg, key));
    ASSERT_TRUE(ValueEquals(msg, value));
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
