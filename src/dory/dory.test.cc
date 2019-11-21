/* <dory/dory.test.cc>

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

   End to end test for dory daemon using mock Kafka server.
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <netinet/in.h>

#include <base/field_access.h>
#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <base/time_util.h>
#include <base/tmp_file.h>
#include <dory/anomaly_tracker.h>
#include <dory/client/client_sender_base.h>
#include <dory/client/dory_client.h>
#include <dory/client/dory_client_socket.h>
#include <dory/client/status_codes.h>
#include <dory/client/tcp_sender.h>
#include <dory/client/unix_dg_sender.h>
#include <dory/client/unix_stream_sender.h>
#include <dory/compress/compression_type.h>
#include <dory/conf/conf.h>
#include <dory/dory_server.h>
#include <dory/kafka_proto/produce/version_util.h>
#include <dory/msg_state_tracker.h>
#include <dory/test_util/mock_kafka_config.h>
#include <dory/util/misc_util.h>
#include <log/pri.h>
#include <test_util/test_logging.h>
#include <thread/fd_managed_thread.h>
#include <xml/test/xml_test_initializer.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Capped;
using namespace Dory;
using namespace Dory::Client;
using namespace Dory::Compress;
using namespace Dory::Conf;
using namespace Dory::Debug;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::MockKafkaServer;
using namespace Dory::TestUtil;
using namespace Dory::Util;
using namespace Log;
using namespace ::TestUtil;
using namespace Thread;
using namespace Xml;
using namespace Xml::Test;

namespace {

  class TDoryTestServer final : private TFdManagedThread {
    NO_COPY_SEMANTICS(TDoryTestServer);

    public:
    TDoryTestServer(in_port_t broker_port, size_t msg_buffer_max_kb,
        const std::string &dory_conf)
        : BrokerPort(broker_port),
          MsgBufferMaxKb(msg_buffer_max_kb),
          DoryConf(dory_conf) {
    }

    TDoryTestServer(in_port_t broker_port, size_t msg_buffer_max_kb,
        std::string &&dory_conf)
        : BrokerPort(broker_port),
          MsgBufferMaxKb(msg_buffer_max_kb),
          DoryConf(std::move(dory_conf)) {
    }

    virtual ~TDoryTestServer();

    void UseUnixDgSocket() {
      assert(this);
      assert(!IsStarted());
      UnixDgSocketName = MakeTmpFilename("/tmp/dory_test_dg_sock.XXXXXX");
    }

    void UseUnixStreamSocket() {
      assert(this);
      assert(!IsStarted());
      UnixStreamSocketName =
          MakeTmpFilename("/tmp/dory_test_stream_sock.XXXXXX");
    }

    void UseTcpInputSocket() {
      assert(this);
      TcpInputActive = true;
    }

    const char *GetUnixDgSocketName() const {
      assert(this);
      return UnixDgSocketName.c_str();
    }

    const char *GetUnixStreamSocketName() const {
      assert(this);
      return UnixStreamSocketName.c_str();
    }

    in_port_t GetInputPort() const {
      assert(this);
      return Dory->GetInputPort();
    }

    /* Call this method to start the server. */
    bool SyncStart();

    /* This must not be called until SyncStart() has been called.  Returns
       pointer to dory server object, or nullptr on dory server initialization
       failure. */
    TDoryServer *GetDory() {
      assert(this);
      return Dory.get();
    }

    virtual void RequestShutdown() override;

    using TFdManagedThread::Join;

    int GetDoryReturnValue() const {
      assert(this);
      return DoryReturnValue;
    }

    protected:
    virtual void Run() override;

    private:
    std::string UnixDgSocketName;

    std::string UnixStreamSocketName;

    bool TcpInputActive = false;

    in_port_t BrokerPort;

    size_t MsgBufferMaxKb;

    std::string DoryConf;

    int DoryReturnValue = EXIT_FAILURE;

    std::unique_ptr<TDoryServer> Dory;
  };  // TDoryTestServer

  TDoryTestServer::~TDoryTestServer() {
    /* This will shut down the thread if something unexpected happens. */
    ShutdownOnDestroy();
  }

  bool TDoryTestServer::SyncStart() {
    assert(this);
    TTmpFile tmp_file("/tmp/dory_test_server.XXXXXX",
        true /* delete_on_destroy */);
    std::ofstream ofs(tmp_file.GetName());
    ofs << DoryConf;
    ofs.close();
    std::string msg_buffer_max_str = std::to_string(MsgBufferMaxKb);
    std::vector<const char *> args;
    args.push_back("dory");
    args.push_back("--config_path");
    args.push_back(tmp_file.GetName().c_str());
    args.push_back("--msg_buffer_max");
    args.push_back(msg_buffer_max_str.c_str());

    if (!UnixDgSocketName.empty()) {
      args.push_back("--receive_socket_name");
      args.push_back(UnixDgSocketName.c_str());
    }

    if (!UnixStreamSocketName.empty()) {
      args.push_back("--receive_stream_socket_name");
      args.push_back(UnixStreamSocketName.c_str());
    }

    if (TcpInputActive) {
      args.push_back("--input_port");
      args.push_back("0");  // 0 means "request ephemeral port"
    }

    args.push_back("--client_id");
    args.push_back("dory");
    args.push_back("--status_loopback_only");
    args.push_back(nullptr);

    try {
      bool large_sendbuf_required = false;
      auto dory_config = TDoryServer::CreateConfig( args.size() - 1, &args[0],
          large_sendbuf_required, true, true);
      Dory.reset(new TDoryServer(std::move(dory_config.first),
          std::move(dory_config.second), GetShutdownRequestedFd()));
      Start();
    } catch (const std::exception &x) {
      std::cerr << "Server error: " << x.what() << std::endl;
      return false;
    } catch (...) {
      std::cerr << "Unknown server error" << std::endl;
      return false;
    }

    if (!Dory->GetInitWaitFd().IsReadableIntr(30000)) {
      std::cerr << "Dory server failed to initialize after 30 seconds."
          << std::endl;
      return false;
    }

    return true;
  }

  void TDoryTestServer::RequestShutdown() {
    assert(this);
    Dory::Util::RequestShutdown();
  }

  void TDoryTestServer::Run() {
    assert(this);
    DoryReturnValue = EXIT_FAILURE;

    try {
      Dory->BindStatusSocket(true);
      DoryReturnValue = Dory->Run();
    } catch (const std::exception &x) {
      std::cerr << "Server error: " << x.what() << std::endl;
    } catch (...) {
      std::cerr << "Unknown server error" << std::endl;
    }
  }

   /* Create simple configuration with batching and compression disabled. */
  std::string CreateSimpleDoryConf(in_port_t broker_port) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
       << "<doryConfig>" << std::endl
       << "    <batching>" << std::endl
       << "        <produceRequestDataLimit value=\"0\" />" << std::endl
       << "        <messageMaxBytes value=\"1024k\" />" << std::endl
       << "        <combinedTopics enable=\"false\" />" << std::endl
       << "        <defaultTopic action=\"disable\" />" << std::endl
       << "    </batching>" << std::endl
       << "    <compression>" << std::endl
       << "        <namedConfigs>" << std::endl
       << "            <config name=\"noComp\" type=\"none\" />" << std::endl
       << "        </namedConfigs>" << std::endl
       << std::endl
       << "        <defaultTopic config=\"noComp\" />" << std::endl
       << "    </compression>" << std::endl
       << "    <initialBrokers>" << std::endl
       << "        <broker host=\"localhost\" port=\"" << broker_port <<"\" />"
       << std::endl
       << "    </initialBrokers>" << std::endl
       << "</doryConfig>" << std::endl;
    return os.str();
  }

  void CreateKafkaConfig(size_t num_brokers,
      const std::vector<std::string> &topic_vec, size_t partitions_per_topic,
      std::vector<std::string> &result) {
    result.clear();

    /* Add contents to a setup file for the mock Kafka server.  This line tells
       the server to simulate the given number of brokers on consecutive ports
       starting at 10000.  During the test, dory will connect to these ports
       and forward messages it gets from the UNIX domain socket. */
    std::string line("ports 10000 ");
    line += std::to_string(num_brokers);
    result.push_back(line);
    std::string num_partitions_str = std::to_string(partitions_per_topic);
    size_t i = 0;

    for (const auto &topic : topic_vec) {
      /* This line tells the mock Kafka server to create a topic with the given
         name, containing the given number of partitions.  The value following
         the partition count specifies that the first partition should be on
         the broker whose port is at the given offset from the starting port
         (10000 above).  The remaining partitions are distributed among the
         brokers in round-robin fashion on consecutive ports. */
      line = "topic ";
      line += topic;
      line += " ";
      line += num_partitions_str;
      size_t offset = i % num_brokers;
      ++i;
      line += " ";
      line += std::to_string(offset);
      result.push_back(line);
    }
  }

  void CreateKafkaConfig(size_t num_brokers, const std::string &topic,
      size_t num_partitions, std::vector<std::string> &result) {
    std::vector<std::string> topic_vec;
    topic_vec.push_back(topic);
    CreateKafkaConfig(num_brokers, topic_vec, num_partitions, result);
  }

  void MakeDg(std::vector<uint8_t> &dg, const std::string &topic,
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

  void MakeDg(std::vector<uint8_t> &dg, const std::string &topic,
      const std::string &key, const std::string &value) {
    size_t dg_size = 0;
    int ret = dory_find_any_partition_msg_size(topic.size(), key.size(),
            value.size(), &dg_size);
    ASSERT_EQ(ret, DORY_OK);
    dg.resize(dg_size);
    ret = dory_write_any_partition_msg(&dg[0], dg.size(), topic.c_str(),
            GetEpochMilliseconds(), key.data(), key.size(), value.data(),
            value.size());
    ASSERT_EQ(ret, DORY_OK);
  }

  void GetKeyAndValue(TDoryServer &dory,
      Dory::MockKafkaServer::TMainThread &mock_kafka,
      const std::string &topic, const std::string &key,
      const std::string &value, size_t expected_ack_count,
      size_t expected_msg_count,
      TCompressionType compression_type = TCompressionType::None) {
    for (size_t i = 0;
         (dory.GetAckCount() < expected_ack_count) && (i < 3000);
         ++i) {
      SleepMilliseconds(10);
    }

    using TTracker = TReceivedRequestTracker;
    std::list<TTracker::TRequestInfo> received;
    bool got_msg_set = false;

    for (size_t i = 0; i < 3000; ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);

      for (auto &item : received) {
        if (item.MetadataRequestInfo.IsKnown()) {
          ASSERT_EQ(item.MetadataRequestInfo->ReturnedErrorCode, 0);
        } else if (item.ProduceRequestInfo.IsKnown()) {
          ASSERT_FALSE(got_msg_set);
          const TTracker::TProduceRequestInfo &info = *item.ProduceRequestInfo;
          ASSERT_EQ(info.Topic, topic);
          ASSERT_EQ(info.ReturnedErrorCode, 0);
          ASSERT_EQ(info.FirstMsgKey, key);
          ASSERT_EQ(info.FirstMsgValue, value);
          ASSERT_EQ(info.MsgCount, expected_msg_count);
          ASSERT_TRUE(info.CompressionType == compression_type);
          got_msg_set = true;
        } else {
          ASSERT_TRUE(false);
        }
      }

      received.clear();

      if (got_msg_set) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_TRUE(got_msg_set);
  }

  /* Fixture for end to end Dory unit test. */
  class TDoryTest : public ::testing::Test {
    protected:
    TDoryTest() = default;

    ~TDoryTest() override = default;

    void SetUp() override {
      ClearShutdownRequest();
    }

    void TearDown() override {
    }

    TXmlTestInitializer Initializer;  // initializes Xerces XML library
  };  // TDoryTest

  TEST_F(TDoryTest, SuccessfulDeliveryTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<std::string> topics;
    std::vector<std::string> bodies;
    topics.push_back(topic);
    bodies.push_back("Scooby");
    topics.push_back(topic);
    bodies.push_back("Shaggy");
    topics.push_back(topic);
    bodies.push_back("Velma");
    topics.push_back(topic);
    bodies.push_back("Daphne");
    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    for (size_t i = 0; (dory->GetAckCount() < 4) && (i < 3000); ++i) {
      SleepMilliseconds(10);
    }

    ASSERT_EQ(dory->GetAckCount(), 4U);
    using TTracker = TReceivedRequestTracker;
    std::list<TTracker::TRequestInfo> received;
    std::vector<std::string> expected_msgs;
    assert(topics.size() == bodies.size());

    for (size_t i = 0; i < topics.size(); ++i) {
      expected_msgs.push_back(bodies[i]);
    }

    for (size_t i = 0; i < 3000; ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);

      for (auto &item : received) {
        if (item.MetadataRequestInfo.IsKnown()) {
          ASSERT_EQ(item.MetadataRequestInfo->ReturnedErrorCode, 0);
        } else if (item.ProduceRequestInfo.IsKnown()) {
          const TTracker::TProduceRequestInfo &info = *item.ProduceRequestInfo;
          ASSERT_EQ(info.Topic, topic);
          ASSERT_EQ(info.ReturnedErrorCode, 0);
          auto iter = std::find(expected_msgs.begin(), expected_msgs.end(),
                                info.FirstMsgValue);

          if (iter == expected_msgs.end()) {
            ASSERT_TRUE(false);
          } else {
            expected_msgs.erase(iter);
          }
        } else {
          ASSERT_TRUE(false);
        }
      }

      received.clear();

      if (expected_msgs.empty()) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_TRUE(expected_msgs.empty());

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, KeyValueTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::string key, value;
    std::vector<uint8_t> dg_buf;
    size_t expected_ack_count = 0;

    /* empty key and value */
    MakeDg(dg_buf, topic, key, value);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);

    /* nonempty key and empty value */
    key = "Scooby";
    value = "";
    MakeDg(dg_buf, topic, key, value);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);

    /* empty key and nonempty value */
    key = "";
    value = "Shaggy";
    MakeDg(dg_buf, topic, key, value);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);

    /* nonempty key and nonempty value */
    key = "Velma";
    value = "Daphne";
    MakeDg(dg_buf, topic, key, value);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);
    GetKeyAndValue(*dory, mock_kafka, topic, key, value, ++expected_ack_count,
                   1);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, AckErrorTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    std::string msg_body("rejected on 1st attempt");

    /* Error code 6 is "not leader for partition", which causes the dispatcher
       to push the pause button. */
    bool success = kafka.Inj.InjectAckError(6, msg_body.c_str(), nullptr);
    ASSERT_TRUE(success);

    /* Kafka is having a really bad day today.  To make things interesting,
       make the mock Kafka server disconnect rather than sending a response on
       the attempted metadata request from dory.  dory should try again and
       succeed on the second attempt. */
    success =
        kafka.Inj.InjectDisconnectBeforeAllTopicsMetadataResponse(nullptr);
    ASSERT_TRUE(success);

    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<uint8_t> dg_buf;
    MakeDg(dg_buf, topic, msg_body);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);

    std::cout << "This part of the test is expected to take a while ..."
        << std::endl;

    /* We should get 2 ACKs: the first will be the injected error and the
       second will indicate successful redelivery. */
    for (size_t i = 0; (dory->GetAckCount() < 2) && (i < 3000); ++i) {
      SleepMilliseconds(10);
    }

    ASSERT_EQ(dory->GetAckCount(), 2U);
    using TTracker = TReceivedRequestTracker;
    std::list<TTracker::TRequestInfo> received;

    for (size_t i = 0; (received.size() < 5) && (i < 3000); ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);
      SleepMilliseconds(10);
    }

    ASSERT_EQ(received.size(), 5U);

    /* initial metadata request from daemon startup */
    TTracker::TRequestInfo *req_info = &received.front();
    ASSERT_TRUE(req_info->MetadataRequestInfo.IsKnown());
    ASSERT_EQ(req_info->MetadataRequestInfo->ReturnedErrorCode, 0);
    received.pop_front();

    /* injected error ACK */
    req_info = &received.front();
    ASSERT_TRUE(req_info->ProduceRequestInfo.IsKnown());
    TTracker::TProduceRequestInfo *prod_req_info =
        &*req_info->ProduceRequestInfo;
    ASSERT_EQ(prod_req_info->Topic, topic);
    ASSERT_EQ(prod_req_info->FirstMsgValue, msg_body);
    ASSERT_EQ(prod_req_info->ReturnedErrorCode, 6);
    received.pop_front();

    /* failed metadata request due to injected disconnect */
    req_info = &received.front();
    ASSERT_TRUE(req_info->MetadataRequestInfo.IsKnown());
    ASSERT_EQ(req_info->MetadataRequestInfo->ReturnedErrorCode, 0);
    received.pop_front();

    /* successful metadata request retry after injected disconnect */
    req_info = &received.front();
    ASSERT_TRUE(req_info->MetadataRequestInfo.IsKnown());
    ASSERT_EQ(req_info->MetadataRequestInfo->ReturnedErrorCode, 0);
    received.pop_front();

    /* successful redelivery ACK */
    req_info = &received.front();
    ASSERT_TRUE(req_info->ProduceRequestInfo.IsKnown());
    prod_req_info = &*req_info->ProduceRequestInfo;
    ASSERT_EQ(prod_req_info->Topic, topic);
    ASSERT_EQ(prod_req_info->FirstMsgValue, msg_body);
    ASSERT_EQ(prod_req_info->ReturnedErrorCode, 0);
    received.pop_front();

    ASSERT_TRUE(received.empty());

    /* Send another message (this time with no error injection) to make sure
       dory is still healthy. */
    msg_body = "another message";
    MakeDg(dg_buf, topic, msg_body);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);

    /* The ACK count should be incremented from its previous value of 2. */
    for (size_t i = 0; (dory->GetAckCount() < 3) && (i < 3000); ++i) {
      SleepMilliseconds(10);
    }

    ASSERT_EQ(dory->GetAckCount(), 3U);

    for (size_t i = 0; received.empty() && (i < 3000); ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);
      SleepMilliseconds(10);
    }

    ASSERT_FALSE(received.empty());

    /* successful delivery ACK */
    req_info = &received.front();
    ASSERT_TRUE(req_info->ProduceRequestInfo.IsKnown());
    prod_req_info = &*req_info->ProduceRequestInfo;
    ASSERT_EQ(prod_req_info->Topic, topic);
    ASSERT_EQ(prod_req_info->FirstMsgValue, msg_body);
    ASSERT_EQ(prod_req_info->ReturnedErrorCode, 0);
    received.pop_front();

    ASSERT_TRUE(received.empty());

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);

    /* Because of the message redelivery due to the injected error, the daemon
       currently reports 1 here.  This is overly pessimistic, since the ACK
       error clearly indicated failed delivery rather than some ambiguous
       result.  Overly pessimistic is ok, but overly optimistic is not.
       However we can still improve this behavior eventually. */
    ASSERT_LE(bad_stuff.DuplicateTopicMap.size(), 1U);

    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, DisconnectTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    std::string msg_body("rejected on 1st attempt");

    /* Make the mock Kafka server close the TCP connection rather than send an
       ACK (simulated broker crash). */
    bool success = kafka.Inj.InjectDisconnectBeforeAck(msg_body.c_str(),
                                                       nullptr);
    ASSERT_TRUE(success);

    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<uint8_t> dg_buf;
    MakeDg(dg_buf, topic, msg_body);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);

    /* We should get a single ACK when the message is successfully redelivered
       after the simulated broker crash. */
    for (size_t i = 0; (dory->GetAckCount() < 1) && (i < 3000); ++i) {
      SleepMilliseconds(10);
    }

    ASSERT_EQ(dory->GetAckCount(), 1U);
    using TTracker = TReceivedRequestTracker;
    std::list<TTracker::TRequestInfo> received;

    for (size_t i = 0; (received.size() < 3) && (i < 3000); ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);
      SleepMilliseconds(10);
    }

    ASSERT_EQ(received.size(), 3U);

    /* initial metadata request from daemon startup */
    TTracker::TRequestInfo *req_info = &received.front();
    ASSERT_TRUE(req_info->MetadataRequestInfo.IsKnown());
    ASSERT_EQ(req_info->MetadataRequestInfo->ReturnedErrorCode, 0);
    received.pop_front();

    /* metadata request due to pause */
    req_info = &received.front();
    ASSERT_TRUE(req_info->MetadataRequestInfo.IsKnown());
    ASSERT_EQ(req_info->MetadataRequestInfo->ReturnedErrorCode, 0);
    received.pop_front();

    /* successful redelivery ACK */
    req_info = &received.front();
    ASSERT_TRUE(req_info->ProduceRequestInfo.IsKnown());
    TTracker::TProduceRequestInfo *prod_req_info =
        &*req_info->ProduceRequestInfo;
    ASSERT_EQ(prod_req_info->Topic, topic);
    ASSERT_EQ(prod_req_info->FirstMsgValue, msg_body);
    ASSERT_EQ(prod_req_info->ReturnedErrorCode, 0);
    received.pop_front();

    ASSERT_TRUE(received.empty());

    /* Send another message (this time with no error injection) to make sure
       dory is still healthy. */
    msg_body = "another message";
    MakeDg(dg_buf, topic, msg_body);
    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);

    /* The ACK count should be incremented from its previous value of 1. */
    for (size_t i = 0; (dory->GetAckCount() < 2) && (i < 3000); ++i) {
      SleepMilliseconds(10);
    }

    ASSERT_EQ(dory->GetAckCount(), 2U);

    for (size_t i = 0; received.empty() && (i < 3000); ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);
      SleepMilliseconds(10);
    }

    ASSERT_FALSE(received.empty());

    /* successful delivery ACK */
    req_info = &received.front();
    ASSERT_TRUE(req_info->ProduceRequestInfo.IsKnown());
    prod_req_info = &*req_info->ProduceRequestInfo;
    ASSERT_EQ(prod_req_info->Topic, topic);
    ASSERT_EQ(prod_req_info->FirstMsgValue, msg_body);
    ASSERT_EQ(prod_req_info->ReturnedErrorCode, 0);
    received.pop_front();

    ASSERT_TRUE(received.empty());

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);

    /* This count is 1 due to the simulated broker crash.  Since the broker
       "crashed" before sending an ACK, dory doesn't know whether the broker
       received the message.  Therefore dory resends it, possibly creating a
       duplicate. */
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 1U);

    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, MalformedMsgTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();

    /* This message will get discarded because it's malformed. */
    std::string msg_body("I like scooby snacks");
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<uint8_t> dg_buf;
    MakeDg(dg_buf, topic, msg_body);

    /* Overwrite the size field with an incorrect value. */
    ASSERT_GE(dg_buf.size(), sizeof(int32_t));
    WriteInt32ToHeader(&dg_buf[0], dg_buf.size() - 1);

    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);

    for (size_t num_tries = 0; ; ++num_tries) {
      if (num_tries > 30) {
        /* Test timed out. */
        ASSERT_TRUE(false);
        break;
      }

      SleepMilliseconds(1000);
      TAnomalyTracker::TInfo bad_stuff;
      dory->GetAnomalyTracker().GetInfo(bad_stuff);

      if (bad_stuff.MalformedMsgCount == 0) {
        continue;
      }

      ASSERT_EQ(bad_stuff.MalformedMsgCount, 1U);
      ASSERT_EQ(bad_stuff.MalformedMsgs.size(), 1U);

      ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);
      ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
      ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
      break;  // success
    }

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, UncleanDisconnectTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    server.UseUnixStreamSocket();
    server.UseTcpInputSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();

    std::unique_ptr<TClientSenderBase> unix_stream_sender(
        new TUnixStreamSender(server.GetUnixStreamSocketName()));
    unix_stream_sender->PrepareToSend();
    std::unique_ptr<TClientSenderBase> tcp_sender(
        new TTcpSender(server.GetInputPort()));
    tcp_sender->PrepareToSend();

    /* Send a short message consisting of the first 2 bytes of the size field
       and then disconnect. */
    std::vector<uint8_t> msg_buf;
    msg_buf.resize(2);
    msg_buf[0] = 0;
    msg_buf[1] = 0;
    unix_stream_sender->Send(&msg_buf[0], msg_buf.size());
    unix_stream_sender->Reset();

    /* Reconnect, send a short message consisting of the first half of a
       complete message, and then disconnect. */
    unix_stream_sender.reset(
        new TUnixStreamSender(server.GetUnixStreamSocketName()));
    unix_stream_sender->PrepareToSend();
    std::string msg_body("I like scooby snacks");
    MakeDg(msg_buf, topic, msg_body);
    unix_stream_sender->Send(&msg_buf[0], msg_buf.size() / 2);
    unix_stream_sender->Reset();

    /* Send the same truncated message over a TCP connection and then
       disconnect. */
    tcp_sender->Send(&msg_buf[0], msg_buf.size() / 2);
    tcp_sender->Reset();

    /* Reconnect using UNIX stream sockets, send a complete message, and
       disconnect. */
    unix_stream_sender.reset(
        new TUnixStreamSender(server.GetUnixStreamSocketName()));
    unix_stream_sender->PrepareToSend();
    unix_stream_sender->Send(&msg_buf[0], msg_buf.size());
    unix_stream_sender->Reset();

    /* Reconnect using local TCP, send a complete message, and disconnect. */
    tcp_sender.reset(new TTcpSender(server.GetInputPort()));
    tcp_sender->PrepareToSend();
    tcp_sender->Send(&msg_buf[0], msg_buf.size());
    tcp_sender->Reset();

    for (size_t num_tries = 0; ; ++num_tries) {
      if (num_tries > 30) {
        /* Test timed out. */
        ASSERT_TRUE(false);
        break;
      }

      SleepMilliseconds(1000);
      TAnomalyTracker::TInfo bad_stuff;
      dory->GetAnomalyTracker().GetInfo(bad_stuff);

      if ((bad_stuff.UnixStreamUncleanDisconnectCount < 2) ||
          (bad_stuff.TcpUncleanDisconnectCount < 1)) {
        continue;
      }

      ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
      ASSERT_EQ(bad_stuff.UnixStreamUncleanDisconnectCount, 2U);
      ASSERT_EQ(bad_stuff.TcpUncleanDisconnectCount, 1U);
      ASSERT_EQ(bad_stuff.UnsupportedApiKeyMsgCount, 0U);
      ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);
      ASSERT_EQ(bad_stuff.BadTopicMsgCount, 0U);
      ASSERT_TRUE(bad_stuff.DiscardTopicMap.empty());
      ASSERT_TRUE(bad_stuff.DuplicateTopicMap.empty());
      ASSERT_TRUE(bad_stuff.RateLimitDiscardMap.empty());
      ASSERT_TRUE(bad_stuff.MalformedMsgs.empty());
      ASSERT_EQ(bad_stuff.UnixStreamUncleanDisconnectMsgs.size(), 2U);
      ASSERT_EQ(bad_stuff.TcpUncleanDisconnectMsgs.size(), 1U);
      ASSERT_TRUE(bad_stuff.UnsupportedVersionMsgs.empty());
      ASSERT_TRUE(bad_stuff.LongMsgs.empty());
      ASSERT_TRUE(bad_stuff.BadTopics.empty());
      break;  // success
    }

    /* Now make sure the two complete messages were delivered successfully. */

    using TTracker = TReceivedRequestTracker;
    std::list<TTracker::TRequestInfo> received;
    size_t num_received = 0;

    for (size_t i = 0; i < 3000; ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);

      for (auto &item : received) {
        if (item.MetadataRequestInfo.IsKnown()) {
          ASSERT_EQ(item.MetadataRequestInfo->ReturnedErrorCode, 0);
        } else if (item.ProduceRequestInfo.IsKnown()) {
          const TTracker::TProduceRequestInfo &info = *item.ProduceRequestInfo;
          ASSERT_EQ(info.Topic, topic);
          ASSERT_EQ(info.ReturnedErrorCode, 0);
          ASSERT_EQ(info.FirstMsgValue, msg_body);
          ++num_received;
        } else {
          ASSERT_TRUE(false);
        }
      }

      if (num_received >= 2) {
        break;
      }

      received.clear();
      SleepMilliseconds(10);
    }

    ASSERT_EQ(num_received, 2U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, UnsupportedVersionMsgTest) {
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();

    /* This message will get discarded because it's malformed. */
    std::string msg_body("I like scooby snacks");
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<uint8_t> dg_buf;
    MakeDg(dg_buf, topic, msg_body);

    /* Overwrite the version field with a bad value. */
    ASSERT_GE(dg_buf.size(), sizeof(int32_t) + sizeof(int8_t));
    dg_buf[4] = -1;

    ret = sock.Send(&dg_buf[0], dg_buf.size());
    ASSERT_EQ(ret, DORY_OK);

    for (size_t num_tries = 0; ; ++num_tries) {
      if (num_tries > 30) {
        /* Test timed out. */
        ASSERT_TRUE(false);
        break;
      }

      SleepMilliseconds(1000);
      TAnomalyTracker::TInfo bad_stuff;
      dory->GetAnomalyTracker().GetInfo(bad_stuff);

      if (bad_stuff.UnsupportedApiKeyMsgCount == 0) {
        continue;
      }

      ASSERT_EQ(bad_stuff.UnsupportedApiKeyMsgCount, 1U);
      ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
      ASSERT_EQ(bad_stuff.MalformedMsgs.size(), 0U);
      ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
      ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
      break;  // success
    }

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  std::string CreateCompressionTestConf(in_port_t broker_port,
      size_t compression_min_size, const char *compression_type,
      TOpt<int> compression_level) {
    std::string level_blurb;

    if (compression_level.IsKnown()) {
      level_blurb = " level=\"";
      level_blurb += std::to_string(*compression_level);
      level_blurb += "\"";
    }

    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>" << std::endl
       << "<doryConfig>" << std::endl
       << "    <batching>" << std::endl
       << "        <namedConfigs>" << std::endl
       << "            <config name=\"config1\">" << std::endl
       << "                <time value=\"disable\" />" << std::endl
       << "                <messages value=\"10\" />" << std::endl
       << "                <bytes value=\"disable\" />" << std::endl
       << "            </config>" << std::endl
       << "        </namedConfigs>" << std::endl
       << "        <produceRequestDataLimit value=\"1024k\" />" << std::endl
       << "        <messageMaxBytes value=\"1024k\" />" << std::endl
       << "        <combinedTopics enable=\"false\" />" << std::endl
       << "        <defaultTopic action=\"perTopic\" config=\"config1\" />"
       << std::endl
       << "    </batching>" << std::endl
       << "    <compression>" << std::endl
       << "        <namedConfigs>" << std::endl
       << "            <config name=\"config1\" type=\"" << compression_type
       << "\"" << level_blurb << " minSize=\""
       << std::to_string(compression_min_size) << "\" />"
       << std::endl
       << "        </namedConfigs>" << std::endl
       << std::endl
       << "        <defaultTopic config=\"config1\" />" << std::endl
       << "    </compression>" << std::endl
       << "    <initialBrokers>" << std::endl
       << "        <broker host=\"localhost\" port=\"" << broker_port <<"\" />"
       << std::endl
       << "    </initialBrokers>" << std::endl
       << "</doryConfig>" << std::endl;
    return os.str();
  }

  TEST_F(TDoryTest, GzipCompressionTest1) {
    std::unique_ptr<TProduceProtocol>
        produce_protocol(ChooseProduceProto(0));
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    std::string msg_body_1("123456789");
    size_t data_size = msg_body_1.size() +
        produce_protocol->GetSingleMsgOverhead();
    TDoryTestServer server(port, 1024,
        CreateCompressionTestConf(port, 1 + (10 * data_size), "gzip",
            TOpt<int>()));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<std::string> topics;
    std::vector<std::string> bodies;

    /* These will be batched together as a single message set, but compression
       will not be used because of the size threshold. */
    for (size_t i = 0; i < 10; ++i) {
      topics.push_back(topic);
      bodies.push_back(msg_body_1);
    }

    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                               TCompressionType::None);

    /* This will push the total size to the threshold and cause compression. */
    bodies[9].push_back('0');

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                   TCompressionType::Gzip);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, GzipCompressionTest2) {
    std::unique_ptr<TProduceProtocol>
        produce_protocol(ChooseProduceProto(0));
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    std::string msg_body_1("123456789");
    size_t data_size = msg_body_1.size() +
        produce_protocol->GetSingleMsgOverhead();
    TDoryTestServer server(port, 1024,
        CreateCompressionTestConf(port, 1 + (10 * data_size), "gzip",
            TOpt<int>(4)));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<std::string> topics;
    std::vector<std::string> bodies;

    /* These will be batched together as a single message set, but compression
       will not be used because of the size threshold. */
    for (size_t i = 0; i < 10; ++i) {
      topics.push_back(topic);
      bodies.push_back(msg_body_1);
    }

    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                               TCompressionType::None);

    /* This will push the total size to the threshold and cause compression. */
    bodies[9].push_back('0');

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                   TCompressionType::Gzip);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, Lz4CompressionTest1) {
    std::unique_ptr<TProduceProtocol>
        produce_protocol(ChooseProduceProto(0));
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    std::string msg_body_1("123456789");
    size_t data_size = msg_body_1.size() +
        produce_protocol->GetSingleMsgOverhead();
    TDoryTestServer server(port, 1024,
        CreateCompressionTestConf(port, 1 + (10 * data_size), "lz4",
            TOpt<int>()));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<std::string> topics;
    std::vector<std::string> bodies;

    /* These will be batched together as a single message set, but compression
       will not be used because of the size threshold. */
    for (size_t i = 0; i < 10; ++i) {
      topics.push_back(topic);
      bodies.push_back(msg_body_1);
    }

    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                               TCompressionType::None);

    /* This will push the total size to the threshold and cause compression. */
    bodies[9].push_back('0');

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                   TCompressionType::Lz4);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, Lz4CompressionTest2) {
    std::unique_ptr<TProduceProtocol>
        produce_protocol(ChooseProduceProto(0));
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    std::string msg_body_1("123456789");
    size_t data_size = msg_body_1.size() +
        produce_protocol->GetSingleMsgOverhead();
    TDoryTestServer server(port, 1024,
        CreateCompressionTestConf(port, 1 + (10 * data_size), "lz4",
            TOpt<int>(3)));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<std::string> topics;
    std::vector<std::string> bodies;

    /* These will be batched together as a single message set, but compression
       will not be used because of the size threshold. */
    for (size_t i = 0; i < 10; ++i) {
      topics.push_back(topic);
      bodies.push_back(msg_body_1);
    }

    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                               TCompressionType::None);

    /* This will push the total size to the threshold and cause compression. */
    bodies[9].push_back('0');

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                   TCompressionType::Lz4);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  TEST_F(TDoryTest, SnappyCompressionTest) {
    std::unique_ptr<TProduceProtocol>
        produce_protocol(ChooseProduceProto(0));
    std::string topic("scooby_doo");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic.c_str(), 2, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    std::string msg_body_1("123456789");
    size_t data_size = msg_body_1.size() +
        produce_protocol->GetSingleMsgOverhead();
    TDoryTestServer server(port, 1024,
        CreateCompressionTestConf(port, 1 + (10 * data_size), "snappy",
            TOpt<int>()));
    server.UseUnixDgSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();
    TDoryClientSocket sock;
    int ret = sock.Bind(server.GetUnixDgSocketName());
    ASSERT_EQ(ret, DORY_OK);
    std::vector<std::string> topics;
    std::vector<std::string> bodies;

    /* These will be batched together as a single message set, but compression
       will not be used because of the size threshold. */
    for (size_t i = 0; i < 10; ++i) {
      topics.push_back(topic);
      bodies.push_back(msg_body_1);
    }

    std::vector<uint8_t> dg_buf;

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                               TCompressionType::None);

    /* This will push the total size to the threshold and cause compression. */
    bodies[9].push_back('0');

    for (size_t i = 0; i < topics.size(); ++i) {
      MakeDg(dg_buf, topics[i], bodies[i]);
      ret = sock.Send(&dg_buf[0], dg_buf.size());
      ASSERT_EQ(ret, DORY_OK);
    }

    GetKeyAndValue(*dory, mock_kafka, topic, "", msg_body_1, 1, 10,
                   TCompressionType::Snappy);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

  std::string CreateStressTestMsgBody(const std::string &msg_body_base,
      size_t seq, size_t pad) {
    std::string result;
    std::string seq_str = std::to_string(seq);

    if (seq_str.size() < pad) {
      result.assign(pad - seq_str.size(), '0');
    }

    result += seq_str;
    result.push_back(' ');
    result += msg_body_base;
    return result;
  }

  std::vector<std::string> CreateStressTestMsgBodyVec(
      const std::string &msg_body_base, size_t msg_count, size_t pad) {
    std::vector<std::string> result;

    for (size_t i = 0; i < msg_count; ++i) {
      result.push_back(CreateStressTestMsgBody(msg_body_base, i, pad));
    }

    return result;
  }

  class TMsgBlaster : public TFdManagedThread {
    NO_COPY_SEMANTICS(TMsgBlaster);

    public:
    TMsgBlaster(TClientSenderBase *sender, const std::string &topic,
        const std::vector<std::string> &msg_vec)
        : Sender(sender),
          Topic(topic),
          MsgVec(msg_vec) {
    }

    virtual ~TMsgBlaster() {
    }

    virtual void Run() override;

    private:
    std::unique_ptr<TClientSenderBase> Sender;

    const std::string &Topic;

    const std::vector<std::string> &MsgVec;
  };  // TMsgBlaster

  void TMsgBlaster::Run() {
    assert(this);
    std::vector<uint8_t> msg_buf;

    for (const std::string &msg : MsgVec) {
      MakeDg(msg_buf, Topic, msg);
      Sender->Send(&msg_buf[0], msg_buf.size());
    }
  }

  TEST_F(TDoryTest, SimpleStressTest) {
    std::cout << "This test is expected to take a while ..." << std::endl;
    std::vector<std::string> topic_vec;
    topic_vec.push_back("scooby_doo");
    topic_vec.push_back("shaggy");
    std::vector<std::string> kafka_config;
    CreateKafkaConfig(2, topic_vec, 1, kafka_config);
    TMockKafkaConfig kafka(kafka_config);
    kafka.StartKafka();
    Dory::MockKafkaServer::TMainThread &mock_kafka = *kafka.MainThread;

    /* Translate virtual port from the mock Kafka server setup file into a
       physical port.  See big comment in <dory/mock_kafka_server/port_map.h>
       for an explanation of what is going on here. */
    in_port_t port = mock_kafka.VirtualPortToPhys(10000);

    assert(port);
    TDoryTestServer server(port, 100 * 1024, CreateSimpleDoryConf(port));
    server.UseUnixDgSocket();
    server.UseUnixStreamSocket();
    server.UseTcpInputSocket();
    bool started = server.SyncStart();
    ASSERT_TRUE(started);
    TDoryServer *dory = server.GetDory();

    std::string msg_base_0("UNIX datagram message for Scooby");
    std::string msg_base_1("UNIX datagram message for Shaggy");
    std::string msg_base_2("UNIX stream message for Scooby");
    std::string msg_base_3("UNIX stream message for Shaggy");
    std::string msg_base_4("TCP message for Scooby");
    std::string msg_base_5("TCP message for Shaggy");
    std::vector<std::string> unix_dg_msg_bodies_0 =
        CreateStressTestMsgBodyVec(msg_base_0, 50000, 5);
    std::vector<std::string> unix_dg_msg_bodies_1 =
        CreateStressTestMsgBodyVec(msg_base_1, 50000, 5);
    std::vector<std::string> unix_stream_msg_bodies_0 =
        CreateStressTestMsgBodyVec(msg_base_2, 50000, 5);
    std::vector<std::string> unix_stream_msg_bodies_1 =
        CreateStressTestMsgBodyVec(msg_base_3, 50000, 5);
    std::vector<std::string> tcp_msg_bodies_0 =
        CreateStressTestMsgBodyVec(msg_base_4, 50000, 5);
    std::vector<std::string> tcp_msg_bodies_1 =
        CreateStressTestMsgBodyVec(msg_base_5, 50000, 5);

    std::unique_ptr<TClientSenderBase> sender(
        new TUnixDgSender(server.GetUnixDgSocketName()));
    sender->PrepareToSend();
    TMsgBlaster b0(sender.release(), topic_vec[0], unix_dg_msg_bodies_0);
    sender.reset(new TUnixDgSender(server.GetUnixDgSocketName()));
    sender->PrepareToSend();
    TMsgBlaster b1(sender.release(), topic_vec[1], unix_dg_msg_bodies_1);
    sender.reset(new TUnixStreamSender(server.GetUnixStreamSocketName()));
    sender->PrepareToSend();
    TMsgBlaster b2(sender.release(), topic_vec[0], unix_stream_msg_bodies_0);
    sender.reset(new TUnixStreamSender(server.GetUnixStreamSocketName()));
    sender->PrepareToSend();
    TMsgBlaster b3(sender.release(), topic_vec[1], unix_stream_msg_bodies_1);
    sender.reset(new TTcpSender(server.GetInputPort()));
    sender->PrepareToSend();
    TMsgBlaster b4(sender.release(), topic_vec[0], tcp_msg_bodies_0);
    sender.reset(new TTcpSender(server.GetInputPort()));
    sender->PrepareToSend();
    TMsgBlaster b5(sender.release(), topic_vec[1], tcp_msg_bodies_1);

    b0.Start();
    b1.Start();
    b2.Start();
    b3.Start();
    b4.Start();
    b5.Start();

    for (size_t i = 0; (dory->GetAckCount() < 300000) && (i < 30000); ++i) {
      SleepMilliseconds(10);
    }

    ASSERT_EQ(dory->GetAckCount(), 300000U);
    b0.Join();
    b1.Join();
    b2.Join();
    b3.Join();
    b4.Join();
    b5.Join();

    using TTracker = TReceivedRequestTracker;
    std::list<TTracker::TRequestInfo> received;
    std::vector<std::pair<std::string, std::string>> received_msgs;

    for (size_t i = 0; i < 30000; ++i) {
      mock_kafka.NonblockingGetHandledRequests(received);

      for (auto &item : received) {
        if (item.MetadataRequestInfo.IsKnown()) {
          ASSERT_EQ(item.MetadataRequestInfo->ReturnedErrorCode, 0);
        } else if (item.ProduceRequestInfo.IsKnown()) {
          const TTracker::TProduceRequestInfo &info = *item.ProduceRequestInfo;
          ASSERT_EQ(info.ReturnedErrorCode, 0);
          received_msgs.push_back(
              std::make_pair(info.Topic, info.FirstMsgValue));
        } else {
          ASSERT_TRUE(false);
        }
      }

      received.clear();

      if (received_msgs.size() == 300000) {
        break;
      }

      SleepMilliseconds(10);
    }

    ASSERT_EQ(received_msgs.size(), 300000U);
    std::vector<std::string> v0, v1, v2, v3, v4, v5;

    for (const auto &topic_and_body : received_msgs) {
      const std::string &topic = topic_and_body.first;
      const std::string &body = topic_and_body.second;

      if (body.find(msg_base_0) != std::string::npos) {
        EXPECT_EQ(topic, topic_vec[0]);
        v0.push_back(body);
      } else if (body.find(msg_base_1) != std::string::npos) {
        EXPECT_EQ(topic, topic_vec[1]);
        v1.push_back(body);
      } else if (body.find(msg_base_2) != std::string::npos) {
        EXPECT_EQ(topic, topic_vec[0]);
        v2.push_back(body);
      } else if (body.find(msg_base_3) != std::string::npos) {
        EXPECT_EQ(topic, topic_vec[1]);
        v3.push_back(body);
      } else if (body.find(msg_base_4) != std::string::npos) {
        EXPECT_EQ(topic, topic_vec[0]);
        v4.push_back(body);
      } else if (body.find(msg_base_5) != std::string::npos) {
        EXPECT_EQ(topic, topic_vec[1]);
        v5.push_back(body);
      } else {
        ASSERT_TRUE(false);
      }
    }

    ASSERT_EQ(unix_dg_msg_bodies_0, v0);
    ASSERT_EQ(unix_dg_msg_bodies_1, v1);
    ASSERT_EQ(unix_stream_msg_bodies_0, v2);
    ASSERT_EQ(unix_stream_msg_bodies_1, v3);
    ASSERT_EQ(tcp_msg_bodies_0, v4);
    ASSERT_EQ(tcp_msg_bodies_1, v5);

    TAnomalyTracker::TInfo bad_stuff;
    dory->GetAnomalyTracker().GetInfo(bad_stuff);
    ASSERT_EQ(bad_stuff.DiscardTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.DuplicateTopicMap.size(), 0U);
    ASSERT_EQ(bad_stuff.BadTopics.size(), 0U);
    ASSERT_EQ(bad_stuff.MalformedMsgCount, 0U);
    ASSERT_EQ(bad_stuff.UnsupportedVersionMsgCount, 0U);

    server.RequestShutdown();
    server.Join();
    ASSERT_EQ(server.GetDoryReturnValue(), EXIT_SUCCESS);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  /* Initialize logging and start signal handler thread only once, before any
     tests run.  Then stop thread after all tests have finished. */
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  TSignalHandlerThreadStarter signal_handler_starter;

  return RUN_ALL_TESTS();
}
