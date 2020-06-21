/* <dory/msg_dispatch/kafka_dispatcher.cc>

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

   Implements <dory/msg_dispatch/kafka_dispatcher.h>.
 */

#include <dory/msg_dispatch/kafka_dispatcher.h>

#include <base/counter.h>
#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Batch;
using namespace Dory::Conf;
using namespace Dory::Debug;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::MsgDispatch;
using namespace Dory::Util;
using namespace Log;

DEFINE_COUNTER(BugDispatchBatchOutOfRangeIndex);
DEFINE_COUNTER(BugDispatchMsgOutOfRangeIndex);
DEFINE_COUNTER(BugGetAckWaitQueueOutOfRangeIndex);
DEFINE_COUNTER(DispatchOneBatch);
DEFINE_COUNTER(DispatchOneMsg);
DEFINE_COUNTER(FinishDispatcherJoinAll);
DEFINE_COUNTER(SkipOutOfServiceBroker);
DEFINE_COUNTER(StartDispatcherFastShutdown);
DEFINE_COUNTER(StartDispatcherJoinAll);
DEFINE_COUNTER(StartDispatcherSlowShutdown);
DEFINE_COUNTER(StartKafkaDispatcher);

void TKafkaDispatcher::SetProduceProtocol(
    TProduceProtocol *protocol) noexcept {
  Ds.ProduceProtocol.reset(protocol);
}

TKafkaDispatcherApi::TState TKafkaDispatcher::GetState() const noexcept {
  return State;
}

size_t TKafkaDispatcher::GetBrokerCount() const noexcept {
  return Connectors.size();
}

void TKafkaDispatcher::Start(const std::shared_ptr<TMetadata> &md) {
  assert(md);
  assert(Ds.ProduceProtocol);
  assert(State == TState::Stopped);
  assert(!Ds.PauseButton.GetFd().IsReadable());
  assert(!Ds.GetShutdownWaitFd().IsReadable());
  assert(Ds.GetRunningThreadCount() == 0);
  StartKafkaDispatcher.Increment();
  OkShutdown = true;
  const std::vector<TMetadata::TBroker> &brokers = md->GetBrokers();
  size_t num_in_service = md->NumInServiceBrokers();

  if (num_in_service > brokers.size()) {
    assert(false);
    LOG(TPri::ERR) << "Bug!!! In service broker count " << num_in_service
        << " exceeds total broker count " << brokers.size();
    num_in_service = brokers.size();
  }

  /* The connectors are not designed to be reused.  Therefore delete all
     connectors remaining from the last dispatcher execution and create new
     ones.  Doing things this way makes the connector implementation simpler
     and less susceptible to bugs being introduced. */

  Connectors.clear();
  Connectors.resize(num_in_service);
  Ds.MarkAllThreadsRunning(num_in_service);

  for (size_t i = 0; i < Connectors.size(); ++i) {
    assert(brokers[i].IsInService());
    std::unique_ptr<TConnector> &broker_ptr = Connectors[i];
    assert(!broker_ptr);
    broker_ptr.reset(new TConnector(i, Ds));
    LOG(TPri::NOTICE) << "Starting connector thread for broker index " << i
        << " (Kafka ID " << brokers[i].GetId() << ")";
    broker_ptr->SetMetadata(md);
    broker_ptr->Start();
  }

  for (size_t i = Connectors.size(); i < brokers.size(); ++i) {
    assert(!brokers[i].IsInService());
    LOG(TPri::NOTICE) << "Skipping out of service broker index " << i
        << " (Kafka ID " << brokers[i].GetId() << ")";
    SkipOutOfServiceBroker.Increment();
  }

  State = TState::Started;
}

void TKafkaDispatcher::Dispatch(TMsg::TPtr &&msg, size_t broker_index) {
  assert(msg);
  assert(State != TState::Stopped);
  DispatchOneMsg.Increment();

  if (broker_index >= Connectors.size()) {
    assert(false);
    LOG_R(TPri::ERR, std::chrono::seconds(30))
        << "Bug!!! Cannot dispatch message because broker index is out of "
        << "range: index " << broker_index << " broker count "
        << Connectors.size();
    BugDispatchMsgOutOfRangeIndex.Increment();
    Ds.Discard(std::move(msg), TAnomalyTracker::TDiscardReason::Bug);
    return;
  }

  assert(Connectors[broker_index]);
  Connectors[broker_index]->Dispatch(std::move(msg));
  assert(!msg);
}

void TKafkaDispatcher::DispatchNow(TMsg::TPtr &&msg, size_t broker_index) {
  assert(msg);
  assert(State != TState::Stopped);
  DispatchOneMsg.Increment();

  if (broker_index >= Connectors.size()) {
    assert(false);
    LOG_R(TPri::ERR, std::chrono::seconds(30))
        << "Bug!!! Cannot dispatch message because broker index is out of "
        << "range: index " << broker_index << " broker count "
        << Connectors.size();
    BugDispatchMsgOutOfRangeIndex.Increment();
    Ds.Discard(std::move(msg), TAnomalyTracker::TDiscardReason::Bug);
    return;
  }

  assert(Connectors[broker_index]);
  Connectors[broker_index]->DispatchNow(std::move(msg));
  assert(!msg);
}

void TKafkaDispatcher::DispatchNow(std::list<std::list<TMsg::TPtr>> &&batch,
    size_t broker_index) {
  assert(State != TState::Stopped);

  if (batch.empty()) {
    return;
  }

  DispatchOneBatch.Increment();

  if (broker_index >= Connectors.size()) {
    assert(false);
    LOG_R(TPri::ERR, std::chrono::seconds(30))
        << "Bug!!! Cannot dispatch message batch because broker index is out "
        << "of range: index " << broker_index << " broker count "
        << Connectors.size();
    BugDispatchBatchOutOfRangeIndex.Increment();
    Ds.Discard(std::move(batch), TAnomalyTracker::TDiscardReason::Bug);
    return;
  }

  assert(Connectors[broker_index]);
  Connectors[broker_index]->DispatchNow(std::move(batch));
  assert(batch.empty());
}

void TKafkaDispatcher::StartSlowShutdown(uint64_t start_time) {
  assert(State != TState::Stopped);
  StartDispatcherSlowShutdown.Increment();

  if (Connectors.empty()) {
    Ds.HandleAllThreadsFinished();
  } else {
    for (std::unique_ptr<TConnector> &c : Connectors) {
      assert(c);
      c->StartSlowShutdown(start_time);
    }

    for (std::unique_ptr<TConnector> &c : Connectors) {
      assert(c);
      c->WaitForShutdownAck();
    }
  }

  State = TState::ShuttingDown;
}

void TKafkaDispatcher::StartFastShutdown() {
  assert(State != TState::Stopped);
  StartDispatcherFastShutdown.Increment();

  if (Connectors.empty()) {
    Ds.HandleAllThreadsFinished();
  } else {
    for (std::unique_ptr<TConnector> &c : Connectors) {
      assert(c);
      c->StartFastShutdown();
    }

    for (std::unique_ptr<TConnector> &c : Connectors) {
      assert(c);
      c->WaitForShutdownAck();
    }
  }

  State = TState::ShuttingDown;
}

const TFd &TKafkaDispatcher::GetPauseFd() const noexcept {
  return Ds.PauseButton.GetFd();
}

const TFd &TKafkaDispatcher::GetShutdownWaitFd() const noexcept {
  return Ds.GetShutdownWaitFd();
}

void TKafkaDispatcher::JoinAll() {
  assert(State != TState::Stopped);
  StartDispatcherJoinAll.Increment();
  LOG(TPri::NOTICE) << "Start waiting for dispatcher shutdown status";
  bool ok_shutdown = true;

  for (std::unique_ptr<TConnector> &c : Connectors) {
    assert(c);
    c->Join();
    c->CleanupAfterJoin();

    if (!c->ShutdownWasOk()) {
      ok_shutdown = false;
    }
  }

  OkShutdown = ok_shutdown;
  Ds.PauseButton.Reset();
  assert(Ds.GetShutdownWaitFd().IsReadable());
  Ds.ResetThreadFinishedState();
  FinishDispatcherJoinAll.Increment();
  LOG(TPri::NOTICE) << "Finished waiting for dispatcher shutdown status";
  State = TState::Stopped;
}

bool TKafkaDispatcher::ShutdownWasOk() const noexcept {
  return OkShutdown;
}

std::list<std::list<TMsg::TPtr>>
TKafkaDispatcher::GetNoAckQueueAfterShutdown(size_t broker_index) {
  assert(State == TState::Stopped);

  if (broker_index >= Connectors.size()) {
    LOG(TPri::ERR)
        << "Bug!!! Cannot get ACK wait queue for out of range broker index "
        << broker_index << " broker count " << Connectors.size();
    BugGetAckWaitQueueOutOfRangeIndex.Increment();
    assert(false);
    return std::list<std::list<TMsg::TPtr>>();
  }

  assert(Connectors[broker_index]);
  return Connectors[broker_index]->GetNoAckQueueAfterShutdown();
}

std::list<std::list<TMsg::TPtr>>
TKafkaDispatcher::GetSendWaitQueueAfterShutdown(size_t broker_index) {
  assert(State == TState::Stopped);

  if (broker_index >= Connectors.size()) {
    LOG(TPri::ERR)
        << "Bug!!! Cannot get send wait queue for out of range broker index "
        << broker_index << " broker count " << Connectors.size();
    assert(false);
    return std::list<std::list<TMsg::TPtr>>();
  }

  assert(Connectors[broker_index]);
  return Connectors[broker_index]->GetSendWaitQueueAfterShutdown();
}

size_t TKafkaDispatcher::GetAckCount() const noexcept {
  return Ds.GetAckCount();
}
