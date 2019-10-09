/* <dory/msg_dispatch/dispatcher_shared_state.h>

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

   State shared by Kafka dispatcher and all of its threads.
 */

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

#include <base/event_semaphore.h>
#include <base/no_copy_semantics.h>
#include <dory/anomaly_tracker.h>
#include <dory/batch/global_batch_config.h>
#include <dory/conf/compression_conf.h>
#include <dory/config.h>
#include <dory/debug/debug_setup.h>
#include <dory/kafka_proto/produce/produce_protocol.h>
#include <dory/msg.h>
#include <dory/msg_state_tracker.h>
#include <dory/util/pause_button.h>

namespace Dory {

  namespace MsgDispatch {

    struct TDispatcherSharedState {
      NO_COPY_SEMANTICS(TDispatcherSharedState);

      const TConfig &Config;

      Conf::TCompressionConf CompressionConf;

      std::shared_ptr<KafkaProto::Produce::TProduceProtocol> ProduceProtocol;

      TMsgStateTracker &MsgStateTracker;

      TAnomalyTracker &AnomalyTracker;

      const Debug::TDebugSetup &DebugSetup;

      Util::TPauseButton PauseButton;

      const Batch::TGlobalBatchConfig BatchConfig;

      TDispatcherSharedState(const TConfig &config,
          const Conf::TCompressionConf &compression_conf,
          TMsgStateTracker &msg_state_tracker,
          TAnomalyTracker &anomaly_tracker,
          const Debug::TDebugSetup &debug_setup,
          const Batch::TGlobalBatchConfig &batch_config);

      size_t GetAckCount() const noexcept {
        assert(this);
        return AckCount.load();
      }

      void IncrementAckCount() noexcept {
        assert(this);
        ++AckCount;
      }

      void Discard(TMsg::TPtr &&msg, TAnomalyTracker::TDiscardReason reason);

      void Discard(std::list<TMsg::TPtr> &&msg_list,
                   TAnomalyTracker::TDiscardReason reason);

      void Discard(std::list<std::list<TMsg::TPtr>> &&batch,
                   TAnomalyTracker::TDiscardReason reason);

      const Base::TFd &GetShutdownWaitFd() const noexcept {
        assert(this);
        return ShutdownFinished.GetFd();
      }

      size_t GetRunningThreadCount() const noexcept {
        assert(this);
        return RunningThreadCount.load();
      }

      void MarkAllThreadsRunning(size_t in_service_broker_count);

      /* Called by connector threads when finished shutting down. */
      void MarkThreadFinished();

      void HandleAllThreadsFinished();

      void ResetThreadFinishedState();

      private:
      /* This is the total number of connector threads that have been started
         and have not yet called MarkShutdownFinished(); */
      std::atomic<size_t> RunningThreadCount{0};

      Base::TEventSemaphore ShutdownFinished;

      std::atomic<size_t> AckCount{0};
    };  // TDispatcherSharedState

  }  // MsgDispatch

}  // Dory
