/* <dory/router_thread.cc>

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

   Implements <dory/router_thread.h>.
 */

#include <dory/router_thread.h>

#include <cstdlib>
#include <exception>
#include <limits>

#include <unistd.h>

#include <base/counter.h>
#include <base/error_util.h>
#include <base/gettid.h>
#include <base/no_default_case.h>
#include <base/time_util.h>
#include <dory/kafka_proto/metadata/metadata_protocol.h>
#include <dory/kafka_proto/metadata/version_util.h>
#include <dory/kafka_proto/produce/produce_protocol.h>
#include <dory/kafka_proto/produce/version_util.h>
#include <dory/util/connect_to_host.h>
#include <dory/util/topic_map.h>
#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Batch;
using namespace Dory::Conf;
using namespace Dory::Debug;
using namespace Dory::KafkaProto::Metadata;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::MsgDispatch;
using namespace Dory::Util;
using namespace Log;

DEFINE_COUNTER(BatchExpiryDetected);
DEFINE_COUNTER(ConnectFailOnTopicAutocreate);
DEFINE_COUNTER(ConnectFailOnTryGetMetadata);
DEFINE_COUNTER(ConnectSuccessOnTopicAutocreate);
DEFINE_COUNTER(ConnectSuccessOnTryGetMetadata);
DEFINE_COUNTER(DiscardBadTopicMsgOnRoute);
DEFINE_COUNTER(DiscardBadTopicOnReroute);
DEFINE_COUNTER(DiscardDeletedTopicMsg);
DEFINE_COUNTER(DiscardDueToRateLimit);
DEFINE_COUNTER(DiscardLongMsg);
DEFINE_COUNTER(DiscardNoAvailablePartition);
DEFINE_COUNTER(DiscardNoAvailablePartitionOnReroute);
DEFINE_COUNTER(DiscardNoLongerAvailableTopicMsg);
DEFINE_COUNTER(DiscardOnTopicAutocreateFail);
DEFINE_COUNTER(FinishRefreshMetadata);
DEFINE_COUNTER(GetMetadataFail);
DEFINE_COUNTER(GetMetadataSuccess);
DEFINE_COUNTER(MetadataChangedOnRefresh);
DEFINE_COUNTER(MetadataUnchangedOnRefresh);
DEFINE_COUNTER(MetadataUpdated);
DEFINE_COUNTER(PerTopicBatchAnyPartition);
DEFINE_COUNTER(PossibleDuplicateMsg);
DEFINE_COUNTER(RefreshMetadataSuccess);
DEFINE_COUNTER(RouteMsgBatchList);
DEFINE_COUNTER(RouterThreadFinishPause);
DEFINE_COUNTER(RouterThreadGetMsgList);
DEFINE_COUNTER(RouterThreadStartPause);
DEFINE_COUNTER(RouteSingleAnyPartitionMsg);
DEFINE_COUNTER(RouteSingleMsg);
DEFINE_COUNTER(RouteSinglePartitionKeyMsg);
DEFINE_COUNTER(SetBatchExpiry);
DEFINE_COUNTER(StartRefreshMetadata);
DEFINE_COUNTER(TopicHasNoAvailablePartitions);

static unsigned GetRandomNumber() {
  return std::rand();
}

TRouterThread::TRouterThread(const TConfig &config, const TConf &conf,
    TAnomalyTracker &anomaly_tracker, TMsgStateTracker &msg_state_tracker,
    const Batch::TGlobalBatchConfig &batch_config,
    const Debug::TDebugSetup &debug_setup,
    MsgDispatch::TKafkaDispatcherApi &dispatcher)
    : Config(config),
      TopicRateConf(conf.GetTopicRateConf()),
      MsgRateLimiter(TopicRateConf),
      MessageMaxBytes(batch_config.GetMessageMaxBytes()),
      AnomalyTracker(anomaly_tracker),
      MsgStateTracker(msg_state_tracker),
      DebugSetup(debug_setup),
      KnownBrokers(conf.GetInitialBrokers()),
      PerTopicBatcher(batch_config.GetPerTopicConfig()),
      Dispatcher(dispatcher),
      DebugLogger(debug_setup, TDebugSetup::TLogId::MSG_RECEIVE) {
}

TRouterThread::~TRouterThread() {
  /* This will shut down the thread if something unexpected happens.  Setting
     the 'Destroying' flag tells the thread to shut down immediately when it
     gets the shutdown request. */
  Destroying = true;
  ShutdownOnDestroy();
}

void TRouterThread::Run() {
  assert(this);
  int tid = static_cast<int>(Gettid());
  LOG(TPri::NOTICE) << "Router thread " << tid << " started";

  try {
    DoRun();
  } catch (const TShutdownOnDestroy &) {
    Die("TShutdownOnDestroy thrown from router thread");
  } catch (const std::exception &x) {
    LOG(TPri::ERR) << "Fatal error in router thread " << tid << ": "
        << x.what();
    Die("Terminating on fatal error");
  } catch (...) {
    LOG(TPri::ERR) << "Fatal unknown error in router thread " << tid;
    Die("Terminating on fatal error");
  }

  LOG(TPri::NOTICE) << "Router thread " << tid << " finished "
      << (OkShutdown ? "normally" : "on error");
}

size_t TRouterThread::ComputeRetryDelay(size_t mean_delay, size_t div) {
  size_t half_range = mean_delay / div;
  size_t lower_bound = mean_delay - half_range;
  size_t upper_bound = mean_delay + half_range;
  size_t range = upper_bound - lower_bound + 1;
  return (std::rand() % range) + lower_bound;
}

void TRouterThread::StartShutdown() {
  assert(this);

  if (Destroying) {
    throw TShutdownOnDestroy();
  }

  assert(ShutdownStartTime.IsUnknown());
  ShutdownStartTime.MakeKnown(GetEpochMilliseconds());
  NeedToContinueShutdown = true;

  /* Future attempts to monitor this FD will not find it readable.  However, if
     something bad happens and our destructor is invoked, we will see the FD
     become readable again (and 'Destroying' set to true), and terminate
     immediately. */
  ClearShutdownRequest();
}

void TRouterThread::Discard(TMsg::TPtr &&msg,
    TAnomalyTracker::TDiscardReason reason) {
  assert(this);
  assert(msg);
  TMsg::TPtr to_discard(std::move(msg));
  AnomalyTracker.TrackDiscard(to_discard, reason);
  MsgStateTracker.MsgEnterProcessed(*to_discard);
}

void TRouterThread::Discard(std::list<TMsg::TPtr> &&msg_list,
    TAnomalyTracker::TDiscardReason reason) {
  assert(this);
  std::list<TMsg::TPtr> to_discard(std::move(msg_list));

  for (TMsg::TPtr &msg : to_discard) {
    assert(msg);
    AnomalyTracker.TrackDiscard(msg, reason);
  }

  MsgStateTracker.MsgEnterProcessed(to_discard);
}

void TRouterThread::Discard(std::list<std::list<TMsg::TPtr>> &&batch_list,
    TAnomalyTracker::TDiscardReason reason) {
  assert(this);
  std::list<std::list<TMsg::TPtr>> to_discard(std::move(batch_list));

  for (std::list<TMsg::TPtr> &msg_list : to_discard) {
    for (TMsg::TPtr &msg : msg_list) {
      assert(msg);
      AnomalyTracker.TrackDiscard(msg, reason);
    }
  }

  MsgStateTracker.MsgEnterProcessed(to_discard);
}

bool TRouterThread::UpdateMetadataAfterTopicAutocreate(
    const std::string &topic) {
  assert(this);
  size_t sleep_ms = 3000;
  static const size_t NUM_ATTEMPTS = 3;

  /* Wait a few seconds, and then update our metadata.  If metadata does not
     yet show the new topic, wait a bit longer and try again.  If new topic
     still does not appear after a few iterations of this, then give up. */
  for (size_t i = 0; ; ) {
    SleepMilliseconds(sleep_ms);

    if (!HandleMetadataUpdate()) {
      /* Shutdown delay expired during metadata update. */
      return false;
    }

    if (Metadata->FindTopicIndex(topic) >= 0) {
      /* Success: topic appears in new metadata */
      return true;
    }

    if (++i == NUM_ATTEMPTS) {
      break;
    }

    sleep_ms *= 2;
    LOG(TPri::INFO) << "Newly created topic [" << topic
        << "] does not yet appear in metadata: will fetch metadata again in "
        << sleep_ms << " ms";
  }

  LOG(TPri::WARNING) << "Newly created topic [" << topic
      << "] does not appear in metadata after " << NUM_ATTEMPTS << " updates";
  return true;  // keep running
}

bool TRouterThread::AutocreateTopic(TMsg::TPtr &msg) {
  assert(this);
  assert(!KnownBrokers.empty());
  assert(msg);
  const std::string &topic = msg->GetTopic();
  TMetadataFetcher::TDisconnecter disconnecter(*MetadataFetcher);
  size_t chosen = std::rand() % KnownBrokers.size();
  bool fail = false;

  for (size_t i = 0;
       i < KnownBrokers.size();
       chosen = ((chosen + 1) % KnownBrokers.size()), ++i) {
    const TKafkaBroker &broker = KnownBrokers[chosen];
    LOG(TPri::INFO) << "Router thread sending autocreate request for topic ["
        << topic << "] to broker " << broker.Host << " port " << broker.Port;

    if (!MetadataFetcher->Connect(broker.Host, broker.Port)) {
      ConnectFailOnTopicAutocreate.Increment();
      LOG(TPri::ERR)
          << "Router thread failed to connect to broker for topic autocreate";
      continue;
    }

    ConnectSuccessOnTopicAutocreate.Increment();

    switch (MetadataFetcher->TopicAutocreate(topic.c_str(),
        static_cast<int>(Config.KafkaSocketTimeout * 1000))) {
      case TMetadataFetcher::TTopicAutocreateResult::Success: {
        LOG(TPri::NOTICE) << "Automatic creation of topic [" << topic
            << "] was successful: updating metadata";

        /* Update metadata so it shows the newly created topic. */
        bool keep_running = UpdateMetadataAfterTopicAutocreate(topic);

        if (!keep_running) {
          /* Shutdown delay expired during metadata update. */
          DiscardOnShutdownDuringMetadataUpdate(std::move(msg));
        }

        return keep_running;
      }
      case TMetadataFetcher::TTopicAutocreateResult::Fail: {
        fail = true;
        break;
      }
      case TMetadataFetcher::TTopicAutocreateResult::TryOtherBroker: {
        break;
      }
      NO_DEFAULT_CASE;
    }

    if (fail) {
      break;
    }

    /* Try next broker. */
    LOG(TPri::ERR)
        << "Router thread did not get valid topic autocreate response from "
        << "broker";
  }

  if (!Config.NoLogDiscard) {
    LOG_R(TPri::ERR, std::chrono::seconds(30))
        << "Discarding message because topic autocreate failed: [" << topic
        << "]";
  }

  Discard(std::move(msg),
          TAnomalyTracker::TDiscardReason::FailedTopicAutocreate);
  DiscardOnTopicAutocreateFail.Increment();
  return true;
}

bool TRouterThread::ValidateNewMsg(TMsg::TPtr &msg) {
  assert(this);
  assert(Metadata);
  const std::string &topic = msg->GetTopic();
  int topic_index = Metadata->FindTopicIndex(topic);

  if (topic_index < 0) {
    if (Config.TopicAutocreate) {
      if (!AutocreateTopic(msg)) {
        /* Shutdown delay expired during metadata update. */
        assert(!msg);
        return false;
      }

      /* On successful topic autocreate, the message will still exist.  On
         failure, it will have been discarded. */
      if (!msg) {
        return true;
      }

      topic_index = Metadata->FindTopicIndex(topic);
    }

    if (topic_index < 0) {
      if (!Config.NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Discarding message due to unknown topic: [" << topic << "]";
      }

      AnomalyTracker.TrackBadTopicDiscard(msg);
      MsgStateTracker.MsgEnterProcessed(*msg);
      DiscardBadTopicMsgOnRoute.Increment();
      msg.reset();
      return true;
    }
  }

  if (msg->BodyIsTruncated() ||
      ((msg->GetKeyAndValue().Size() + SingleMsgOverhead) > MessageMaxBytes)) {
    /* Check for truncation _after_ checking for topic existence.  If the topic
       doesn't exist, we treat it as a bad topic discard even if the message is
       also too long.  Perform this check _before_ assigning a partition so we
       still log the fact that we got a too long message even when Kafka
       problems would prevent assigning a partition. */

    if (!Config.NoLogDiscard) {
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Discarding message that exceeds max allowed size: topic ["
          << topic << "]";
    }

    AnomalyTracker.TrackLongMsgDiscard(msg);
    MsgStateTracker.MsgEnterProcessed(*msg);
    DiscardLongMsg.Increment();
    msg.reset();
  } else {
    const std::vector<TMetadata::TTopic> &topic_vec = Metadata->GetTopics();
    assert((topic_index >= 0) &&
           (static_cast<size_t>(topic_index) < topic_vec.size()));
    const TMetadata::TTopic &topic_meta = topic_vec[topic_index];

    if (topic_meta.GetOkPartitions().empty()) {
      if (!Config.NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Discarding message because topic has no available partitions: "
            << "[" << topic << "]";
      }

      Discard(std::move(msg),
              TAnomalyTracker::TDiscardReason::NoAvailablePartitions);
      DiscardNoAvailablePartition.Increment();
    } else if (MsgRateLimiter.WouldExceedLimit(topic,
        msg->GetCreationTimestamp())) {
      if (!Config.NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Discarding message due to rate limit: [" << topic << "]";
      }

      Discard(std::move(msg), TAnomalyTracker::TDiscardReason::RateLimit);
      DiscardDueToRateLimit.Increment();
    }
  }

  return true;
}

void TRouterThread::ValidateBeforeReroute(std::list<TMsg::TPtr> &msg_list) {
  assert(this);
  assert(!msg_list.empty());
  const std::string &topic = msg_list.front()->GetTopic();
  int topic_index = Metadata->FindTopicIndex(topic);

  if (topic_index < 0) {
    if (!Config.NoLogDiscard) {
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Discarding message due to unknown topic on reroute: ["
          << topic << "]";
    }

    for (TMsg::TPtr &msg : msg_list) {
      AnomalyTracker.TrackBadTopicDiscard(msg);
    }

    MsgStateTracker.MsgEnterProcessed(msg_list);
    DiscardBadTopicOnReroute.Increment();
    msg_list.clear();
  } else {
    const std::vector<TMetadata::TTopic> &topic_vec = Metadata->GetTopics();
    assert((topic_index >= 0) &&
           (static_cast<size_t>(topic_index) < topic_vec.size()));
    const TMetadata::TTopic &topic_meta = topic_vec[topic_index];
    const std::vector<TMetadata::TPartition> &partition_vec =
        topic_meta.GetOkPartitions();

    if (partition_vec.empty()) {
      if (!Config.NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Discarding message because topic has no available partitions "
            << "on reroute: [" << topic << "]";
      }

      Discard(std::move(msg_list),
              TAnomalyTracker::TDiscardReason::NoAvailablePartitions);
      DiscardNoAvailablePartitionOnReroute.Increment();
    }
  }
}

size_t TRouterThread::LookupValidTopicIndex(const std::string &topic) const {
  assert(this);
  assert(Metadata);
  int topic_index = Metadata->FindTopicIndex(topic);

  if (topic_index < 0) {
    /* This should never happen, since the topic is assumed to be present in
       the metadata. */
    Die("LookupValidTopicIndex() got unknown topic");
  }

  if (static_cast<size_t>(topic_index) >= Metadata->GetTopics().size()) {
    Die("Out of range topic index in ChooseAnyPartitionBrokerIndex()");
  }

  return static_cast<size_t>(topic_index);
}

size_t TRouterThread::ChooseAnyPartitionBrokerIndex(const std::string &topic) {
  assert(this);
  assert(Metadata);

  /* When we update our metadata, we delete from the batcher any topics that
     are no longer present or have no available partitions.  Therefore all
     messages we get from the batcher will have valid topics and at least one
     available partition.  In general, all topics are validated before routing,
     so parameter 'topic' should always be valid.  */
  size_t topic_index = LookupValidTopicIndex(topic);

  const std::vector<TMetadata::TTopic> &topic_vec = Metadata->GetTopics();
  const TMetadata::TTopic &topic_meta = topic_vec[topic_index];
  const std::vector<TMetadata::TPartition> &partition_vec =
      topic_meta.GetOkPartitions();
  assert(!partition_vec.empty());

  /* Choose a broker by round-robin selection based on partitions.  Then the
     frequency of choosing a given broker will be proportional to the fraction
     of the topic's total partition count that is assigned to the broker.  We
     don't do partition selection here.  That is deferred until the connector
     thread for the chosen broker is preparing a produce request to be sent.
     The partition chosen by the connector thread may differ from the one
     chosen here.  The connector thread chooses a partition from all available
     partitions assigned to its broker that match the message topic.  This
     approach allows the connector thread to decide how frequently it rotates
     through the partitions for a topic assigned to its broker. */
  assert(RouteCounters.size() == topic_vec.size());
  const TMetadata::TPartition &partition =
      partition_vec[++RouteCounters[topic_index] % partition_vec.size()];
  return partition.GetBrokerIndex();
}

const TMetadata::TPartition &TRouterThread::ChoosePartitionByKey(
    const TMetadata::TTopic &topic_meta, int32_t partition_key) {
  assert(this);
  assert(Metadata);
  const std::vector<TMetadata::TBroker> &broker_vec = Metadata->GetBrokers();
  assert(!broker_vec.empty());
  const std::vector<TMetadata::TPartition> &partition_vec =
      topic_meta.GetAllPartitions();
  assert(!partition_vec.empty());
  size_t start_index =
      static_cast<uint32_t>(partition_key) % partition_vec.size();
  size_t index = start_index;

  do {
    const TMetadata::TPartition &partition = partition_vec[index];
    size_t broker_index = partition.GetBrokerIndex();
    assert(broker_index < broker_vec.size());

    if (broker_vec[broker_index].IsInService()) {
      return partition;
    }

    index = (index + 1) % partition_vec.size();
  } while (index != start_index);

  /* This should never happen, since before routing, we verify that a topic has
     at least one available partition. */
  Die("ChoosePartitionByKey() found no in service partitions");
}

size_t TRouterThread::AssignBroker(TMsg::TPtr &msg) {
  assert(this);
  RouteSingleMsg.Increment();
  const std::string &topic = msg->GetTopic();

  if (msg->GetRoutingType() == TMsg::TRoutingType::PartitionKey) {
    RouteSinglePartitionKeyMsg.Increment();
    const TMetadata::TPartition &partition =
        ChoosePartitionByKey(topic, msg->GetPartitionKey());
    msg->SetPartition(partition.GetId());
    return partition.GetBrokerIndex();
  }

  RouteSingleAnyPartitionMsg.Increment();

  /* Don't set the partition here.  For AnyPartition messages, partition
     selection is done by the connector thread, right before sending to Kafka.
   */
  return ChooseAnyPartitionBrokerIndex(topic);
}

void TRouterThread::Route(TMsg::TPtr &&msg) {
  assert(this);
  size_t broker_index = AssignBroker(msg);
  Dispatcher.Dispatch(std::move(msg), broker_index);
}

void TRouterThread::RouteNow(TMsg::TPtr &&msg) {
  assert(this);
  size_t broker_index = AssignBroker(msg);
  Dispatcher.DispatchNow(std::move(msg), broker_index);
}

void TRouterThread::RouteAnyPartitionNow(
    std::list<std::list<TMsg::TPtr>> &&batch_list) {
  assert(this);

  if (batch_list.empty()) {
    return;
  }

  RouteMsgBatchList.Increment();

  /* Map batches to brokers. */
  while (!batch_list.empty()) {
    auto iter = batch_list.begin();
    assert(!(*iter).empty());
    size_t broker_index =
        ChooseAnyPartitionBrokerIndex(iter->front()->GetTopic());
    auto &to_broker = TmpBrokerMap[broker_index];
    to_broker.splice(to_broker.end(), batch_list, iter);
  }

  /* Dispatch to brokers. */
  for (auto &item : TmpBrokerMap) {
    if (!item.second.empty()) {
      Dispatcher.DispatchNow(std::move(item.second), item.first);
    }

    assert(item.second.empty());
  }
}

void TRouterThread::RoutePartitionKeyNow(
    std::list<std::list<TMsg::TPtr>> &&batch_list) {
  assert(this);
  assert(Metadata);

  if (batch_list.empty()) {
    return;
  }

  /* Key is broker index (not ID), and value is list of messages with mixed
     topics. */
  std::unordered_map<size_t, std::list<TMsg::TPtr>>
      broker_map(Metadata->GetBrokers().size());

  for (auto &batch : batch_list) {
    assert(!batch.empty());

    /* Topics are checked for validity before routing, so we know the topic is
       valid. */
    const TMetadata::TTopic &topic_meta =
        GetValidTopicMetadata(batch.front()->GetTopic());

    for (auto &msg_ptr : batch) {
      assert(msg_ptr);
      const TMetadata::TPartition &partition =
          ChoosePartitionByKey(topic_meta, msg_ptr->GetPartitionKey());
      msg_ptr->SetPartition(partition.GetId());
      broker_map[partition.GetBrokerIndex()].push_back(std::move(msg_ptr));
    }
  }

  batch_list.clear();
  TTopicMap topic_map;

  for (auto &item : broker_map) {
    assert(topic_map.IsEmpty());

    for (auto &msg_ptr : item.second) {
      topic_map.Put(std::move(msg_ptr));
      assert(!msg_ptr);
    }

    /* Dispatch messages grouped by topic. */
    Dispatcher.DispatchNow(topic_map.Get(), item.first);
  }
}

void TRouterThread::Reroute(std::list<std::list<TMsg::TPtr>> &&batch_list) {
  assert(this);

  if (batch_list.empty()) {
    return;
  }

  std::list<std::list<TMsg::TPtr>> partition_key_batches;
  std::list<TMsg::TPtr> tmp;

  /* Separate PartitionKey messages from AnyPartition messages. */
  for (auto iter = batch_list.begin(), next = iter;
       iter != batch_list.end();
       iter = next) {
    ++next;
    std::list<TMsg::TPtr> &batch = *iter;
    ValidateBeforeReroute(batch);

    /* Move all PartitionKey messages to 'partition_key_batches', since they
       must be treated separately. */

    assert(tmp.empty());

    for (auto iter2 = batch.begin(), next2 = iter2;
         iter2 != batch.end();
         iter2 = next2) {
      ++next2;
      assert((*iter2)->GetTopic() == batch.front()->GetTopic());

      if ((*iter2)->GetRoutingType() == TMsg::TRoutingType::PartitionKey) {
        tmp.splice(tmp.end(), batch, iter2);
      }
    }

    if (!tmp.empty()) {
      partition_key_batches.push_back(std::move(tmp));
    }

    if (batch.empty()) {
      /* Either the above call to ValidateBeforeReroute() emptied the batch, or
         the batch became empty when we removed all PartitionKey messages. */
      batch_list.erase(iter);
    }
  }

  RouteAnyPartitionNow(std::move(batch_list));
  RoutePartitionKeyNow(std::move(partition_key_batches));
  assert(batch_list.empty());
  assert(partition_key_batches.empty());
}

void TRouterThread::RouteFinalMsgs() {
  assert(this);
  assert(Metadata);

  if (PerTopicBatcher.IsEnabled()) {
    RouteAnyPartitionNow(PerTopicBatcher.GetAllBatches());
  }

  /* Get any remaining queued messages from the input thread. */
  std::list<TMsg::TPtr> msg_list = MsgChannel.NonblockingGet();

  bool keep_running = true;

  for (TMsg::TPtr &msg : msg_list) {
    keep_running = ValidateNewMsg(msg);

    if (!keep_running) {
      break;
    }

    if (msg) {
      DebugLogger.LogMsg(msg);
      RouteNow(std::move(msg));
    }

    assert(!msg);
  }

  if (!keep_running) {
    /* The shutdown timeout expired while we were updating metadata after
       automatic topic creation.  Discard all remaining messages before we shut
       down. */
    for (TMsg::TPtr &msg : msg_list) {
      if (msg) {
        DiscardOnShutdownDuringMetadataUpdate(std::move(msg));
      }
    }
  }
}

void TRouterThread::DiscardFinalMsgs() {
  assert(this);
  std::list<TMsg::TPtr> msg_list;

  /* Get any remaining queued messages from the input thread. */
  msg_list.splice(msg_list.end(), MsgChannel.NonblockingGet());

  for (TMsg::TPtr &msg : msg_list) {
    if (msg) {
      if (!Config.NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Discarding message queued for router thread on server "
            << "shutdown: topic [" << msg->GetTopic() << "]";
      }

      Discard(std::move(msg), TAnomalyTracker::TDiscardReason::ServerShutdown);
    } else {
      LOG(TPri::ERR)
          << "Router thread got empty TMsg::TPtr in DiscardFinalMsgs()";
      LogStackTrace(TPri::ERR);
      assert(false);
    }
  }
}

void TRouterThread::InitWireProtocol() {
  assert(this);

  /* This code is just a placeholder, since Dory currently supports only
     version 0 of the metadata and produce wire protocols.  Eventually code
     will go here that handles cases where a specific metadata or produce
     protocol version was not specified as a command line arg.  In this case,
     we will probe the Kafka cluster and choose the highest version supported
     by both Dory and the Kafka brokers. */
  size_t metadata_api_version = Config.MetadataApiVersion.IsKnown() ?
      *Config.MetadataApiVersion : 0;
  std::unique_ptr<TMetadataProtocol> metadata_protocol(
      ChooseMetadataProto(metadata_api_version));
  assert(metadata_protocol);
  size_t produce_api_version = Config.ProduceApiVersion.IsKnown() ?
      *Config.ProduceApiVersion : 0;
  std::unique_ptr<TProduceProtocol> produce_protocol(
      ChooseProduceProto(produce_api_version));
  assert(produce_protocol);

  MetadataFetcher.reset(new TMetadataFetcher(metadata_protocol.release()));
  SingleMsgOverhead = produce_protocol->GetSingleMsgOverhead();
  Dispatcher.SetProduceProtocol(produce_protocol.release());
}

bool TRouterThread::Init() {
  assert(this);
  InitWireProtocol();
  std::shared_ptr<TMetadata> meta;

  LOG(TPri::NOTICE) << "Router thread sending initial metadata request";
  meta = GetInitialMetadata();

  if (!meta) {
    LOG(TPri::NOTICE)
        << "Router thread got shutdown request while getting initial metadata";

    /* Discard any remaining queued messages from the input thread.

       TODO: Examine what input thread does in this case.  This may not be
       necessary. */
    DiscardFinalMsgs();

    return false;
  }

  SetMetadata(std::move(meta));

  LOG(TPri::NOTICE)
      << "Router thread starting dispatcher during initialization";
  Dispatcher.Start(Metadata);

  PauseRateLimiter.reset(new TDoryRateLimiter(Config.PauseRateLimitInitial,
      Config.PauseRateLimitMaxDouble, Config.MinPauseDelay, GetRandomNumber));
  InitMetadataRefreshTimer();
  LOG(TPri::NOTICE) << "Router thread finished initialization";
  InitFinishedSem.Push();
  return true;
}

void TRouterThread::CheckDispatcherShutdown() {
  assert(this);
  Dispatcher.JoinAll();

  if (Dispatcher.ShutdownWasOk()) {
    LOG(TPri::INFO) << "Dispatcher terminated normally";
  } else {
    LOG(TPri::ERR) << "Dispatcher terminated on error";
  }
}

bool TRouterThread::ReplaceMetadataOnRefresh(
    std::shared_ptr<TMetadata> &&meta) {
  assert(this);
  std::shared_ptr<TMetadata> md = std::move(meta);
  LOG(TPri::NOTICE)
      << "Router thread starting fast dispatcher shutdown for metadata "
      << "refresh";
  Dispatcher.StartFastShutdown();
  LOG(TPri::NOTICE)
      << "Router thread started fast dispatcher shutdown for metadata refresh";

  if (!md) {
    LOG(TPri::NOTICE) << "Starting metadata fetch 2";
    md = GetMetadata();
    LOG(TPri::NOTICE) << "Finished metadata fetch 2";

    if (md) {
      MetadataTimestamp.RecordUpdate(true);
    }
  }

  LOG(TPri::NOTICE) << "Waiting for dispatcher shutdown to finish";
  CheckDispatcherShutdown();
  LOG(TPri::NOTICE)
      << "Router thread finished waiting for dispatcher shutdown on metadata "
      << "refresh";

  if (!md) {
    LOG(TPri::ERR)
        << "Metadata fetch 2 cut short by shutdown delay expiration";
    return false;
  }

  SetMetadata(std::move(md), false);
  RefreshMetadataSuccess.Increment();
  std::list<std::list<TMsg::TPtr>> to_reroute = EmptyDispatcher();
  LOG(TPri::NOTICE)
      << "Router thread finished metadata fetch for refresh: starting "
      << "dispatcher";
  Dispatcher.Start(Metadata);
  LOG(TPri::NOTICE) << "Router thread started dispatcher";
  Reroute(std::move(to_reroute));
  InitMetadataRefreshTimer();
  return true;
}

/* Return true on success, or false if we got a shutdown signal and the
   shutdown delay expired while trying to refresh metadata. */
bool TRouterThread::RefreshMetadata() {
  assert(this);
  assert(ShutdownStartTime.IsUnknown());
  std::shared_ptr<TMetadata> meta;

  if (!Config.SkipCompareMetadataOnRefresh) {
    LOG(TPri::INFO) << "Starting metadata fetch 1";
    meta = GetMetadata();
    LOG(TPri::INFO) << "Finished metadata fetch 1";

    if (!meta) {
      LOG(TPri::ERR)
          << "Metadata fetch 1 cut short by shutdown delay expiration";
      return false;
    }

    bool unchanged = (*meta == *Metadata);
    MetadataTimestamp.RecordUpdate(!unchanged);

    if (unchanged) {
      MetadataUnchangedOnRefresh.Increment();
      LOG(TPri::INFO) << "Metadata is unchanged on refresh";
      InitMetadataRefreshTimer();
      return true;
    }

    MetadataChangedOnRefresh.Increment();
  }

  return ReplaceMetadataOnRefresh(std::move(meta));
}

std::list<std::list<TMsg::TPtr>> TRouterThread::EmptyDispatcher() {
  assert(this);
  std::vector<std::list<std::list<TMsg::TPtr>>> broker_lists;
  size_t broker_count = Dispatcher.GetBrokerCount();
  broker_lists.reserve(broker_count);
  std::list<std::list<TMsg::TPtr>> tmp;

  for (size_t i = 0; i < broker_count; ++i) {
    tmp = Dispatcher.GetNoAckQueueAfterShutdown(i);

    for (const std::list<TMsg::TPtr> &msg_list : tmp) {
      for (const TMsg::TPtr &msg : msg_list) {
        /* We are resending a message that we previously sent but didn't get an
           ACK for.  Track this event, since it may cause a duplicate message.
         */

        if (!Config.NoLogDiscard) {
          LOG_R(TPri::WARNING, std::chrono::seconds(30))
              << "Possible duplicate message (topic: [" << msg->GetTopic()
              << "])";
        }

        AnomalyTracker.TrackDuplicate(msg);
        PossibleDuplicateMsg.Increment();
      }
    }

    tmp.splice(tmp.end(), Dispatcher.GetSendWaitQueueAfterShutdown(i));

    if (!tmp.empty()) {
      broker_lists.push_back(std::move(tmp));
    }
  }

  std::list<std::list<TMsg::TPtr>> result;

  /* Build the result by cycling through the broker lists, each time taking the
     front item.  This is a bit more complicated than simply concatenating the
     broker lists, but it will tend to do a better job of preserving the
     ordering of the messages. */
  while (!broker_lists.empty()) {
    size_t nonempty_count = broker_lists.size();

    for (size_t i = nonempty_count; i; ) {
      --i;
      std::list<std::list<TMsg::TPtr>> &current_list = broker_lists[i];
      assert(!current_list.empty());
      result.splice(result.end(), current_list, current_list.begin());

      if (current_list.empty()) {
        --nonempty_count;
        current_list.swap(broker_lists[nonempty_count]);
        assert((i == nonempty_count) ||
               ((i < nonempty_count) && !current_list.empty()));
      }
    }

    broker_lists.resize(nonempty_count);
  }

  return result;
}

bool TRouterThread::RespondToPause() {
  assert(this);
  RouterThreadStartPause.Increment();

  if (!HandlePause()) {
    /* Shutdown delay expired while getting metadata.  The dispatcher is
       already shut down, so we are finished. */

    std::list<std::list<TMsg::TPtr>> to_discard = EmptyDispatcher();

    for (const std::list<TMsg::TPtr> &msg_list : to_discard) {
      assert(!msg_list.empty());

      if (!Config.NoLogDiscard) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Router thread discarding message with topic ["
            << msg_list.front()->GetTopic()
            << "] on shutdown delay expiration during pause";
      }
    }

    Discard(std::move(to_discard),
            TAnomalyTracker::TDiscardReason::ServerShutdown);
    return false;
  }

  /* We successfully handled the pause.  Since we just got metadata, restart
     the metadata refresh timer. */
  InitMetadataRefreshTimer();

  RouterThreadFinishPause.Increment();
  return true;
}

void TRouterThread::DiscardOnShutdownDuringMetadataUpdate(TMsg::TPtr &&msg) {
  assert(this);

  if (!Config.NoLogDiscard) {
    LOG_R(TPri::ERR, std::chrono::seconds(30))
        << "Router thread discarding message with topic [" << msg->GetTopic()
        << "] on shutdown delay expiration during metadata update";
  }

  Discard(std::move(msg), TAnomalyTracker::TDiscardReason::ServerShutdown);
}

void TRouterThread::DiscardOnShutdownDuringMetadataUpdate(
    std::list<TMsg::TPtr> &&msg_list) {
  assert(this);
  std::list<TMsg::TPtr> to_discard(std::move(msg_list));

  for (TMsg::TPtr &msg : to_discard) {
    DiscardOnShutdownDuringMetadataUpdate(std::move(msg));
  }
}

void TRouterThread::DiscardOnShutdownDuringMetadataUpdate(
    std::list<std::list<TMsg::TPtr>> &&batch_list) {
  assert(this);
  std::list<std::list<TMsg::TPtr>> to_discard(std::move(batch_list));

  for (std::list<TMsg::TPtr> &batch : to_discard) {
    DiscardOnShutdownDuringMetadataUpdate(std::move(batch));
  }
}

bool TRouterThread::HandleMetadataUpdate() {
  assert(this);

  if (MetadataUpdateRequestSem.GetFd().IsReadable()) {
    MetadataUpdateRequestSem.Pop();
    LOG(TPri::NOTICE)
        << "Router thread responding to user-initiated metadata update "
        << "request";
  }

  StartRefreshMetadata.Increment();
  bool keep_running = true;

  if (!RefreshMetadata()) {
    /* Shutdown delay expired while getting metadata.  The dispatcher is
       already shut down, so we are finished. */
    DiscardOnShutdownDuringMetadataUpdate(EmptyDispatcher());
    keep_running = false;
  }

  FinishRefreshMetadata.Increment();
  return keep_running;
}

void TRouterThread::ContinueShutdown() {
  assert(this);
  NeedToContinueShutdown = false;

  /* Start watching for slow shutdown finish notification.  Stop watching for
     shutdown request and messages from the input thread.  Likewise, stop
     watching for metadata refresh events.
   */
  MetadataRefreshTimer.reset();

  /* Get any remaining queued messages from the input thread and forward them
     to the brokers.  When the brokers get the slow shutdown message, they will
     expect to receive no more messages, and will terminate once their queues
     are empty or the shutdown period expires. */
  RouteFinalMsgs();

  LOG(TPri::NOTICE)
      << "Router thread forwarding shutdown request to dispatcher";
  Dispatcher.StartSlowShutdown(*ShutdownStartTime);
  LOG(TPri::NOTICE)
      << "Router thread finished forwarding shutdown request to dispatcher";
}

int TRouterThread::ComputeMainLoopPollTimeout() {
  assert(this);

  if (OptNextBatchExpiry.IsUnknown()) {
    return -1;  // infinite timeout
  }

  auto expiry = static_cast<uint64_t>(*OptNextBatchExpiry);
  uint64_t now = GetEpochMilliseconds();

  if (expiry <= now) {
    return 0;
  }

  uint64_t delta = expiry - now;

  if (delta > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    LOG(TPri::WARNING)
        << "Likely bug: batch timeout is ridiculously large: expiry " << expiry
        << " now " << now;
    OptNextBatchExpiry.Reset();
    OptNextBatchExpiry.MakeKnown(now);
    return 0;
  }

  return static_cast<int>(delta);
}

void TRouterThread::InitMainLoopPollArray() {
  assert(this);
  struct pollfd &pause_item = MainLoopPollArray[TMainLoopPollItem::Pause];
  struct pollfd &shutdown_request_item =
      MainLoopPollArray[TMainLoopPollItem::ShutdownRequest];
  struct pollfd &msg_available_item =
      MainLoopPollArray[TMainLoopPollItem::MsgAvailable];
  struct pollfd &md_update_request_item =
      MainLoopPollArray[TMainLoopPollItem::MdUpdateRequest];
  struct pollfd &md_refresh_item =
      MainLoopPollArray[TMainLoopPollItem::MdRefresh];
  struct pollfd &shutdown_finished_item =
      MainLoopPollArray[TMainLoopPollItem::ShutdownFinished];
  bool shutdown_started = ShutdownStartTime.IsKnown();
  pause_item.fd = Dispatcher.GetPauseFd();
  pause_item.events = POLLIN;
  pause_item.revents = 0;
  shutdown_request_item.fd = GetShutdownRequestFd();
  shutdown_request_item.events = POLLIN;
  shutdown_request_item.revents = 0;
  msg_available_item.fd = shutdown_started ?
      -1 : int(MsgChannel.GetMsgAvailableFd());
  msg_available_item.events = POLLIN;
  msg_available_item.revents = 0;
  md_update_request_item.fd = MetadataUpdateRequestSem.GetFd();
  md_update_request_item.events = POLLIN;
  md_update_request_item.revents = 0;
  md_refresh_item.fd = shutdown_started ?
      -1 : int(MetadataRefreshTimer->GetFd());
  md_refresh_item.events = POLLIN;
  md_refresh_item.revents = 0;
  shutdown_finished_item.fd = shutdown_started ?  
      int(Dispatcher.GetShutdownWaitFd()) : -1;
  shutdown_finished_item.events = POLLIN;
  shutdown_finished_item.revents = 0;
}

void TRouterThread::DoRun() {
  assert(this);
  OkShutdown = false;

  if (!Init()) {
    /* Got shutdown signal during initialization.  This is not an error. */
    OkShutdown = true;
    return;
  }

  for (; ; ) {
    if (NeedToContinueShutdown) {
      ContinueShutdown();
    }

    InitMainLoopPollArray();
    IfLt0(poll(MainLoopPollArray, MainLoopPollArray.Size(),
               ComputeMainLoopPollTimeout()));

    if (MainLoopPollArray[TMainLoopPollItem::ShutdownRequest].revents) {
      StartShutdown();
    }

    if (MainLoopPollArray[TMainLoopPollItem::ShutdownFinished].revents) {
      /* TODO: Consider fixing things so that if a pause occurs during a slow
         shutdown and there is still plenty of time left before shutdown time
         limit expiration, we handle the pause rather than terminating early.
         This may not be worth dealing with. */
      HandleShutdownFinished();
      break;
    }

    if (MainLoopPollArray[TMainLoopPollItem::Pause].revents &&
        !RespondToPause()) {
      break;  // shutdown delay expired during pause
    }

    if ((MainLoopPollArray[TMainLoopPollItem::MdUpdateRequest].revents ||
         MainLoopPollArray[TMainLoopPollItem::MdRefresh].revents) &&
        !HandleMetadataUpdate()) {
      break;  // shutdown delay expired during metadata update
    }

    uint64_t now = GetEpochMilliseconds();

    if (OptNextBatchExpiry.IsKnown() &&
        (now >= static_cast<uint64_t>(*OptNextBatchExpiry))) {
      HandleBatchExpiry(now);
    }

    if (MainLoopPollArray[TMainLoopPollItem::MsgAvailable].revents) {
      HandleMsgAvailable(now);
    }
  }

  Discard(PerTopicBatcher.GetAllBatches(),
          TAnomalyTracker::TDiscardReason::ServerShutdown);
  OkShutdown = true;
}

void TRouterThread::HandleShutdownFinished() {
  assert(this);

  if (ShutdownStartTime.IsKnown()) {
    LOG(TPri::NOTICE)
        << "Router thread got shutdown finished notification from dispatcher";
  } else {
    LOG(TPri::ERR)
        << "Router thread got unexpected shutdown finished notification from "
        << "dispatcher";
  }

  CheckDispatcherShutdown();
  std::list<std::list<TMsg::TPtr>> to_discard = EmptyDispatcher();

  for (const std::list<TMsg::TPtr> &msg_list : to_discard) {
    assert(!msg_list.empty());

    if (!Config.NoLogDiscard) {
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Router thread discarding message with topic ["
          << msg_list.front()->GetTopic() << " on shutdown";
    }
  }

  Discard(std::move(to_discard),
          TAnomalyTracker::TDiscardReason::ServerShutdown);
}

void TRouterThread::HandleBatchExpiry(uint64_t now) {
  assert(this);
  assert(PerTopicBatcher.IsEnabled());
  BatchExpiryDetected.Increment();
  RouteAnyPartitionNow(PerTopicBatcher.GetCompleteBatches(now));
  OptNextBatchExpiry = PerTopicBatcher.GetNextCompleteTime();

  if (OptNextBatchExpiry.IsKnown()) {
    SetBatchExpiry.Increment();
  }
}

void TRouterThread::HandleMsgAvailable(uint64_t now) {
  assert(this);
  RouterThreadGetMsgList.Increment();
  std::list<std::list<TMsg::TPtr>> ready_batches;
  std::list<TMsg::TPtr> msg_list = MsgChannel.Get();
  std::list<TMsg::TPtr> remaining;
  bool keep_running = true;

  for (auto iter = msg_list.begin(), next = iter;
       iter != msg_list.end();
       iter = next) {
    ++next;
    TMsg::TPtr &msg_ptr = *iter;
    keep_running = ValidateNewMsg(msg_ptr);

    if (!keep_running) {
      break;
    }

    if (!msg_ptr) {
      continue;
    }

    DebugLogger.LogMsg(msg_ptr);

    /* For AnyPartition messages, per topic batching is done here, before we
       choose a destination broker.  For PartitionKey messages, it is done
       after we choose a broker (since the partition key determines the
       broker). */
    if ((msg_ptr->GetRoutingType() == TMsg::TRoutingType::AnyPartition) &&
        PerTopicBatcher.IsEnabled()) {
      TMsg &msg = *msg_ptr;
      ready_batches.splice(ready_batches.end(),
                           PerTopicBatcher.AddMsg(std::move(msg_ptr), now));

      /* Note: msg_ptr may still contain the message here, since the batcher
         only accepts messages when appropriate.  If msg_ptr is empty, then the
         batcher now contains the message so we transition its state to
         batching. */
      if (!msg_ptr) {
        MsgStateTracker.MsgEnterBatching(msg);
      }

      OptNextBatchExpiry = PerTopicBatcher.GetNextCompleteTime();

      if (OptNextBatchExpiry.IsKnown()) {
        SetBatchExpiry.Increment();
      }
    }

    if (msg_ptr) {
      remaining.splice(remaining.end(), msg_list, iter);
    } else {
      PerTopicBatchAnyPartition.Increment();
    }
  }

  if (keep_running) {
    RouteAnyPartitionNow(std::move(ready_batches));

    for (TMsg::TPtr &msg_ptr : remaining) {
      Route(std::move(msg_ptr));
    }
  } else {
    /* Shutdown delay expired while fetching metadata due to topic autocreate.
       Discard all remaining messages. */
    for (TMsg::TPtr &msg_ptr : msg_list) {
      if (msg_ptr) {
        DiscardOnShutdownDuringMetadataUpdate(std::move(msg_ptr));
      }
    }

    DiscardOnShutdownDuringMetadataUpdate(std::move(remaining));
    DiscardOnShutdownDuringMetadataUpdate(std::move(ready_batches));
  }
}

bool TRouterThread::HandlePause() {
  assert(this);

  /* Impose a delay before handling a pause that occurs shortly after a
     previous pause.  If something goes seriously wrong, this prevents us from
     going into a tight pause loop. */
  size_t delay = PauseRateLimiter->ComputeDelay();
  LOG(TPri::NOTICE) << "Router thread detected pause: waiting " << delay
      << " ms before responding";
  SleepMilliseconds(delay);
  PauseRateLimiter->OnAction();

  LOG(TPri::NOTICE) << "Router thread shutting down dispatcher on pause";
  Dispatcher.StartFastShutdown();
  LOG(TPri::NOTICE) << "Router thread waiting for dispatcher shutdown";
  CheckDispatcherShutdown();
  bool shutdown_previously_started = ShutdownStartTime.IsKnown();
  LOG(TPri::NOTICE) << "Router thread getting metadata in response to pause";
  std::shared_ptr<TMetadata> meta = GetMetadata();

  if (!meta) {
    LOG(TPri::NOTICE) << "Shutdown delay expired while getting metadata";
    return false;
  }

  SetMetadata(std::move(meta));
  LOG(TPri::NOTICE)
      << "Router thread got metadata in response to pause: starting "
      << "dispatcher";
  std::list<std::list<TMsg::TPtr>> to_reroute = EmptyDispatcher();
  Dispatcher.Start(Metadata);
  LOG(TPri::NOTICE) << "Router thread started new dispatcher";
  Reroute(std::move(to_reroute));

  if (ShutdownStartTime.IsKnown()) {
    if (!shutdown_previously_started) {
      /* We received the shutdown request while fetching metadata.  Get any
         remaining queued messages from the input thread and forward them to
         the brokers.  When the brokers get the slow shutdown message, they
         will expect to receive no more messages, and will terminate once their
         queues are empty or the shutdown period expires. */
      RouteFinalMsgs();
    }

    /* Notify the dispatcher that a slow shutdown is in progress.  If the
       shutdown was already in progress before the pause, the dispatcher will
       get the original start time, and therefore set its deadline correctly.
     */
    LOG(TPri::NOTICE)
        << "Router thread resending shutdown request to restarted dispatcher";
    Dispatcher.StartSlowShutdown(*ShutdownStartTime);
    LOG(TPri::NOTICE)
        << "Router thread resent shutdown request to restarted dispatcher";
  }

  return true;
}

void TRouterThread::UpdateKnownBrokers(const TMetadata &md) {
  assert(this);
  std::vector<TKafkaBroker> broker_vec;
  const std::vector<TMetadata::TBroker> &new_brokers = md.GetBrokers();

  for (const TMetadata::TBroker &b : new_brokers) {
    broker_vec.emplace_back(b.GetHostname(), b.GetPort());
  }

  KnownBrokers = std::move(broker_vec);
}

std::shared_ptr<TMetadata> TRouterThread::TryGetMetadata() {
  assert(this);
  assert(!KnownBrokers.empty());
  TMetadataFetcher::TDisconnecter disconnecter(*MetadataFetcher);
  size_t chosen = std::rand() % KnownBrokers.size();
  std::shared_ptr<TMetadata> result;

  for (size_t i = 0;
       i < KnownBrokers.size();
       chosen = ((chosen + 1) % KnownBrokers.size()), ++i) {
    const TKafkaBroker &broker = KnownBrokers[chosen];
    LOG(TPri::INFO) << "Router thread getting metadata from broker "
        << broker.Host << " port " << broker.Port;

    if (!MetadataFetcher->Connect(broker.Host, broker.Port)) {
      ConnectFailOnTryGetMetadata.Increment();
      LOG(TPri::ERR)
          << "Router thread failed to connect to broker for metadata";
      continue;
    }

    ConnectSuccessOnTryGetMetadata.Increment();
    result = std::move(
        MetadataFetcher->Fetch(
            static_cast<int>(Config.KafkaSocketTimeout) * 1000));

    if (result) {
      break;  // success
    }

    /* Failed to get metadata: try next broker. */
    LOG(TPri::ERR)
        << "Router thread did not get valid metadata response from broker";
  }

  bool success = false;

  if (result) {
    if (result->SanityCheck()) {
      success = true;
      LOG(TPri::INFO) << "Metadata sanity check passed";
      UpdateKnownBrokers(*result);
    } else {
      LOG(TPri::ERR) << "Metadata sanity check failed!!!";
      result.reset();
      assert(false);
    }
  }

  if (success) {
    GetMetadataSuccess.Increment();
  } else {
    GetMetadataFail.Increment();
  }

  return result;
}

void TRouterThread::InitMetadataRefreshTimer() {
  assert(this);
  MetadataRefreshTimer.reset(new TTimerFd(ComputeRetryDelay(
      Config.MetadataRefreshInterval * 60 * 1000, 5)));
}

std::shared_ptr<TMetadata> TRouterThread::GetInitialMetadata() {
  assert(this);
  std::shared_ptr<TMetadata> result;
  TDoryRateLimiter retry_rate_limiter(Config.PauseRateLimitInitial,
      Config.PauseRateLimitMaxDouble, Config.MinPauseDelay, GetRandomNumber);
  const TFd &shutdown_request_fd = GetShutdownRequestFd();

  for (; ; ) {
    /* TODO: Add shutdown request monitoring inside this call. */
    result = TryGetMetadata();

    if (result) {
      break;  // success
    }

    size_t delay = retry_rate_limiter.ComputeDelay();
    LOG(TPri::ERR)
        << "Initial metadata request failed for all known brokers, waiting "
        << delay << " ms before retry";

    if (shutdown_request_fd.IsReadable(static_cast<int>(delay))) {
      break;  // got shutdown signal
    }

    retry_rate_limiter.OnAction();
  }

  return result;
}

std::shared_ptr<TMetadata> TRouterThread::GetMetadataBeforeSlowShutdown() {
  assert(this);
  std::shared_ptr<TMetadata> result;
  TDoryRateLimiter retry_rate_limiter(Config.PauseRateLimitInitial,
      Config.PauseRateLimitMaxDouble, Config.MinPauseDelay, GetRandomNumber);

  /* A slow shutdown is not currently in progress, so we will watch for a
     shutdown notification while attempting to get metadata. */
  const TFd &shutdown_request_fd = GetShutdownRequestFd();

  for (; ; ) {
    /* TODO: Add shutdown request monitoring inside this call. */
    result = TryGetMetadata();

    if (result) {
      break;
    }

    size_t delay = retry_rate_limiter.ComputeDelay();
    LOG(TPri::ERR) << "Metadata request failed for all known brokers, waiting "
        << delay << " ms before retry (1)";

    if (shutdown_request_fd.IsReadable(static_cast<int>(delay))) {
      /* We got a shutdown request while waiting to retry.  We will keep
         trying, but must stop once the deadline has expired. */
      StartShutdown();
      break;
    }

    retry_rate_limiter.OnAction();
  }

  return result;
}

std::shared_ptr<TMetadata> TRouterThread::GetMetadataDuringSlowShutdown() {
  assert(this);
  std::shared_ptr<TMetadata> result;
  size_t shutdown_delay = Config.ShutdownMaxDelay;
  uint64_t finish_time = *ShutdownStartTime + shutdown_delay;
  uint64_t now = GetEpochMilliseconds();

  if (now >= finish_time) {
    return result;  // deadline expired
  }

  TDoryRateLimiter retry_rate_limiter(Config.PauseRateLimitInitial,
      Config.PauseRateLimitMaxDouble, Config.MinPauseDelay, GetRandomNumber);

  for (; ; ) {
    result = TryGetMetadata();
    now = GetEpochMilliseconds();

    if (now >= finish_time) {
      result.reset();  // deadline expired
      break;
    }

    if (result) {
      break;
    }

    uint64_t time_left = finish_time - now;
    size_t delay = retry_rate_limiter.ComputeDelay();
    LOG(TPri::ERR) << "Metadata request failed for all known brokers, waiting "
        << delay << " ms before retry (2)";
    SleepMilliseconds(delay);

    if (time_left <= delay) {
      /* Deadline expiration prevents retry. */
      break;
    }

    retry_rate_limiter.OnAction();
  }

  return result;
}

std::shared_ptr<TMetadata> TRouterThread::GetMetadata() {
  assert(this);
  std::shared_ptr<TMetadata> result;

  if (ShutdownStartTime.IsUnknown()) {
    result = std::move(GetMetadataBeforeSlowShutdown());

    if (result) {
      return result;
    }

    /* We got a shutdown request while trying to get metadata.  Keep trying,
       but stop once the deadline has expired. */
  }

  /* From here onward we handle the case where a slow shutdown is in progress.
   */
  result = std::move(GetMetadataDuringSlowShutdown());
  return result;
}

void TRouterThread::UpdateBatchStateForNewMetadata(const TMetadata &old_md,
    const TMetadata &new_md) {
  assert(this);
  std::list<TMsg::TPtr> deleted_topic_msgs, unavailable_topic_msgs;
  const std::vector<TMetadata::TTopic> &old_topic_vec = old_md.GetTopics();
  const std::vector<TMetadata::TTopic> &new_topic_vec = new_md.GetTopics();
  const std::unordered_map<std::string, size_t> &old_topic_name_map =
      old_md.GetTopicNameMap();

  for (const auto &old_item : old_topic_name_map) {
    assert(old_item.second < old_topic_vec.size());
    const TMetadata::TTopic &old_topic = old_topic_vec[old_item.second];

    if (!old_topic.GetOkPartitions().empty()) {
      int new_topic_index = new_md.FindTopicIndex(old_item.first);

      if (new_topic_index < 0) {
        deleted_topic_msgs.splice(deleted_topic_msgs.end(),
            PerTopicBatcher.DeleteTopic(old_item.first));
      } else {
        assert(static_cast<size_t>(new_topic_index) < new_topic_vec.size());

        if (new_topic_vec[new_topic_index].GetOkPartitions().empty()) {
          unavailable_topic_msgs.splice(unavailable_topic_msgs.end(),
              PerTopicBatcher.DeleteTopic(old_item.first));
        }
      }
    }
  }

  for (const TMsg::TPtr &msg : deleted_topic_msgs) {
    assert(msg);
    DiscardDeletedTopicMsg.Increment();

    if (!Config.NoLogDiscard) {
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Router thread discarding message with topic [" << msg->GetTopic()
          << "] that is not present in new metadata";
    }
  }

  for (const TMsg::TPtr &msg : unavailable_topic_msgs) {
    assert(msg);
    DiscardNoLongerAvailableTopicMsg.Increment();

    if (!Config.NoLogDiscard) {
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Router thread discarding message with topic [" << msg->GetTopic()
          << "] that has no available partitions in new metadata";
    }
  }

  for (TMsg::TPtr &msg : deleted_topic_msgs) {
    AnomalyTracker.TrackBadTopicDiscard(msg);
  }

  MsgStateTracker.MsgEnterProcessed(deleted_topic_msgs);
  Discard(std::move(unavailable_topic_msgs),
          TAnomalyTracker::TDiscardReason::NoAvailablePartitions);
}

void TRouterThread::SetMetadata(std::shared_ptr<TMetadata> &&meta,
        bool record_update) {
  assert(this);
  assert(meta);

  /* TODO: make this a lambda */
  class t_topic_exists_fn final {
    public:
    explicit t_topic_exists_fn(const TMetadata &md)
        : Md(md) {
    }

    /* Return true if 'topic' exists, or false otherwise. */
    bool operator()(const std::string &topic) {
      return (Md.FindTopicIndex(topic) >= 0);
    }

    private:
    const TMetadata &Md;
  };  // t_topic_exists_fn

  if (record_update) {
    MetadataTimestamp.RecordUpdate(true);
  }

  /* The route counters are used for round-robin broker selection.  Their
     specific values don't really matter.  All we need for each topic is a
     value to increment each time a message or batch of messages for that topic
     is routed. */
  RouteCounters.resize(meta->GetTopics().size(), 0);

  if (Metadata) {
    UpdateBatchStateForNewMetadata(*Metadata, *meta);
  }

  Metadata = std::move(meta);
  MetadataUpdated.Increment();

  MsgStateTracker.PruneTopics(
      TMsgStateTracker::TTopicExistsFn(t_topic_exists_fn(*Metadata)));

  const std::unordered_map<std::string, size_t> &topic_name_map =
      Metadata->GetTopicNameMap();
  const std::vector<TMetadata::TTopic> &topic_vec = Metadata->GetTopics();

  for (const auto &item : topic_name_map) {
    assert(item.second < topic_vec.size());
    const TMetadata::TTopic &topic = topic_vec[item.second];

    if (topic.GetOkPartitions().empty()) {
      TopicHasNoAvailablePartitions.Increment();
      LOG(TPri::WARNING) << "Topic [" << item.first
          << "] has no available partitions";
    }
  }

  TmpBrokerMap.clear();
}
