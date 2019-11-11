/* <dory/conf/logging_conf.h>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Class representing logging configuration obtained from Dory's config file.
 */

#pragma once

#include <cassert>
#include <string>
#include <utility>

#include <sys/types.h>

#include <base/opt.h>
#include <dory/conf/conf_error.h>
#include <log/file_log_writer.h>

namespace Dory {

  namespace Conf {

    class TLoggingConf {
      public:
      class TBuilder;

      TLoggingConf() = default;

      TLoggingConf(const TLoggingConf &) = default;

      TLoggingConf(TLoggingConf &&) = default;

      TLoggingConf(bool enable_stdout_stderr, bool enable_syslog,
          const std::string &file_path, mode_t file_mode)
          : EnableStdoutStderr(enable_stdout_stderr),
            EnableSyslog(enable_syslog),
            FilePath(file_path),
            FileMode(file_mode) {
      }

      TLoggingConf &operator=(const TLoggingConf &) = default;

      TLoggingConf &operator=(TLoggingConf &&) = default;

      bool GetEnableStdoutStderr() const noexcept {
        assert(this);
        return EnableStdoutStderr;
      }

      bool GetEnableSyslog() const noexcept {
        assert(this);
        return EnableSyslog;
      }

      const std::string &GetFilePath() const noexcept {
        assert(this);
        return FilePath;
      }

      mode_t GetFileMode() const noexcept {
        assert(this);
        return FileMode;
      }

      private:
      bool EnableStdoutStderr = false;

      bool EnableSyslog = true;

      /* Must be empty string or absolute pathname.  Empty string indicates
         that file logging is disabled. */
      std::string FilePath;

      mode_t FileMode = Log::TFileLogWriter::DEFAULT_FILE_MODE;
    };  // TLoggingConf

    class TLoggingConf::TBuilder {
      public:
      /* Exception base class. */
      class TErrorBase : public TConfError {
        protected:
        explicit TErrorBase(std::string &&msg)
            : TConfError(std::move(msg)) {
        }
      };  // TErrorBase

      class TRelativeFilePath final : public TErrorBase {
        public:
        TRelativeFilePath()
            : TErrorBase("Logfile path must be absolute") {
        }
      };  // TRelativeFilePath

      class TInvalidFileMode final : public TErrorBase {
        public:
        TInvalidFileMode()
            : TErrorBase("Invalid logfile mode") {
        }
      };  // TInvalidFileMode

      TBuilder() = default;

      TBuilder(const TBuilder &) = default;

      TBuilder(TBuilder &&) = default;

      TBuilder &operator=(const TBuilder &) = default;

      TBuilder &operator=(TBuilder &&) = default;

      void Reset() {
        assert(this);
        *this = TBuilder();
      }

      void SetStdoutStderrConf(bool enable_stdout_stderr);

      void SetSyslogConf(bool enable_syslog);

      void SetFileConf(const std::string &path, mode_t mode);

      TLoggingConf Build();

      private:
      TLoggingConf BuildResult;
    };  // TLoggingConf::TBuilder

  }  // Conf

}  // Dory
