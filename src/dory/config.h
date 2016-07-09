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

#include <base/opt.h>

namespace Dory {

  struct TConfig {
    /* Throws TArgParseError on error parsing args. */
    TConfig(int argc, char *argv[], bool allow_input_bind_ephemeral);

    std::string ConfigPath;

    int LogLevel;

    bool LogEcho;

    std::string ReceiveSocketName;

    std::string ReceiveStreamSocketName;

    /* Unknown means "TCP input is disabled".  Known and nonzero means "Use
       this TCP input port".  Known but 0 means "bind() to ephemeral port for
       TCP input".  The last option is used by test code. */
    Base::TOpt<in_port_t> InputPort;

    Base::TOpt<mode_t> ReceiveSocketMode;

    Base::TOpt<mode_t> ReceiveStreamSocketMode;

    size_t ProtocolVersion;

    in_port_t StatusPort;

    bool StatusLoopbackOnly;

    size_t MsgBufferMax;

    size_t MaxInputMsgSize;

    size_t MaxStreamInputMsgSize;

    bool AllowLargeUnixDatagrams;

    size_t MaxFailedDeliveryAttempts;

    bool Daemon;

    std::string ClientId;

    bool ClientIdWasEmpty;

    int16_t RequiredAcks;

    size_t ReplicationTimeout;

    size_t ShutdownMaxDelay;

    size_t DispatcherRestartMaxDelay;

    size_t MetadataRefreshInterval;

    size_t KafkaSocketTimeout;

    size_t PauseRateLimitInitial;

    size_t PauseRateLimitMaxDouble;

    size_t MinPauseDelay;

    size_t DiscardReportInterval;

    bool NoLogDiscard;

    std::string DebugDir;

    size_t MsgDebugTimeLimit;

    size_t MsgDebugByteLimit;

    bool SkipCompareMetadataOnRefresh;

    std::string DiscardLogPath;

    size_t DiscardLogMaxFileSize;

    size_t DiscardLogMaxArchiveSize;

    size_t DiscardLogBadMsgPrefixSize;

    size_t DiscardReportBadMsgPrefixSize;

    bool TopicAutocreate;
  };  // TConfig

  void LogConfig(const TConfig &config);

}  // Dory
