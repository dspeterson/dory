/* <dory/mock_kafka_server/main_thread.h>

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

   Main thread class for integrating mock Kafka server into dory unit tests.
 */

#pragma once

#include <cassert>
#include <list>

#include <netinet/in.h>
#include <signal.h>

#include <base/event_semaphore.h>
#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <dory/mock_kafka_server/cmd_line_args.h>
#include <dory/mock_kafka_server/received_request_tracker.h>
#include <dory/mock_kafka_server/server.h>
#include <thread/fd_managed_thread.h>

namespace Dory {

  namespace MockKafkaServer {

    /* This class implements the main thread for the mock Kafka server when
       integrated into unit tests for dory.  It is not used for the standalone
       mock Kafka server executable. */
    class TMainThread final : public Thread::TFdManagedThread {
      NO_COPY_SEMANTICS(TMainThread);

      public:
      explicit TMainThread(const TCmdLineArgs &args,
          int shutdown_signum = SIGINT)
          : ShutdownSignum(shutdown_signum),
            Server(args, true, true, shutdown_signum) {
       }

      ~TMainThread() override;

      void RequestShutdown() override;

      /* Return a file descriptor that becomes readable when the server has
       finished its initialization and is open for business. */
      const Base::TFd &GetInitWaitFd() const {
        assert(this);
        return InitFinishedSem.GetFd();
      }

      bool ShutdownWasOk() const {
        assert(this);
        return OkShutdown;
      }

      in_port_t GetCmdPort() const {
        assert(this);
        return Server.GetCmdPort();
      }

      /* Return the physical port corresponding to the given virtual port.  A
         return value of 0 means "mapping not found".  See big comment in
         <dory/mock_kafka_server/port_map.h> for an explanation of what is
         going on here. */
      in_port_t VirtualPortToPhys(in_port_t v_port) const {
        assert(this);
        return Server.VirtualPortToPhys(v_port);
      }

      /* Return the virtual port corresponding to the given physical port.  A
         return value of 0 means "mapping not found".  See big comment in
         <dory/mock_kafka_server/port_map.h> for an explanation of what is
         going on here. */
      in_port_t PhysicalPortToVirt(in_port_t p_port) const {
        assert(this);
        return Server.PhysicalPortToVirt(p_port);
      }

      void GetHandledRequests(
          std::list<TReceivedRequestTracker::TRequestInfo> &result) {
        assert(this);
        Server.GetHandledRequests(result);
      }

      void NonblockingGetHandledRequests(
          std::list<TReceivedRequestTracker::TRequestInfo> &result) {
        assert(this);
        Server.NonblockingGetHandledRequests(result);
      }

      protected:
      void Run() override;

      private:
      const int ShutdownSignum;

      /* Indicates whether the mock Kafka server terminated normally or with an
         error.  */
      bool OkShutdown = true;

      /* This becomes readable when the input thread has finished its
         initialization and is open for business. */
      Base::TEventSemaphore InitFinishedSem;

      /* Mock Kafka server implementation. */
      TServer Server;
    };  // TMainThread

  }  // MockKafkaServer

}  // Dory
