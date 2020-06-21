/* <dory/debug/debug_logger.h>

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

   Class for logging messages for debugging.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <dory/debug/debug_setup.h>
#include <dory/msg.h>

namespace Dory {

  namespace Debug {

    class TDebugLogger final {
      NO_COPY_SEMANTICS(TDebugLogger);

      public:
      TDebugLogger(const TDebugSetup &debug_setup, TDebugSetup::TLogId log_id)
          : DebugSetup(debug_setup),
            LogId(log_id),
            Settings(debug_setup.GetSettings()),
            LogFd(Settings->GetLogFileDescriptor(log_id)),
            CachedSettingsVersion(Settings->GetVersion()),
            CachedDebugTopics(Settings->GetDebugTopics()),
            LoggingEnabled(Settings->LoggingIsEnabled()),
            LoggingEnabledAt(Now()) {
      }

      void LogMsg(const TMsg &msg);

      void LogMsg(const TMsg::TPtr &msg_ptr) {
        LogMsg(*msg_ptr);
      }

      void LogMsgList(const std::list<TMsg::TPtr> &msg_list);

      private:
      using TSettings = TDebugSetup::TSettings;

      /* Return a monotonically increasing timestamp indicating the number of
         seconds since some fixed point in the past (not necessarily the
         epoch). */
      static unsigned long Now() noexcept;

      unsigned long SecondsSinceEnabled() const noexcept {
        unsigned long t = Now();
        return (t >= LoggingEnabledAt) ? (t - LoggingEnabledAt): 0;
      }

      void DisableLogging() noexcept;

      void EnableLogging() noexcept;

      const TDebugSetup &DebugSetup;

      const TDebugSetup::TLogId LogId;

      std::shared_ptr<TSettings> Settings;

      int LogFd;

      size_t CachedSettingsVersion;

      const std::unordered_set<std::string> *CachedDebugTopics;

      bool LoggingEnabled;

      unsigned long LoggingEnabledAt;

      size_t MsgCount = 0;

      std::vector<uint8_t> RawData;

      std::string Encoded;

      std::string LogEntry;
    };  // TDebugLogger

  }  // Debug

}  // Dory
