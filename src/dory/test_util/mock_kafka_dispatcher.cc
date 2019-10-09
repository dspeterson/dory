/* <dory/test_util/mock_kafka_dispatcher.cc>

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

   Implements <dory/test_util/mock_kafka_dispatcher.h>.
 */

#include <dory/test_util/mock_kafka_dispatcher.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Batch;
using namespace Dory::Debug;
using namespace Dory::KafkaProto;
using namespace Dory::MsgDispatch;
using namespace Dory::TestUtil;

TMockKafkaDispatcher::TMockKafkaDispatcher(const TConfig &/*config*/,
    TMsgStateTracker &/*msg_state_tracker*/,
    TAnomalyTracker &/*anomaly_trackeri*/,
    const TBatchConfig &/*batch_config*/,
    std::unordered_set<std::string> &&/*batch_topic_filter*/,
    bool /*batch_topic_filter_exclude*/, size_t /*produce_request_data_limit*/,
    const TDebugSetup &/*debug_setup*/) {
}

void TMockKafkaDispatcher::SetProduceProtocol(
          KafkaProto::Produce::TProduceProtocol * /*protocol*/) noexcept {
  assert(this);




}

TKafkaDispatcherApi::TState TMockKafkaDispatcher::GetState() const noexcept {
  assert(this);




  return TState::Stopped;
}

size_t TMockKafkaDispatcher::GetBrokerCount() const noexcept {
  assert(this);





  return 0;
}

void TMockKafkaDispatcher::Start(
    const std::shared_ptr<TMetadata> &/*md*/) {
  assert(this);





}

void TMockKafkaDispatcher::Dispatch(TMsg::TPtr &&/*msg*/,
    size_t /*broker_index*/) {
  assert(this);






}

void TMockKafkaDispatcher::DispatchNow(TMsg::TPtr &&/*msg*/,
    size_t /*broker_index*/) {
  assert(this);





}

void TMockKafkaDispatcher::DispatchNow(
    std::list<std::list<TMsg::TPtr>> &&/*batch*/, size_t /*broker_index*/) {
  assert(this);





}

void TMockKafkaDispatcher::StartSlowShutdown(uint64_t /*start_time*/) {
  assert(this);





}

void TMockKafkaDispatcher::StartFastShutdown() {
  assert(this);





}

const TFd &TMockKafkaDispatcher::GetPauseFd() const noexcept {
  assert(this);





  static TFd placeholder;
  return placeholder;
}

const TFd &TMockKafkaDispatcher::GetShutdownWaitFd() const noexcept {
  assert(this);





  static TFd placeholder;
  return placeholder;
}

void TMockKafkaDispatcher::JoinAll() {
  assert(this);





}

bool TMockKafkaDispatcher::ShutdownWasOk() const noexcept {
  assert(this);





  return true;
}

std::list<std::list<TMsg::TPtr>>
TMockKafkaDispatcher::GetNoAckQueueAfterShutdown(size_t /*broker_index*/) {
  assert(this);





  return std::list<std::list<TMsg::TPtr>>();
}

std::list<std::list<TMsg::TPtr>>
TMockKafkaDispatcher::GetSendWaitQueueAfterShutdown(size_t /*broker_index*/) {
  assert(this);





  return std::list<std::list<TMsg::TPtr>>();
}

size_t TMockKafkaDispatcher::GetAckCount() const noexcept {
  assert(this);





  return 0;
}
