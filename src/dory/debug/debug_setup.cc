/* <dory/debug/debug_setup.cc>

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

   Implements <dory/debug/debug_setup.h>.
 */

#include <dory/debug/debug_setup.h>

#include <cerrno>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/wr/file_util.h>
#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Debug;
using namespace Log;

static int OpenDebugFile(const char *path, bool truncate_file) {
  int flags = O_CREAT | O_APPEND | O_WRONLY;

  if (truncate_file) {
    flags |= O_TRUNC;
  }

  int fd = Wr::open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (fd < 0) {
    /* Fail gracefully. */
    LOG_ERRNO(TPri::ERR, errno) << "Failed to open debug logfile " << path
        << ": ";
  }

  return fd;
}

TDebugSetup::TSettings::TSettings(size_t version,
    std::unordered_set<std::string> *debug_topics,
    const char *msg_receive_log_path, const char *msg_send_log_path,
    const char *msg_got_ack_log_path, size_t byte_limit, bool truncate_files)
    : Version(version),
      LoggingEnabled(EnableIsSpecified(debug_topics)),
      BytesRemaining(byte_limit) {
  if (debug_topics) {
    DebugTopics.MakeKnown(std::move(*debug_topics));
  }

  if (LoggingEnabled) {
    GetLogFd(TLogId::MSG_RECEIVE) = OpenDebugFile(msg_receive_log_path,
                                                  truncate_files);
    GetLogFd(TLogId::MSG_SEND) = OpenDebugFile(msg_send_log_path,
                                               truncate_files);
    GetLogFd(TLogId::MSG_GOT_ACK) = OpenDebugFile(msg_got_ack_log_path,
                                                  truncate_files);
  }
}

bool TDebugSetup::AddDebugTopic(const char *topic) {
  std::lock_guard<std::mutex> lock(Mutex);
  assert(Settings);
  const std::unordered_set<std::string> *tptr = Settings->GetDebugTopics();

  if (tptr == nullptr) {
    /* "All topics" is already specified. */
    return false;
  }

  std::string to_add(topic);

  if (tptr->find(to_add) != tptr->end()) {
    /* 'topic' is already specified. */
    return false;
  }

  if (!Settings->LoggingIsEnabled()) {
    CreateDebugDir();
  }

  std::unordered_set<std::string> new_topics(*tptr);
  new_topics.insert(std::move(to_add));
  ReplaceSettings(&new_topics);
  return true;
}

bool TDebugSetup::DelDebugTopic(const char *topic) {
  std::lock_guard<std::mutex> lock(Mutex);
  assert(Settings);
  const std::unordered_set<std::string> *tptr = Settings->GetDebugTopics();

  if (tptr == nullptr) {
    /* "All topics" is specified.  Implementing "all topics except { X, Y, Z }"
       semantics wouldn't be hard, but that feature isn't currently needed and
       I don't feel like implementing it.  Therefore ignore the request. */
    return false;
  }

  std::string to_del(topic);

  if (tptr->find(to_del) == tptr->end()) {
    /* 'topic' is already absent. */
    return false;
  }

  std::unordered_set<std::string> new_topics(*tptr);
  new_topics.erase(to_del);
  ReplaceSettings(&new_topics);
  return true;
}

void TDebugSetup::SetDebugTopics(
    std::unordered_set<std::string> *debug_topics) {
  std::lock_guard<std::mutex> lock(Mutex);
  assert(Settings);

  if (!Settings->LoggingIsEnabled() &&
      TSettings::EnableIsSpecified(debug_topics)) {
    CreateDebugDir();
  }

  ReplaceSettings(debug_topics);
}

void TDebugSetup::TruncateDebugFiles() {
  if (Wr::truncate(GetLogPath(TLogId::MSG_RECEIVE).c_str(), 0) < 0) {
    LOG(TPri::ERR) << "Failed to truncate MSG_RECEIVE debug logfile";
  }

  if (Wr::truncate(GetLogPath(TLogId::MSG_SEND).c_str(), 0) < 0) {
    LOG(TPri::ERR) << "Failed to truncate MSG_SEND debug logfile";
  }

  if (Wr::truncate(GetLogPath(TLogId::MSG_GOT_ACK).c_str(), 0) < 0) {
    LOG(TPri::ERR) << "Failed to truncate MSG_GOT_ACK debug logfile";
  }
}

void TDebugSetup::CreateDebugDir() {
  std::string cmd("/bin/mkdir -p ");
  cmd += DebugDir;

  if (std::system(cmd.c_str()) < 0) {
    LOG(TPri::ERR) << "Failed to create debug directory [" << DebugDir.c_str()
        << "]";
    /* Keep running, with debug logfiles disabled. */
  }
}

static void SettingsFtruncate(const TDebugSetup::TSettings &settings) {
  int fd = settings.GetLogFileDescriptor(TDebugSetup::TLogId::MSG_RECEIVE);

  if (fd >= 0 && Wr::ftruncate(fd, 0)) {
    /* Fail gracefully. */
    LOG_ERRNO(TPri::ERR, errno)
        << "Failed to truncate msg receive debug logfile: ";
  }

  fd = settings.GetLogFileDescriptor(TDebugSetup::TLogId::MSG_SEND);

  if (fd >= 0 && Wr::ftruncate(fd, 0)) {
    /* Fail gracefully. */
    LOG_ERRNO(TPri::ERR, errno)
        << "Failed to truncate msg send debug logfile: ";
  }

  fd = settings.GetLogFileDescriptor(TDebugSetup::TLogId::MSG_GOT_ACK);

  if (fd >= 0 && Wr::ftruncate(fd, 0)) {
    /* Fail gracefully. */
    LOG_ERRNO(TPri::ERR, errno)
        << "Failed to truncate msg got ACK debug logfile: ";
  }
}

void TDebugSetup::DeleteOldDebugFiles(
    const std::shared_ptr<TSettings> &old_settings) {
  /* Unlink the old files.  When we create new files to replace them, any
     threads still using the old file descriptors (and debug settings) will
     write to the unlinked files until they see that the debug settings have
     changed.  New debug data gets written to the new files, and is not mixed
     with any data associated with the previous debug settings. */
  const std::string &msg_receive_path = GetLogPath(TLogId::MSG_RECEIVE);
  const std::string &msg_send_path = GetLogPath(TLogId::MSG_SEND);
  const std::string &msg_got_ack_path = GetLogPath(TLogId::MSG_GOT_ACK);

  if (Wr::unlink(msg_receive_path.c_str())) {
    LOG_ERRNO(TPri::ERR, errno)
        << "Failed to unlink debug file for received messages ["
        << msg_receive_path << "]: ";
  }

  if (Wr::unlink(msg_send_path.c_str())) {
    LOG_ERRNO(TPri::ERR, errno)
        << "Failed to unlink debug file for sent messages [" << msg_send_path
        << "]: ";
  }

  if (Wr::unlink(msg_got_ack_path.c_str())) {
    LOG_ERRNO(TPri::ERR, errno)
        << "Failed to unlink debug file for acknowledged messages ["
        << msg_got_ack_path << "]: ";
  }

  if (old_settings) {
    /* Now ftruncate the files we just unlinked through their still open file
       descriptors.  In case the old (soon to be discarded) file data is large,
       we want to get rid of it right away so dory isn't occupying a ton of
       disk space with data no longer visible in the filesystem namespace.
     */
    SettingsFtruncate(*old_settings);
  }
}
