/* <dory/msg_dispatch/produce_response_processor.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)
   Copyright 2015 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/msg_dispatch/produce_response_processor.h>.
 */

#include <dory/msg_dispatch/produce_response_processor.h>

#include <cassert>

#include <base/counter.h>
#include <base/gettid.h>
#include <base/no_default_case.h>
#include <dory/util/msg_util.h>
#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::MsgDispatch;
using namespace Dory::Util;
using namespace Log;

DEFINE_COUNTER(ConnectorGotDiscardAck);
DEFINE_COUNTER(ConnectorGotDiscardAndPauseAck);
DEFINE_COUNTER(ConnectorGotOkProduceResponse);
DEFINE_COUNTER(ConnectorGotPauseAck);
DEFINE_COUNTER(ConnectorGotResendAck);
DEFINE_COUNTER(ConnectorGotSuccessfulAck);
DEFINE_COUNTER(ConnectorQueueImmediateResendMsgSet);
DEFINE_COUNTER(ConnectorQueueNoAckMsgs);
DEFINE_COUNTER(ConnectorQueuePauseAndResendMsgSet);
DEFINE_COUNTER(CorrelationIdMismatch);
DEFINE_COUNTER(DiscardOnFailedDeliveryAttemptLimit);
DEFINE_COUNTER(ProduceResponseShortPartitionList);
DEFINE_COUNTER(ProduceResponseShortTopicList);
DEFINE_COUNTER(ProduceResponseUnexpectedPartition);
DEFINE_COUNTER(ProduceResponseUnexpectedTopic);

TProduceResponseProcessor::TAction
TProduceResponseProcessor::ProcessResponse(TProduceRequest &request,
    const uint8_t *response_buf, size_t response_buf_size) {
  ResponseReader.SetResponse(response_buf, response_buf_size);
  TCorrId corr_id = ResponseReader.GetCorrelationId();

  if (corr_id != request.first) {
    LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
        << Gettid() << " (index " << MyBrokerIndex << " broker " << MyBrokerId
        << ") starting pause due to correlation ID mismatch: expected "
        << request.first << " actual " << corr_id;
    CorrelationIdMismatch.Increment();

    /* The pause handling code in the router thread will reroute these messages
       after the dispatcher has restarted with new metadata. */
    ProcessNoAckMsgs(request.second);
    return TAction::PauseAndFinishNow;
  }

  TAction action = ProcessResponseAcks(request);
  assert(request.second.empty());
  return action;
}

void TProduceResponseProcessor::ReportBadResponseTopic(
    const std::string &topic) const {
  LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread " << Gettid()
      << " (index " << MyBrokerIndex << " broker " << MyBrokerId
      << ") starting pause due to produce response with unexpected topic ["
      << topic << "]";
  ProduceResponseUnexpectedTopic.Increment();
}

void TProduceResponseProcessor::ReportBadResponsePartition(
    int32_t partition) const {
  LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread " << Gettid()
      << " (index " << MyBrokerIndex << " broker " << MyBrokerId
      << ") starting pause due to produce response with unexpected partition: "
      << partition;
  ProduceResponseUnexpectedPartition.Increment();
}

void TProduceResponseProcessor::ReportShortResponsePartitionList(
    const std::string &topic) const {
  LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread " << Gettid()
      << " (index " << MyBrokerIndex << " broker " << MyBrokerId
      << ") starting pause due to produce response with short partition list "
      << "for topic [" << topic << "]";
  ProduceResponseShortPartitionList.Increment();
}

void TProduceResponseProcessor::ReportShortResponseTopicList() const {
  LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread " << Gettid()
      << " (index " << MyBrokerIndex << " broker " << MyBrokerId
      << ") starting pause due to produce response with short topic list";
  ProduceResponseShortTopicList.Increment();
}

void TProduceResponseProcessor::CountFailedDeliveryAttempt(
    std::list<TMsg::TPtr> &msg_set, const std::string &topic) {
  for (auto iter = msg_set.begin(), next = iter;
      iter != msg_set.end();
      iter = next) {
    ++next;
    TMsg::TPtr &msg = *iter;
    assert(msg->GetTopic() == topic);

    if (msg->CountFailedDeliveryAttempt() >
        Ds.Conf.MsgDeliveryConf.MaxFailedDeliveryAttempts) {
      DiscardOnFailedDeliveryAttemptLimit.Increment();

      if (Ds.Conf.LoggingConf.LogDiscards) {
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Discarding message because failed delivery attempt limit "
            << "reached (topic: [" << msg->GetTopic() << "])";
      }

      Ds.Discard(std::move(msg),
          TAnomalyTracker::TDiscardReason::FailedDeliveryAttemptLimit);
      msg_set.erase(iter);
    }
  }
}

void TProduceResponseProcessor::ProcessImmediateResendMsgSet(
    std::list<TMsg::TPtr> &&msg_set, const std::string &topic) {
  assert(!msg_set.empty());
  CountFailedDeliveryAttempt(msg_set, topic);

  if (!msg_set.empty()) {
    ConnectorQueueImmediateResendMsgSet.Increment();
    LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
        << Gettid() << " (index " << MyBrokerIndex << " broker " << MyBrokerId
        << ") queueing msg set (topic: [" << topic
        << "]) for immediate resend";
    Ds.MsgStateTracker.MsgEnterSendWait(msg_set);
    ImmediateResendAckMsgs.push_back(std::move(msg_set));
  }
}

void TProduceResponseProcessor::ProcessPauseAndResendMsgSet(
    std::list<TMsg::TPtr> &&msg_set, const std::string &topic) {
  assert(!msg_set.empty());
  CountFailedDeliveryAttempt(msg_set, topic);

  if (!msg_set.empty()) {
    ConnectorQueuePauseAndResendMsgSet.Increment();
    LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
        << Gettid() << " (index " << MyBrokerIndex << " broker " << MyBrokerId
        << ") queueing msg set (topic: [" << topic
        << "]) for resend after pause";
    Ds.MsgStateTracker.MsgEnterSendWait(msg_set);
    PauseAndResendAckMsgs.push_back(std::move(msg_set));
  }
}

void TProduceResponseProcessor::ProcessNoAckMsgs(TAllTopics &all_topics) {
  std::list<std::list<TMsg::TPtr>> tmp;
  EmptyAllTopics(all_topics, tmp);

  if (!tmp.empty()) {
    ConnectorQueueNoAckMsgs.Increment();
    LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
        << Gettid() << " (index " << MyBrokerIndex << " broker " << MyBrokerId
        << ") processing msgs without ACKs after error";
    Ds.MsgStateTracker.MsgEnterSendWait(tmp);
    MsgsWithoutAcks.splice(MsgsWithoutAcks.end(), std::move(tmp));
  }
}

bool TProduceResponseProcessor::ProcessOneAck(std::list<TMsg::TPtr> &&msg_set,
    int16_t ack, const std::string &topic) {
  assert(!msg_set.empty());
  Ds.IncrementAckCount();

  switch (Ds.ProduceProtocol->ProcessAck(ack)) {
    case TAckResultAction::Ok: {  // got successful ACK
      ConnectorGotSuccessfulAck.Increment();
      DebugLogger.LogMsgList(msg_set);
      Ds.MsgStateTracker.MsgEnterProcessed(msg_set);
      msg_set.clear();
      break;
    }
    case TAckResultAction::Resend: {
      ConnectorGotResendAck.Increment();
      LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
          << Gettid() << " (index " << MyBrokerIndex << " broker "
          << MyBrokerId
          << ") got ACK error that triggers immediate resend without pause";

      /* These messages can be immediately resent without pausing and rerouting
         based on new metadata, although some may be discarded here due to the
         failed delivery attempt limit. */
      ProcessImmediateResendMsgSet(std::move(msg_set), topic);
      break;
    }
    case TAckResultAction::Discard: {
      ConnectorGotDiscardAck.Increment();

      /* Write a log message even if Ds.Config.LogDiscard is false because
         these events are always interesting enough to be worth logging. */
      LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
          << Gettid() << " (index " << MyBrokerIndex << " broker "
          << MyBrokerId
          << ") got ACK error that triggers discard without pause: topic ["
          << msg_set.front()->GetTopic() << "], " << msg_set.size()
          << " messages in set with total data size " << GetDataSize(msg_set);

      Ds.Discard(std::move(msg_set),
          TAnomalyTracker::TDiscardReason::KafkaErrorAck);
      break;
    }
    case TAckResultAction::Pause: {
      ConnectorGotPauseAck.Increment();
      LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
          << Gettid() << " (index " << MyBrokerIndex << " broker "
          << MyBrokerId
          << ") got ACK error that triggers deferred pause";

      /* Messages may be discarded here due to the failed delivery attempt
         limit.  Messages not discarded will be rerouted after the dispatcher
         has been restarted. */
      ProcessPauseAndResendMsgSet(std::move(msg_set), topic);
      return false;
    }
    case TAckResultAction::DiscardAndPause: {
      ConnectorGotDiscardAndPauseAck.Increment();
      LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Connector thread "
          << Gettid() << " (index " << MyBrokerIndex << " broker "
          << MyBrokerId
          << ") got ACK error that triggers discard and deferred pause";
      Ds.Discard(std::move(msg_set),
          TAnomalyTracker::TDiscardReason::KafkaErrorAck);
      return false;
    }
    NO_DEFAULT_CASE;
  }

  assert(msg_set.empty());
  return true;
}

TProduceResponseProcessor::TAction
TProduceResponseProcessor::ProcessResponseAcks(TProduceRequest &request) {
  std::string topic;
  bool got_pause_ack = false;
  bool bad_response = false;
  TAllTopics &all_topics = request.second;

  while (ResponseReader.NextTopic()) {
    topic.assign(ResponseReader.GetCurrentTopicNameBegin(),
        ResponseReader.GetCurrentTopicNameEnd());
    auto topic_iter = all_topics.find(topic);

    if (topic_iter == all_topics.end()) {
      ReportBadResponseTopic(topic);
      bad_response = true;
      break;
    }

    TMultiPartitionGroup &all_partitions = topic_iter->second;

    while (ResponseReader.NextPartitionInTopic()) {
      int32_t partition = ResponseReader.GetCurrentPartitionNumber();
      auto partition_iter = all_partitions.find(partition);

      if (partition_iter == all_partitions.end()) {
        ReportBadResponsePartition(partition);
        bad_response = true;
        break;
      }

      std::list<TMsg::TPtr> &msg_set = partition_iter->second.Contents;
      assert(!msg_set.empty());

      if (!ProcessOneAck(std::move(msg_set),
              ResponseReader.GetCurrentPartitionErrorCode(), topic)) {
        got_pause_ack = true;  // we will pause, but keep processing ACKs
      }

      assert(msg_set.empty());
      all_partitions.erase(partition_iter);
    }

    if (!all_partitions.empty()) {
      ReportShortResponsePartitionList(topic);
      bad_response = true;
      break;
    }

    all_topics.erase(topic_iter);
  }

  if (!bad_response && !all_topics.empty()) {
    bad_response = true;
    ReportShortResponseTopicList();
  }

  if (bad_response) {
    ProcessNoAckMsgs(all_topics);
  }

  assert(all_topics.empty());

  if (bad_response) {
    return TAction::PauseAndFinishNow;
  }

  ConnectorGotOkProduceResponse.Increment();
  return got_pause_ack ? TAction::PauseAndDeferFinish : TAction::KeepRunning;
}
