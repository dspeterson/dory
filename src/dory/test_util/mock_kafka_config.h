/* <dory/test_util/mock_kafka_config.h>

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

   Mock Kafka server configuration.
 */

#pragma once

#include <cassert>
#include <string>
#include <vector>

#include <base/no_copy_semantics.h>
#include <base/tmp_dir.h>
#include <base/tmp_file.h>
#include <dory/mock_kafka_server/cmd_line_args.h>
#include <dory/mock_kafka_server/error_injector.h>
#include <dory/mock_kafka_server/main_thread.h>

namespace Dory {

  namespace TestUtil {

    /* Mock Kafka server configuration for unit tests. */
    struct TMockKafkaConfig {
      NO_COPY_SEMANTICS(TMockKafkaConfig);

      private:
      bool KafkaStarted = false;

      public:
      Base::TTmpFile SetupFile{"/tmp/mock_kafka_setup.XXXXXX",
          true /* delete_on_destroy */};

      Base::TTmpDir OutputDir{"/tmp/mock_kafka_output_dir.XXXXXX",
          true /* delete_on_destroy */};

      std::vector<const char *> Args;

      std::unique_ptr<Dory::MockKafkaServer::TCmdLineArgs> CmdLineArgs;

      std::unique_ptr<Dory::MockKafkaServer::TMainThread> MainThread;

      Dory::MockKafkaServer::TErrorInjector Inj;

      explicit TMockKafkaConfig(const std::vector<std::string> &config_file);

      ~TMockKafkaConfig() {
        StopKafka();
      }

      void StartKafka();

      void StopKafka();
    };  // TMockKafkaConfig

  }  // TestUtil

}  // Dory
