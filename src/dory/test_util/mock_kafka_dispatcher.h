/* <dory/test_util/mock_kafka_dispatcher.h>

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

   Mock Kafka dispatcher class for unit testing.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <dory/anomaly_tracker.h>
#include <dory/batch/batch_config.h>
#include <dory/config.h>
#include <dory/debug/debug_setup.h>
#include <dory/metadata.h>
#include <dory/msg.h>
#include <dory/msg_dispatch/kafka_dispatcher_api.h>
#include <dory/msg_state_tracker.h>

namespace Dory {

  namespace TestUtil {

    /* Mock Kafka dispatcher class for unit testing. */
    class TMockKafkaDispatcher final
        : public MsgDispatch::TKafkaDispatcherApi {
      NO_COPY_SEMANTICS(TMockKafkaDispatcher);

      public:
      TMockKafkaDispatcher(const TConfig &config,
          TMsgStateTracker &msg_state_tracker,
          TAnomalyTracker &anomaly_tracker,
          const Batch::TBatchConfig &batch_config,
          std::unordered_set<std::string> &&batch_topic_filter,
          bool batch_topic_filter_exclude, size_t produce_request_data_limit,
          const Debug::TDebugSetup &debug_setup);

      ~TMockKafkaDispatcher() override = default;

      void SetProduceProtocol(
          KafkaProto::Produce::TProduceProtocol *protocol) noexcept override;

      TState GetState() const noexcept override;

      size_t GetBrokerCount() const noexcept override;

      void Start(const std::shared_ptr<TMetadata> &md) override;

      void Dispatch(TMsg::TPtr &&msg, size_t broker_index) override;

      void DispatchNow(TMsg::TPtr &&msg, size_t broker_index) override;

      void DispatchNow(std::list<std::list<TMsg::TPtr>> &&batch,
                            size_t broker_index) override;

      void StartSlowShutdown(uint64_t start_time) override;

      void StartFastShutdown() override;

      const Base::TFd &GetPauseFd() const noexcept override;

      const Base::TFd &GetShutdownWaitFd() const noexcept override;

      void JoinAll() override;

      bool ShutdownWasOk() const noexcept override;

      std::list<std::list<TMsg::TPtr>>
      GetNoAckQueueAfterShutdown(size_t broker_index) override;

      std::list<std::list<TMsg::TPtr>>
      GetSendWaitQueueAfterShutdown(size_t broker_index) override;

      size_t GetAckCount() const noexcept override;
    };  // TMockKafkaDispatcher

  }  // TestUtil

}  // Dory
