/* <dory/test_util/mock_kafka_config.cc>

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

   Implements <dory/test_util/mock_kafka_config.h>.
 */

#include <dory/test_util/mock_kafka_config.h>

#include <signal.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/wr/fd_util.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::TestUtil;

TMockKafkaConfig::TMockKafkaConfig(
    const std::vector<std::string> &config_file) {
  for (const std::string &line : config_file) {
    IfLt0(Wr::write(SetupFile.GetFd(), line.data(), line.size()));
    IfLt0(Wr::write(SetupFile.GetFd(), "\n", 1));
  }

  Args.push_back("mock_kafka_server");
  Args.push_back("--log-echo");
  Args.push_back("--output-dir");
  Args.push_back(OutputDir.GetName().c_str());
  Args.push_back("--setup-file");
  Args.push_back(SetupFile.GetName().c_str());
  Args.push_back(nullptr);
  CmdLineArgs.reset(new Dory::MockKafkaServer::TCmdLineArgs(
      static_cast<int>(Args.size() - 1), &Args[0]));
  MainThread.reset(new Dory::MockKafkaServer::TMainThread(*CmdLineArgs,
      SIGUSR2 /* shutdown_signum */));
}

void TMockKafkaConfig::StartKafka() {
  if (!KafkaStarted) {
    MainThread->Start();
    MainThread->GetInitWaitFd().IsReadableIntr(-1);
    bool success = Inj.Connect("localhost", MainThread->GetCmdPort());
    ASSERT_TRUE(success);
    KafkaStarted = true;
  }
}

void TMockKafkaConfig::StopKafka() {
  if (KafkaStarted) {
    MainThread->RequestShutdown();
    MainThread->Join();
    KafkaStarted = false;
  }
}
