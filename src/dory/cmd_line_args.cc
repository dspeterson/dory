/* <dory/cmd_line_args.cc>

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

   Implements <dory/cmd_line_args.h>.
 */

#include <dory/cmd_line_args.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <base/basename.h>
#include <base/no_default_case.h>
#include <dory/build_id.h>
#include <dory/util/arg_parse_error.h>
#include <dory/util/misc_util.h>
#include <log/log.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;
using namespace Log;

static void ProcessModeArg(const std::string &mode_string,
    const char *opt_name, TOpt<mode_t> &result) {
  if (!mode_string.empty()) {
    std::string s(mode_string);
    boost::algorithm::trim(s);

    if (s.empty()) {
      std::string blurb("Invalid value for --");
      blurb += opt_name;
      throw TArgParseError(std::move(blurb));
    }

    char *pos = nullptr;
    long n = std::strtol(s.c_str(), &pos, 0);
    auto mode = static_cast<mode_t>(n);

    if ((*pos != '\0') || (n < 0) || (n == LONG_MAX) ||
        (static_cast<long>(mode) != n)) {
      std::string blurb("Invalid value for --");
      blurb += opt_name;
      throw TArgParseError(std::move(blurb));
    }

    result.MakeKnown(mode);
  }
}

static const char *LogLevelToString(TPri level) {
  switch (level) {
    case TPri::ERR: {
      return "LOG_ERR";
    }
    case TPri::WARNING: {
      return "LOG_WARNING";
    }
    case TPri::NOTICE: {
      return "LOG_NOTICE";
    }
    case TPri::INFO: {
      return "LOG_INFO";
    }
    case TPri::DEBUG: {
      return "LOG_DEBUG";
    }
    NO_DEFAULT_CASE;
  }
}

static TPri StringToLogLevel(const char * level_string) {
  if (!std::strcmp(level_string, "LOG_ERR")) {
    return TPri::ERR;
  }

  if (!std::strcmp(level_string, "LOG_WARNING")) {
    return TPri::WARNING;
  }

  if (!std::strcmp(level_string, "LOG_NOTICE")) {
    return TPri::NOTICE;
  }

  if (!std::strcmp(level_string, "LOG_INFO")) {
    return TPri::INFO;
  }

  if (!std::strcmp(level_string, "LOG_DEBUG")) {
    return TPri::DEBUG;
  }

  throw std::range_error("Bad log level string");
}

static void ParseArgs(int argc, char *argv[], TCmdLineArgs &args,
    bool allow_input_bind_ephemeral) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd("Producer daemon for Apache Kafka", ' ', dory_build_id);
    ValueArg<decltype(args.ConfigPath)> arg_config_path("", "config_path",
        "Pathname of config file.", true, args.ConfigPath, "PATH");
    cmd.add(arg_config_path);
    std::vector<std::string> log_levels({"LOG_ERR", "LOG_WARNING",
        "LOG_NOTICE", "LOG_INFO", "LOG_DEBUG"});
    ValuesConstraint<std::string> log_levels_constraint(log_levels);
    ValueArg<std::string> arg_log_level("", "log_level", "Log level.", false,
        LogLevelToString(args.LogLevel), &log_levels_constraint);
    cmd.add(arg_log_level);
    SwitchArg arg_log_echo("", "log_echo", "Echo syslog messages to standard "
        "error.", cmd, args.LogEcho);
    ValueArg<decltype(args.ReceiveSocketName)> arg_receive_socket_name("",
        "receive_socket_name", "Pathname of UNIX domain datagram socket for "
        "receiving messages from clients", false, args.ReceiveSocketName,
        "PATH");
    cmd.add(arg_receive_socket_name);
    ValueArg<decltype(args.ReceiveStreamSocketName)>
        arg_receive_stream_socket_name("", "receive_stream_socket_name",
        "Pathname of UNIX domain stream socket for receiving messages from "
        "clients", false, args.ReceiveStreamSocketName, "PATH");
    cmd.add(arg_receive_stream_socket_name);
    ValueArg<std::remove_reference<decltype(*args.InputPort)>::type>
        arg_input_port("", "input_port", "Port for receiving TCP connections "
            "from local clients that wish to send messages.", false, 0,
            "PORT");
    cmd.add(arg_input_port);
    ValueArg<std::string> arg_receive_socket_mode("", "receive_socket_mode",
        "File permission bits for UNIX domain datagram socket for receiving "
        "messages from clients.  If unspecified, the umask determines the "
        "permission bits.  To specify an octal value, you must use a 0 "
        "prefix.  For instance, specify 0777 rather than 777 for unrestricted "
        "access.", false, "", "MODE");
    cmd.add(arg_receive_socket_mode);
    ValueArg<std::string> arg_receive_stream_socket_mode("",
        "receive_stream_socket_mode", "File permission bits for UNIX domain "
        "stream socket for receiving messages from clients.  If unspecified, "
        "the umask determines the permission bits.  To specify an octal "
        "value, you must use a 0 prefix.  For instance, specify 0777 rather "
        "than 777 for unrestricted access.", false, "", "MODE");
    cmd.add(arg_receive_stream_socket_mode);
    ValueArg<std::remove_reference<decltype(*args.MetadataApiVersion)>::type>
        arg_metadata_api_version("", "metadata_api_version",
        "Version of Kafka metadata API to use.", false, 0, "VERSION");
    cmd.add(arg_metadata_api_version);
    ValueArg<std::remove_reference<decltype(*args.ProduceApiVersion)>::type>
        arg_produce_api_version("", "produce_api_version",
        "Version of Kafka produce API to use.", false, 0, "VERSION");
    cmd.add(arg_produce_api_version);
    ValueArg<decltype(args.StatusPort)> arg_status_port("", "status_port",
        "HTTP Status monitoring port.", false, args.StatusPort, "PORT");
    cmd.add(arg_status_port);
    SwitchArg arg_status_loopback_only("", "status_loopback_only",
        "Make web interface available only on loopback interface.", cmd,
        args.StatusLoopbackOnly);
    ValueArg<decltype(args.MsgBufferMax)> arg_msg_buffer_max("",
        "msg_buffer_max", "Maximum amount of memory in Kb to use for "
        "buffering messages.", true, args.MsgBufferMax, "MAX_KB");
    cmd.add(arg_msg_buffer_max);
    ValueArg<decltype(args.MaxInputMsgSize)> arg_max_input_msg_size("",
        "max_input_msg_size", "Maximum input message size in bytes expected "
        "from clients sending UNIX domain datagrams.  This limit does NOT "
        "apply to messages sent by UNIX domain stream socket or local TCP "
        "(see max_stream_input_msg_size).  Input datagrams larger than this "
        "value will be discarded.", false, args.MaxInputMsgSize,
        "MAX_BYTES");
    cmd.add(arg_max_input_msg_size);
    ValueArg<decltype(args.MaxStreamInputMsgSize)>
        arg_max_stream_input_msg_size("", "max_stream_input_msg_size",
        "Maximum input message size in bytes expected from clients using UNIX "
        "domain stream sockets or local TCP.  Input messages larger than this "
        "value will cause Dory to immediately log an error and disconnect, "
        "forcing the client to reconnect if it wishes to continue sending "
        "messages.  The purpose of this is to guard against ridiculously "
        "large messages.  Even if a message doesn't exceed this limit, it may "
        "still be discarded if it is too large to send in a single produce "
        "request.  However, in this case Dory will still leave the connection "
        "open and continue reading messages.", false,
        args.MaxStreamInputMsgSize, "MAX_BYTES");
    cmd.add(arg_max_stream_input_msg_size);
    SwitchArg arg_allow_large_unix_datagrams("", "allow_large_unix_datagrams",
        "Allow large enough values for max_input_msg_size that a client "
        "sending a UNIX domain datagram of the maximum allowed size will need "
        "to increase its SO_SNDBUF socket option above the default value.",
        cmd, args.AllowLargeUnixDatagrams);
    ValueArg<decltype(args.MaxFailedDeliveryAttempts)>
        arg_max_failed_delivery_attempts("", "max_failed_delivery_attempts",
        "Maximum number of failed delivery attempts allowed before a message "
        "is discarded.", false, args.MaxFailedDeliveryAttempts,
        "MAX_ATTEMPTS");
    cmd.add(arg_max_failed_delivery_attempts);
    SwitchArg arg_daemon("", "daemon", "Run as daemon.", cmd, args.Daemon);
    ValueArg<decltype(args.ClientId)> arg_client_id("", "client_id",
        "Client ID string to send in produce requests.", false,
        args.ClientId, "CLIENT_ID");
    cmd.add(arg_client_id);
    ValueArg<decltype(args.RequiredAcks)> arg_required_acks("",
        "required_acks", "Required ACKs value to send in produce requests.",
        false, args.RequiredAcks, "REQUIRED_ACKS");
    cmd.add(arg_required_acks);
    ValueArg<decltype(args.ReplicationTimeout)> arg_replication_timeout("",
        "replication_timeout", "Replication timeout value in millisceonds to "
        "send in produce requests.", false, args.ReplicationTimeout,
        "TIMEOUT");
    cmd.add(arg_replication_timeout);
    ValueArg<decltype(args.ShutdownMaxDelay)> arg_shutdown_max_delay("",
        "shutdown_max_delay", "Maximum delay in milliseconds for sending "
        "buffered messages once shutdown signal is received.", false,
        args.ShutdownMaxDelay, "MAX_DELAY_MS");
    cmd.add(arg_shutdown_max_delay);
    ValueArg<decltype(args.DispatcherRestartMaxDelay)>
        arg_dispatcher_restart_max_delay("", "dispatcher_restart_max_delay",
        "Max dispatcher shutdown delay in milliseconds when restarting "
        "dispatcher for metadata update", false,
        args.DispatcherRestartMaxDelay, "MAX_DELAY_MS");
    cmd.add(arg_dispatcher_restart_max_delay);
    ValueArg<decltype(args.MetadataRefreshInterval)>
        arg_metadata_refresh_interval("", "metadata_refresh_interval",
        "Interval in minutes (plus or minus a bit of randomness) between "
        "periodic metadata updates", false, args.MetadataRefreshInterval,
        "INTERVAL_MINUTES");
    cmd.add(arg_metadata_refresh_interval);
    ValueArg<decltype(args.KafkaSocketTimeout)> arg_kafka_socket_timeout("",
        "kafka_socket_timeout", "Socket timeout in seconds to use when "
        "communicating with Kafka broker.", false, args.KafkaSocketTimeout,
        "TIMEOUT_SECONDS");
    cmd.add(arg_kafka_socket_timeout);
    ValueArg<decltype(args.PauseRateLimitInitial)>
        arg_pause_rate_limit_initial("", "pause_rate_limit_initial", "Initial "
        "delay value in milliseconds between consecutive metadata fetches due "
        "to Kafka-related errors.  The actual value has some randomness "
        "added.", false, args.PauseRateLimitInitial, "DELAY_MS");
    cmd.add(arg_pause_rate_limit_initial);
    ValueArg<decltype(args.PauseRateLimitMaxDouble)>
        arg_pause_rate_limit_max_double("", "pause_rate_limit_max_double",
        "Maximum number of times to double pause_rate_limit_initial on "
        "repeated errors.", false, args.PauseRateLimitMaxDouble,
        "MAX_DOUBLE");
    cmd.add(arg_pause_rate_limit_max_double);
    ValueArg<decltype(args.MinPauseDelay)> arg_min_pause_delay("",
        "min_pause_delay", "Minimum delay in milliseconds before fetching new "
        "metadata from Kafka in response to an error.", false,
        args.MinPauseDelay, "MIN_DELAY_MS");
    cmd.add(arg_min_pause_delay);
    ValueArg<decltype(args.DiscardReportInterval)>
        arg_discard_report_interval("", "discard_report_interval",
        "Discard reporting interval in seconds.", false,
        args.DiscardReportInterval, "INTERVAL_SECONDS");
    cmd.add(arg_discard_report_interval);
    SwitchArg arg_no_log_discard("", "no_log_discard", "Do not write syslog "
        "messages when discards occur.  Discard information will still be "
        "available through the web interface.", cmd, args.NoLogDiscard);
    ValueArg<decltype(args.DebugDir)> arg_debug_dir("", "debug_dir",
        "Directory for debug instrumentation files.", false, args.DebugDir,
        "DIR");
    cmd.add(arg_debug_dir);
    ValueArg<decltype(args.MsgDebugTimeLimit)> arg_msg_debug_time_limit("",
        "msg_debug_time_limit", "Message debugging time limit in seconds.",
        false, args.MsgDebugTimeLimit, "LIMIT_SECONDS");
    cmd.add(arg_msg_debug_time_limit);
    ValueArg<decltype(args.MsgDebugByteLimit)> arg_msg_debug_byte_limit("",
        "msg_debug_byte_limit", "Message debugging byte limit.", false,
        args.MsgDebugByteLimit, "MAX_BYTES");
    cmd.add(arg_msg_debug_byte_limit);
    SwitchArg arg_skip_compare_metadata_on_refresh("",
        "skip_compare_metadata_on_refresh", "On metadata refresh, don't "
        "compare new metadata to old metadata.  Always replace the metadata "
        "even if it is unchanged.  This should be disabled for normal "
        "operation, but enabling it may be useful for testing.", cmd,
        args.SkipCompareMetadataOnRefresh);
    ValueArg<decltype(args.DiscardLogPath)> arg_discard_log_path("",
        "discard_log_path", "Absolute pathname of local file where discards "
        "will be logged.  This is intended for debugging.  If unspecified, "
        "logging of discards to a file will be disabled.", false,
        args.DiscardLogPath, "PATH");
    cmd.add(arg_discard_log_path);
    ValueArg<decltype(args.DiscardLogMaxFileSize)>
        arg_discard_log_max_file_size("", "discard_log_max_file_size",
              "Maximum size (in Kb) of discard logfile.  When the next log "
              "entry e would exceed the maximum, the logfile (with name f) is "
              "renamed to f.N where N is the current time in milliseconds "
              "since the epoch.  Then a new file f is opened, and e is "
              "written to f.  See also discard_log_max_archive_size.", false,
              args.DiscardLogMaxFileSize, "MAX_KB");
    cmd.add(arg_discard_log_max_file_size);
    ValueArg<decltype(args.DiscardLogMaxArchiveSize)>
        arg_discard_log_max_archive_size("", "discard_log_max_archive_size",
        "See description of discard_log_max_file_size.  Once a discard "
        "logfile is renamed from f to f.N due to the size restriction imposed "
        "by discard_log_max_file_size, the directory containing f.N is "
        "scanned for all old discard logfiles.  If their combined size "
        "exceeds discard_log_max_archive_size (specified in Kb), then old "
        "logfiles are deleted, starting with the oldest, until their combined "
        "size no longer exceeds the maximum.", false,
        args.DiscardLogMaxArchiveSize, "MAX_KB");
    cmd.add(arg_discard_log_max_archive_size);
    ValueArg<decltype(args.DiscardLogBadMsgPrefixSize)>
        arg_discard_log_bad_msg_prefix_size("",
        "discard_log_bad_msg_prefix_size", "Maximum bad message prefix size "
        "in bytes to write to discard logfile when discarding", false,
        args.DiscardLogBadMsgPrefixSize, "MAX_BYTES");
    cmd.add(arg_discard_log_bad_msg_prefix_size);
    ValueArg<decltype(args.DiscardReportBadMsgPrefixSize)>
        arg_discard_report_bad_msg_prefix_size("",
        "discard_report_bad_msg_prefix_size", "Maximum bad message prefix "
        "size in bytes to write to discard report", false,
        args.DiscardReportBadMsgPrefixSize, "MAX_BYTES");
    cmd.add(arg_discard_report_bad_msg_prefix_size);
    SwitchArg arg_topic_autocreate("", "topic_autocreate", "Enable support "
        "for automatic topic creation.  The Kafka brokers must also be "
        "configured to support this.", cmd, args.TopicAutocreate);
    cmd.parse(argc, &arg_vec[0]);
    args.ConfigPath = arg_config_path.getValue();
    args.LogLevel = StringToLogLevel(arg_log_level.getValue().c_str());
    args.LogEcho = arg_log_echo.getValue();
    args.ReceiveSocketName = arg_receive_socket_name.getValue();
    args.ReceiveStreamSocketName = arg_receive_stream_socket_name.getValue();

    if (arg_input_port.isSet()) {
      in_port_t port = arg_input_port.getValue();

      if ((port < 1) && !allow_input_bind_ephemeral) {
        /* A value of 0 requests an ephemeral port, which is only allowed for
           test code. */
        throw TArgParseError("Invalid input port");
      }

      args.InputPort.MakeKnown(port);
    }

    ProcessModeArg(arg_receive_socket_mode.getValue(), "receive_socket_mode",
        args.ReceiveSocketMode);
    ProcessModeArg(arg_receive_stream_socket_mode.getValue(),
        "receive_stream_socket_mode", args.ReceiveStreamSocketMode);

    if (arg_metadata_api_version.isSet()) {
      args.MetadataApiVersion.MakeKnown(arg_metadata_api_version.getValue());
    }

    if (arg_produce_api_version.isSet()) {
      args.ProduceApiVersion.MakeKnown(arg_produce_api_version.getValue());
    }

    args.StatusPort = arg_status_port.getValue();
    args.StatusLoopbackOnly = arg_status_loopback_only.getValue();
    args.MsgBufferMax = arg_msg_buffer_max.getValue();
    args.MaxInputMsgSize = arg_max_input_msg_size.getValue();
    args.MaxStreamInputMsgSize = arg_max_stream_input_msg_size.getValue();
    args.AllowLargeUnixDatagrams = arg_allow_large_unix_datagrams.getValue();
    args.MaxFailedDeliveryAttempts =
        arg_max_failed_delivery_attempts.getValue();
    args.Daemon = arg_daemon.getValue();
    args.ClientId = arg_client_id.getValue();
    args.ClientIdWasEmpty = args.ClientId.empty();

    if (args.ClientIdWasEmpty) {
      /* Workaround for bug in Kafka 0.9.0.0.  See
         https://issues.apache.org/jira/browse/KAFKA-3088 for details. */
      args.ClientId = "dory";
    }

    args.RequiredAcks = arg_required_acks.getValue();
    args.ReplicationTimeout = arg_replication_timeout.getValue();
    args.ShutdownMaxDelay = arg_shutdown_max_delay.getValue();
    args.DispatcherRestartMaxDelay =
        arg_dispatcher_restart_max_delay.getValue();
    args.MetadataRefreshInterval = arg_metadata_refresh_interval.getValue();
    args.KafkaSocketTimeout = arg_kafka_socket_timeout.getValue();
    args.PauseRateLimitInitial = arg_pause_rate_limit_initial.getValue();
    args.PauseRateLimitMaxDouble =
        arg_pause_rate_limit_max_double.getValue();
    args.MinPauseDelay = arg_min_pause_delay.getValue();
    args.DiscardReportInterval = arg_discard_report_interval.getValue();
    args.NoLogDiscard = arg_no_log_discard.getValue();
    args.DebugDir = arg_debug_dir.getValue();
    args.MsgDebugTimeLimit = arg_msg_debug_time_limit.getValue();
    args.MsgDebugByteLimit = arg_msg_debug_byte_limit.getValue();
    args.SkipCompareMetadataOnRefresh =
        arg_skip_compare_metadata_on_refresh.getValue();
    args.DiscardLogPath = arg_discard_log_path.getValue();
    args.DiscardLogMaxFileSize = arg_discard_log_max_file_size.getValue();
    args.DiscardLogMaxArchiveSize =
        arg_discard_log_max_archive_size.getValue();
    args.DiscardLogBadMsgPrefixSize =
        arg_discard_log_bad_msg_prefix_size.getValue();
    args.DiscardReportBadMsgPrefixSize =
        arg_discard_report_bad_msg_prefix_size.getValue();
    args.TopicAutocreate = arg_topic_autocreate.getValue();

    if (!arg_receive_socket_name.isSet() &&
        !arg_receive_stream_socket_name.isSet() && !arg_input_port.isSet()) {
      throw TArgParseError(
          "At least one of (--receive_socket_name, "
          "--receive_stream_socket_name, --input_port) options must be "
          "specified.");
    }

    if (!arg_receive_socket_name.isSet()) {
      if (arg_receive_socket_mode.isSet()) {
        throw TArgParseError(
            "Option --receive_socket_mode is only allowed when "
            "--receive_socket_name is specified.");
      }

      if (arg_allow_large_unix_datagrams.isSet()) {
        throw TArgParseError(
            "Option --allow_large_unix_datagrams is only allowed when "
            "--receive_socket_name is specified.");
      }
    }

    if (!arg_receive_stream_socket_name.isSet() &&
        arg_receive_stream_socket_mode.isSet()) {
      throw TArgParseError(
          "Option --receive_stream_socket_mode is only allowed when "
          "--receive_stream_socket_name is specified.");
    }
  } catch (const ArgException &x) {
    throw TArgParseError(x.error(), x.argId());
  }

  if (args.StatusPort < 1) {
    throw TArgParseError("Invalid value specified for option --status_port.");
  }
}

TCmdLineArgs::TCmdLineArgs(int argc, char *argv[],
    bool allow_input_bind_ephemeral) {
  ParseArgs(argc, argv, *this, allow_input_bind_ephemeral);
}

static std::string BuildModeString(const TOpt<mode_t> &opt_mode) {
  char socket_mode[32];

  if (opt_mode.IsKnown()) {
    std::snprintf(socket_mode, sizeof(socket_mode), "0%o", *opt_mode);
  } else {
    std::strncpy(socket_mode, "<unspecified>", sizeof(socket_mode));
  }

  socket_mode[sizeof(socket_mode) - 1] = '\0';
  return socket_mode;
}

void Dory::LogCmdLineArgs(const TCmdLineArgs &args) {
  if (args.ClientIdWasEmpty) {
    LOG(TPri::WARNING) << "Using \"dory\" for client ID since none was "
        << "specified with --client_id option.  This is a workaround for a "
        << "bug in Kafka 0.9.0.0 that causes broker to crash on receipt of "
        << "produce request with empty client ID.  See "
        << "https://issues.apache.org/jira/browse/KAFKA-3088 for details.";
  }

  LOG(TPri::NOTICE) << "Version: [" << dory_build_id << "]";
  LOG(TPri::NOTICE) << "Config file: [" << args.ConfigPath << "]";

  if (args.ReceiveSocketName.empty()) {
    LOG(TPri::NOTICE) << "UNIX domain datagram input socket disabled";
  } else {
    LOG(TPri::NOTICE) << "UNIX domain datagram input socket ["
                      << args.ReceiveSocketName << "]";
  }

  if (args.ReceiveStreamSocketName.empty()) {
    LOG(TPri::NOTICE) << "UNIX domain stream input socket disabled";
  } else {
    LOG(TPri::NOTICE) << "UNIX domain stream input socket ["
                      << args.ReceiveStreamSocketName << "]";
  }

  if (args.InputPort.IsKnown()) {
    LOG(TPri::NOTICE) << "Listening on input port " << *args.InputPort;
  } else {
    LOG(TPri::NOTICE) << "Input port disabled";
  }

  if (!args.ReceiveSocketName.empty()) {
    LOG(TPri::NOTICE) << "UNIX domain datagram input socket mode "
        << BuildModeString(args.ReceiveSocketMode);
  }

  if (!args.ReceiveStreamSocketName.empty()) {
    LOG(TPri::NOTICE) << "UNIX domain stream input socket mode "
        << BuildModeString(args.ReceiveStreamSocketMode);
  }

  if (args.MetadataApiVersion.IsKnown()) {
    LOG(TPri::NOTICE) << "Metadata API version is specified as "
        << *args.MetadataApiVersion;
  } else {
    LOG(TPri::NOTICE) << "Metadata API version is unspecified";
  }

  if (args.ProduceApiVersion.IsKnown()) {
    LOG(TPri::NOTICE) << "Produce API version is specified as "
        << *args.ProduceApiVersion;
  } else {
    LOG(TPri::NOTICE) << "Produce API version is unspecified";
  }

  LOG(TPri::NOTICE) << "Listening on status port " << args.StatusPort;
  LOG(TPri::NOTICE) << "Web interface loopback only: "
                    << std::boolalpha << args.StatusLoopbackOnly;
  LOG(TPri::NOTICE) << "Buffered message limit " << args.MsgBufferMax
                    << "kbytes";
  LOG(TPri::NOTICE) << "Max datagram input message size "
                    << args.MaxInputMsgSize << " bytes";
  LOG(TPri::NOTICE) << "Max stream input message size "
                    << args.MaxStreamInputMsgSize << " bytes";

  if (!args.ReceiveSocketName.empty()) {
    LOG(TPri::NOTICE) << "Allow large UNIX datagrams: "
                      << std::boolalpha << args.AllowLargeUnixDatagrams;
  }

  LOG(TPri::NOTICE) << "Max failed delivery attempts "
                    << args.MaxFailedDeliveryAttempts;
  LOG(TPri::NOTICE) << "Running as daemon: " << std::boolalpha
                    << args.Daemon;
  LOG(TPri::NOTICE) << "Client ID [" << args.ClientId << "]";
  LOG(TPri::NOTICE) << "Required ACKs " << args.RequiredAcks;
  LOG(TPri::NOTICE) << "Replication timeout " << args.ReplicationTimeout
                    << " ms";
  LOG(TPri::NOTICE) << "Shutdown send grace period " << args.ShutdownMaxDelay
                    << " ms";
  LOG(TPri::NOTICE) << "Kafka dispatch restart grace period "
                    << args.DispatcherRestartMaxDelay << " ms";
  LOG(TPri::NOTICE) << "Metadata refresh interval "
                    << args.MetadataRefreshInterval << " min";
  LOG(TPri::NOTICE) << "Kafka socket timeout " << args.KafkaSocketTimeout
                    << " sec";
  LOG(TPri::NOTICE) << "Pause rate limit initial "
                    << args.PauseRateLimitInitial << " ms";
  LOG(TPri::NOTICE) << "Pause rate limit max double "
                    << args.PauseRateLimitMaxDouble;
  LOG(TPri::NOTICE) << "Minimum pause delay " << args.MinPauseDelay << " ms";
  LOG(TPri::NOTICE) << "Discard reporting interval "
                    << args.DiscardReportInterval << " sec";
  LOG(TPri::NOTICE) << "Debug directory [" << args.DebugDir << "]";
  LOG(TPri::NOTICE) << "Message debug time limit " << args.MsgDebugTimeLimit
                    << " sec";
  LOG(TPri::NOTICE) << "Message debug byte limit " << args.MsgDebugByteLimit;
  LOG(TPri::NOTICE) << "Skip comparing metadata on refresh: " << std::boolalpha
                    << args.SkipCompareMetadataOnRefresh;

  if (args.DiscardLogPath.empty()) {
    LOG(TPri::NOTICE) << "Discard logfile creation is disabled";
  } else {
    LOG(TPri::NOTICE) << "Discard logfile: [" << args.DiscardLogPath << "]";
    LOG(TPri::NOTICE) << "Discard log max file size: "
                      << args.DiscardLogMaxFileSize << " kb";
    LOG(TPri::NOTICE) << "Discard log max archive size: "
                      << args.DiscardLogMaxArchiveSize << " kb";
    LOG(TPri::NOTICE) << "Discard log bad msg prefix size: "
                      << args.DiscardLogBadMsgPrefixSize << " bytes";
  }

  LOG(TPri::NOTICE) << "Discard report bad msg prefix size: "
                    << args.DiscardReportBadMsgPrefixSize << " bytes";
  LOG(TPri::NOTICE) << "Automatic topic creation enabled: " << std::boolalpha
                    << args.TopicAutocreate;
}
