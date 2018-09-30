/* <dory/config.h>

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

   Configuration options for dory daemon.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <netinet/in.h>
#include <sys/stat.h>
#include <syslog.h>

#include <base/opt.h>

namespace Dory {

  struct TConfig {
    /* Throws TArgParseError on error parsing args. */
    TConfig(int argc, char *argv[], bool allow_input_bind_ephemeral);

    std::string ConfigPath;

    int LogLevel = LOG_NOTICE;

    bool LogEcho = false;

    std::string ReceiveSocketName;

    std::string ReceiveStreamSocketName;

    /* Unknown means "TCP input is disabled".  Known and nonzero means "Use
       this TCP input port".  Known but 0 means "bind() to ephemeral port for
       TCP input".  The last option is used by test code. */
    Base::TOpt<in_port_t> InputPort;

    Base::TOpt<mode_t> ReceiveSocketMode;

    Base::TOpt<mode_t> ReceiveStreamSocketMode;

    Base::TOpt<size_t> MetadataApiVersion;

    Base::TOpt<size_t> ProduceApiVersion;

    in_port_t StatusPort = 9090;

    bool StatusLoopbackOnly = false;

    size_t MsgBufferMax = 256 * 1024;

    size_t MaxInputMsgSize = 64 * 1024;

    size_t MaxStreamInputMsgSize = 2 * 1024 * 1024;

    bool AllowLargeUnixDatagrams = false;

    size_t MaxFailedDeliveryAttempts = 5;

    bool Daemon = false;

    std::string ClientId;

    bool ClientIdWasEmpty = true;

    int16_t RequiredAcks = -1;

    size_t ReplicationTimeout = 10000;

    size_t ShutdownMaxDelay = 30000;

    size_t DispatcherRestartMaxDelay = 5000;

    size_t MetadataRefreshInterval = 15;

    size_t KafkaSocketTimeout = 60;

    size_t PauseRateLimitInitial = 5000;

    size_t PauseRateLimitMaxDouble = 4;

    size_t MinPauseDelay = 5000;

    size_t DiscardReportInterval = 600;

    bool NoLogDiscard = false;

    std::string DebugDir{"/home/dory/debug"};

    size_t MsgDebugTimeLimit = 3600;

    size_t MsgDebugByteLimit = 2UL * 1024UL * 1024UL * 1024UL;

    bool SkipCompareMetadataOnRefresh = false;

    std::string DiscardLogPath;

    size_t DiscardLogMaxFileSize = 1024;

    size_t DiscardLogMaxArchiveSize = 8 * 1024;

    size_t DiscardLogBadMsgPrefixSize = 256;

    size_t DiscardReportBadMsgPrefixSize = 256;

    bool TopicAutocreate = false;
  };  // TConfig

  void LogConfig(const TConfig &config);

}  // Dory
