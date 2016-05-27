/* <dory/stream_client_handler.test.cc>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

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

   Unit test for <dory/stream_client_handler.h>.
 */

#include <dory/stream_client_handler.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <base/field_access.h>
#include <base/thrower.h>
#include <base/time_util.h>
#include <base/tmp_file_name.h>
#include <capped/blob.h>
#include <capped/pool.h>
#include <capped/reader.h>
#include <dory/anomaly_tracker.h>
#include <dory/client/dory_client.h>
#include <dory/client/unix_stream_sender.h>
#include <dory/config.h>
#include <dory/debug/debug_setup.h>
#include <dory/discard_file_logger.h>
#include <dory/kafka_proto/choose_proto.h>
#include <dory/kafka_proto/wire_protocol.h>
#include <dory/metadata_timestamp.h>
#include <dory/msg_state_tracker.h>
#include <dory/stream_client_handler.h>
#include <dory/test_util/misc_util.h>
#include <server/unix_stream_server.h>
#include <thread/gate.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Client;
using namespace Dory::Debug;
using namespace Dory::KafkaProto;
using namespace Dory::TestUtil;
using namespace Server;
using namespace Thread;

namespace {

  struct TDoryConfig {
    private:
    bool DoryStarted;

    public:
    DEFINE_ERROR(TStartFailure, std::runtime_error,
        "Failed to start UNIX stream input agent");

    using TWorkerPool = TStreamClientHandler::TWorkerPool;

    TTmpFileName UnixSocketName;

    std::vector<const char *> Args;

    std::unique_ptr<TConfig> Cfg;

    std::unique_ptr<TWireProtocol> Protocol;

    TPool Pool;

    TDiscardFileLogger DiscardFileLogger;

    TAnomalyTracker AnomalyTracker;

    TMsgStateTracker MsgStateTracker;

    TMetadataTimestamp MetadataTimestamp;

    TDebugSetup DebugSetup;

    std::unique_ptr<TGate<TMsg::TPtr>> OutputQueue;

    std::unique_ptr<TWorkerPool> StreamClientWorkerPool;

    std::unique_ptr<TUnixStreamServer> UnixStreamServer;

    explicit TDoryConfig(size_t pool_block_size);

    ~TDoryConfig() noexcept {
      StopDory();
    }

    TStreamClientHandler *CreateStreamClientHandler() {
      assert(this);
      return new TStreamClientHandler(false, *Cfg, Pool, MsgStateTracker,
          AnomalyTracker, *OutputQueue, *StreamClientWorkerPool);
    }

    void StartDory() {
      if (!DoryStarted) {
        StreamClientWorkerPool->Start();

        if (!UnixStreamServer->SyncStart()) {
          THROW_ERROR(TStartFailure);
        }

        DoryStarted = true;
      }
    }

    void StopDory() {
      if (DoryStarted) {
        TUnixStreamServer &unix_stream_server = *UnixStreamServer;
        unix_stream_server.RequestShutdown();
        unix_stream_server.Join();
        StreamClientWorkerPool->RequestShutdown();
        StreamClientWorkerPool->WaitForShutdown();
        DoryStarted = false;
      }
    }
  };  // TDoryConfig

  static inline size_t
  ComputeBlockCount(size_t max_buffer_kb, size_t block_size) {
    return std::max<size_t>(1, (1024 * max_buffer_kb) / block_size);
  }

  TDoryConfig::TDoryConfig(size_t pool_block_size)
      : DoryStarted(false),
        Pool(pool_block_size, ComputeBlockCount(1, pool_block_size),
             TPool::TSync::Mutexed),
        AnomalyTracker(DiscardFileLogger, 0,
                       std::numeric_limits<size_t>::max()),
        DebugSetup("/unused/path", TDebugSetup::MAX_LIMIT,
                   TDebugSetup::MAX_LIMIT) {
    Args.push_back("dory");
    Args.push_back("--config_path");
    Args.push_back("/nonexistent/path");
    Args.push_back("--msg_buffer_max");
    Args.push_back("1");  /* this is 1 * 1024 bytes, not 1 byte */
    Args.push_back("--receive_stream_socket_name");
    Args.push_back(UnixSocketName);
    Args.push_back(nullptr);
    Cfg.reset(
        new TConfig(Args.size() - 1, const_cast<char **>(&Args[0]), true));
    Protocol.reset(ChooseProto(Cfg->ProtocolVersion, Cfg->RequiredAcks,
                   static_cast<int32_t>(Cfg->ReplicationTimeout)));
    OutputQueue.reset(new TGate<TMsg::TPtr>);
    StreamClientWorkerPool.reset(new TWorkerPool(
        [](const char *msg) {
          std::cerr << "Stream client worker pool fatal error: " << msg
              << std::endl;
          ASSERT_TRUE(false);
        }));
    UnixStreamServer.reset(new TUnixStreamServer(16,
        Cfg->ReceiveStreamSocketName, CreateStreamClientHandler(),
        [](const char *msg) {
          std::cerr << "UNIX stream server fatal error: " << msg
              << std::endl;
          ASSERT_TRUE(false);
        }));
  }

  static void MakeDg(std::vector<uint8_t> &dg, const std::string &topic,
      const std::string &body) {
    size_t dg_size = 0;
    int ret = dory_find_any_partition_msg_size(topic.size(), 0,
            body.size(), &dg_size);
    ASSERT_EQ(ret, DORY_OK);
    dg.resize(dg_size);
    ret = dory_write_any_partition_msg(&dg[0], dg.size(), topic.c_str(),
            GetEpochMilliseconds(), nullptr, 0, body.data(), body.size());
    ASSERT_EQ(ret, DORY_OK);
  }

  /* The fixture for testing class TStreamClientHandler. */
  class TStreamClientHandlerTest : public ::testing::Test {
    protected:
    TStreamClientHandlerTest() {
    }

    virtual ~TStreamClientHandlerTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TStreamClientHandlerTest

  TEST_F(TStreamClientHandlerTest, SuccessfulForwarding) {
    /* If this value is set too large, message(s) will be discarded and the
       test will fail. */
    const size_t pool_block_size = 256;

    TDoryConfig conf(pool_block_size);
    TGate<TMsg::TPtr> &output_queue = *conf.OutputQueue;

    try {
      conf.StartDory();
    } catch (const TDoryConfig::TStartFailure &) {
      ASSERT_TRUE(false);
    }

    TUnixStreamSender sender(conf.UnixSocketName);

    try {
      sender.PrepareToSend();
    } catch (const std::exception &x) {
      std::cerr << "Failed to connect to Dory for sending: " << x.what()
          << std::endl;
      ASSERT_TRUE(false);
    }

    std::vector<std::string> topics;
    std::vector<std::string> bodies;
    topics.push_back("topic1");
    bodies.push_back("Scooby");
    topics.push_back("topic2");
    bodies.push_back("Shaggy");
    topics.push_back("topic3");
    bodies.push_back("Velma");
    topics.push_back("topic4");
    bodies.push_back("Daphne");
    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);

      try {
        sender.Send(&dg_buf[0], dg_buf.size());
      } catch (const std::exception &x) {
        std::cerr << "Failed to send message to Dory: " << x.what()
            << std::endl;
        ASSERT_TRUE(false);
      }
    }

    std::list<TMsg::TPtr> msg_list;
    const Base::TFd &msg_available_fd = output_queue.GetMsgAvailableFd();

    while (msg_list.size() < 4) {
      if (!msg_available_fd.IsReadable(30000)) {
        ASSERT_TRUE(false);
        break;
      }

      msg_list.splice(msg_list.end(), output_queue.Get());
    }

    ASSERT_EQ(msg_list.size(), 4U);
    size_t i = 0;

    for (std::list<TMsg::TPtr>::iterator iter = msg_list.begin();
         iter != msg_list.end();
         ++i, ++iter) {
      TMsg::TPtr &msg_ptr = *iter;

      /* Prevent spurious assertion failure in msg dtor. */
      SetProcessed(msg_ptr);

      ASSERT_EQ(msg_ptr->GetTopic(), topics[i]);
      ASSERT_TRUE(ValueEquals(msg_ptr, bodies[i]));
    }

    TAnomalyTracker::TInfo bad_stuff;
    conf.AnomalyTracker.GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    msg_list.clear();
  }

  TEST_F(TStreamClientHandlerTest, NoBufferSpaceDiscard) {
    /* This setting must be chosen properly, since it determines how many
       messages will be discarded. */
    const size_t pool_block_size = 256;

    TDoryConfig conf(pool_block_size);
    TGate<TMsg::TPtr> &output_queue = *conf.OutputQueue;

    try {
      conf.StartDory();
    } catch (const TDoryConfig::TStartFailure &) {
      ASSERT_TRUE(false);
    }

    TUnixStreamSender sender(conf.UnixSocketName);

    try {
      sender.PrepareToSend();
    } catch (const std::exception &x) {
      std::cerr << "Failed to connect to Dory for sending: " << x.what()
          << std::endl;
      ASSERT_TRUE(false);
    }

    std::vector<std::string> topics;
    std::vector<std::string> bodies;
    topics.push_back("topic1");
    bodies.push_back("Scooby");
    topics.push_back("topic2");
    bodies.push_back("Shaggy");
    topics.push_back("topic3");
    bodies.push_back("Velma");
    topics.push_back("topic4");
    bodies.push_back("Daphne");

    /* Fred gets discarded due to the buffer space cap. */
    topics.push_back("topic5");
    bodies.push_back("Fred");

    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);

      try {
        sender.Send(&dg_buf[0], dg_buf.size());
      } catch (const std::exception &x) {
        std::cerr << "Failed to send message to Dory: " << x.what()
            << std::endl;
        ASSERT_TRUE(false);
      }
    }

    std::list<TMsg::TPtr> msg_list;
    const Base::TFd &msg_available_fd = output_queue.GetMsgAvailableFd();

    while (msg_list.size() < 4) {
      if (!msg_available_fd.IsReadable(30000)) {
        ASSERT_TRUE(false);
        break;
      }

      msg_list.splice(msg_list.end(), output_queue.Get());
    }

    for (size_t i = 0; i < 3000; ++i) {
      TAnomalyTracker::TInfo bad_stuff;
      conf.AnomalyTracker.GetInfo(bad_stuff);

      if (!bad_stuff.DiscardTopicMap.empty()) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_EQ(msg_list.size(), 4U);
    size_t i = 0;

    for (std::list<TMsg::TPtr>::iterator iter = msg_list.begin();
         iter != msg_list.end();
         ++i, ++iter) {
      TMsg::TPtr &msg_ptr = *iter;

      /* Prevent spurious assertion failure in msg dtor. */
      SetProcessed(msg_ptr);

      ASSERT_EQ(msg_ptr->GetTopic(), topics[i]);
      ASSERT_TRUE(ValueEquals(msg_ptr, bodies[i]));
    }

    TAnomalyTracker::TInfo bad_stuff;
    conf.AnomalyTracker.GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 1U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.begin()->first, topics[4]);
    const TAnomalyTracker::TTopicInfo &discard_info =
        bad_stuff.DiscardTopicMap.begin()->second;
    ASSERT_EQ(discard_info.Count, 1U);
    msg_list.clear();
  }

  TEST_F(TStreamClientHandlerTest, MalformedMessageDiscards) {
    /* If this value is set too large, message(s) will be discarded and the
       test will fail. */
    const size_t pool_block_size = 256;

    TDoryConfig conf(pool_block_size);
    TGate<TMsg::TPtr> &output_queue = *conf.OutputQueue;

    try {
      conf.StartDory();
    } catch (const TDoryConfig::TStartFailure &) {
      ASSERT_TRUE(false);
    }

    /* This message will get discarded because it's malformed. */
    std::string topic("scooby_doo");
    std::string msg_body("I like scooby snacks");

    TUnixStreamSender sender(conf.UnixSocketName);

    try {
      sender.PrepareToSend();
    } catch (const std::exception &x) {
      std::cerr << "Failed to connect to Dory for sending: " << x.what()
          << std::endl;
      ASSERT_TRUE(false);
    }

    std::vector<uint8_t> dg_buf;
    MakeDg(dg_buf, topic, msg_body);

    /* Overwrite the size field with an incorrect value. */
    ASSERT_GE(dg_buf.size(), sizeof(int32_t));
    WriteInt32ToHeader(&dg_buf[0], dg_buf.size() - 1);

    try {
      sender.Send(&dg_buf[0], dg_buf.size());
    } catch (const std::exception &x) {
      std::cerr << "Failed to send message to Dory: " << x.what() << std::endl;
      ASSERT_TRUE(false);
    }

    for (size_t i = 0; i < 3000; ++i) {
      TAnomalyTracker::TInfo bad_stuff;
      conf.AnomalyTracker.GetInfo(bad_stuff);

      if (bad_stuff.MalformedMsgCount) {
        break;
      }

      SleepMilliseconds(10);
    }

    std::list<TMsg::TPtr> msg_list(output_queue.NonblockingGet());
    ASSERT_TRUE(msg_list.empty());
    TAnomalyTracker::TInfo bad_stuff;
    conf.AnomalyTracker.GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 1U);
    ASSERT_TRUE(bad_stuff.BadTopics.empty());
    msg_list.clear();
  }

}  // namespace
