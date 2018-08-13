/* <dory/config.cc>

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

   Implements <dory/config.h>.
 */

#include <dory/config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <libgen.h>
#include <syslog.h>

#include <base/basename.h>
#include <base/no_default_case.h>
#include <dory/build_id.h>
#include <dory/util/arg_parse_error.h>
#include <dory/util/misc_util.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Util;

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

static void ParseArgs(int argc, char *argv[], TConfig &config,
    bool allow_input_bind_ephemeral) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd("Producer daemon for Apache Kafka", ' ', dory_build_id);
    ValueArg<decltype(config.ConfigPath)> arg_config_path("", "config_path",
        "Pathname of config file.", true, config.ConfigPath, "PATH");
    cmd.add(arg_config_path);
    std::vector<std::string> log_levels({"LOG_ERR", "LOG_WARNING",
        "LOG_NOTICE", "LOG_INFO", "LOG_DEBUG"});
    ValuesConstraint<std::string> log_levels_constraint(log_levels);
    ValueArg<std::string> arg_log_level("", "log_level", "Log level.", false,
        LogLevelToString(config.LogLevel), &log_levels_constraint);
    cmd.add(arg_log_level);
    SwitchArg arg_log_echo("", "log_echo", "Echo syslog messages to standard "
        "error.", cmd, config.LogEcho);
    ValueArg<decltype(config.ReceiveSocketName)> arg_receive_socket_name("",
        "receive_socket_name", "Pathname of UNIX domain datagram socket for "
        "receiving messages from clients", false, config.ReceiveSocketName,
        "PATH");
    cmd.add(arg_receive_socket_name);
    ValueArg<decltype(config.ReceiveStreamSocketName)>
        arg_receive_stream_socket_name("", "receive_stream_socket_name",
        "Pathname of UNIX domain stream socket for receiving messages from "
        "clients", false, config.ReceiveStreamSocketName, "PATH");
    cmd.add(arg_receive_stream_socket_name);
    ValueArg<std::remove_reference<decltype(*config.InputPort)>::type>
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
    ValueArg<std::remove_reference<decltype(*config.MetadataApiVersion)>::type>
        arg_metadata_api_version("", "metadata_api_version",
        "Version of Kafka metadata API to use.", false, 0, "VERSION");
    cmd.add(arg_metadata_api_version);
    ValueArg<std::remove_reference<decltype(*config.ProduceApiVersion)>::type>
        arg_produce_api_version("", "produce_api_version",
        "Version of Kafka produce API to use.", false, 0, "VERSION");
    cmd.add(arg_produce_api_version);
    ValueArg<decltype(config.StatusPort)> arg_status_port("", "status_port",
        "HTTP Status monitoring port.", false, config.StatusPort, "PORT");
    cmd.add(arg_status_port);
    SwitchArg arg_status_loopback_only("", "status_loopback_only",
        "Make web interface available only on loopback interface.", cmd,
        config.StatusLoopbackOnly);
    ValueArg<decltype(config.MsgBufferMax)> arg_msg_buffer_max("",
        "msg_buffer_max", "Maximum amount of memory in Kb to use for "
        "buffering messages.", true, config.MsgBufferMax, "MAX_KB");
    cmd.add(arg_msg_buffer_max);
    ValueArg<decltype(config.MaxInputMsgSize)> arg_max_input_msg_size("",
        "max_input_msg_size", "Maximum input message size in bytes expected "
        "from clients sending UNIX domain datagrams.  This limit does NOT "
        "apply to messages sent by UNIX domain stream socket or local TCP "
        "(see max_stream_input_msg_size).  Input datagrams larger than this "
        "value will be discarded.", false, config.MaxInputMsgSize,
        "MAX_BYTES");
    cmd.add(arg_max_input_msg_size);
    ValueArg<decltype(config.MaxStreamInputMsgSize)>
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
        config.MaxStreamInputMsgSize, "MAX_BYTES");
    cmd.add(arg_max_stream_input_msg_size);
    SwitchArg arg_allow_large_unix_datagrams("", "allow_large_unix_datagrams",
        "Allow large enough values for max_input_msg_size that a client "
        "sending a UNIX domain datagram of the maximum allowed size will need "
        "to increase its SO_SNDBUF socket option above the default value.",
        cmd, config.AllowLargeUnixDatagrams);
    ValueArg<decltype(config.MaxFailedDeliveryAttempts)>
        arg_max_failed_delivery_attempts("", "max_failed_delivery_attempts",
        "Maximum number of failed delivery attempts allowed before a message "
        "is discarded.", false, config.MaxFailedDeliveryAttempts,
        "MAX_ATTEMPTS");
    cmd.add(arg_max_failed_delivery_attempts);
    SwitchArg arg_daemon("", "daemon", "Run as daemon.", cmd, config.Daemon);
    ValueArg<decltype(config.ClientId)> arg_client_id("", "client_id",
        "Client ID string to send in produce requests.", false,
        config.ClientId, "CLIENT_ID");
    cmd.add(arg_client_id);
    ValueArg<decltype(config.RequiredAcks)> arg_required_acks("",
        "required_acks", "Required ACKs value to send in produce requests.",
        false, config.RequiredAcks, "REQUIRED_ACKS");
    cmd.add(arg_required_acks);
    ValueArg<decltype(config.ReplicationTimeout)> arg_replication_timeout("",
        "replication_timeout", "Replication timeout value in millisceonds to "
        "send in produce requests.", false, config.ReplicationTimeout,
        "TIMEOUT");
    cmd.add(arg_replication_timeout);
    ValueArg<decltype(config.ShutdownMaxDelay)> arg_shutdown_max_delay("",
        "shutdown_max_delay", "Maximum delay in milliseconds for sending "
        "buffered messages once shutdown signal is received.", false,
        config.ShutdownMaxDelay, "MAX_DELAY_MS");
    cmd.add(arg_shutdown_max_delay);
    ValueArg<decltype(config.DispatcherRestartMaxDelay)>
        arg_dispatcher_restart_max_delay("", "dispatcher_restart_max_delay",
        "Max dispatcher shutdown delay in milliseconds when restarting "
        "dispatcher for metadata update", false,
        config.DispatcherRestartMaxDelay, "MAX_DELAY_MS");
    cmd.add(arg_dispatcher_restart_max_delay);
    ValueArg<decltype(config.MetadataRefreshInterval)>
        arg_metadata_refresh_interval("", "metadata_refresh_interval",
        "Interval in minutes (plus or minus a bit of randomness) between "
        "periodic metadata updates", false, config.MetadataRefreshInterval,
        "INTERVAL_MINUTES");
    cmd.add(arg_metadata_refresh_interval);
    ValueArg<decltype(config.KafkaSocketTimeout)> arg_kafka_socket_timeout("",
        "kafka_socket_timeout", "Socket timeout in seconds to use when "
        "communicating with Kafka broker.", false, config.KafkaSocketTimeout,
        "TIMEOUT_SECONDS");
    cmd.add(arg_kafka_socket_timeout);
    ValueArg<decltype(config.PauseRateLimitInitial)>
        arg_pause_rate_limit_initial("", "pause_rate_limit_initial", "Initial "
        "delay value in milliseconds between consecutive metadata fetches due "
        "to Kafka-related errors.  The actual value has some randomness "
        "added.", false, config.PauseRateLimitInitial, "DELAY_MS");
    cmd.add(arg_pause_rate_limit_initial);
    ValueArg<decltype(config.PauseRateLimitMaxDouble)>
        arg_pause_rate_limit_max_double("", "pause_rate_limit_max_double",
        "Maximum number of times to double pause_rate_limit_initial on "
        "repeated errors.", false, config.PauseRateLimitMaxDouble,
        "MAX_DOUBLE");
    cmd.add(arg_pause_rate_limit_max_double);
    ValueArg<decltype(config.MinPauseDelay)> arg_min_pause_delay("",
        "min_pause_delay", "Minimum delay in milliseconds before fetching new "
        "metadata from Kafka in response to an error.", false,
        config.MinPauseDelay, "MIN_DELAY_MS");
    cmd.add(arg_min_pause_delay);
    ValueArg<decltype(config.DiscardReportInterval)>
        arg_discard_report_interval("", "discard_report_interval",
        "Discard reporting interval in seconds.", false,
        config.DiscardReportInterval, "INTERVAL_SECONDS");
    cmd.add(arg_discard_report_interval);
    SwitchArg arg_no_log_discard("", "no_log_discard", "Do not write syslog "
        "messages when discards occur.  Discard information will still be "
        "available through the web interface.", cmd, config.NoLogDiscard);
    ValueArg<decltype(config.DebugDir)> arg_debug_dir("", "debug_dir",
        "Directory for debug instrumentation files.", false, config.DebugDir,
        "DIR");
    cmd.add(arg_debug_dir);
    ValueArg<decltype(config.MsgDebugTimeLimit)> arg_msg_debug_time_limit("",
        "msg_debug_time_limit", "Message debugging time limit in seconds.",
        false, config.MsgDebugTimeLimit, "LIMIT_SECONDS");
    cmd.add(arg_msg_debug_time_limit);
    ValueArg<decltype(config.MsgDebugByteLimit)> arg_msg_debug_byte_limit("",
        "msg_debug_byte_limit", "Message debugging byte limit.", false,
        config.MsgDebugByteLimit, "MAX_BYTES");
    cmd.add(arg_msg_debug_byte_limit);
    SwitchArg arg_skip_compare_metadata_on_refresh("",
        "skip_compare_metadata_on_refresh", "On metadata refresh, don't "
        "compare new metadata to old metadata.  Always replace the metadata "
        "even if it is unchanged.  This should be disabled for normal "
        "operation, but enabling it may be useful for testing.", cmd,
        config.SkipCompareMetadataOnRefresh);
    ValueArg<decltype(config.DiscardLogPath)> arg_discard_log_path("",
        "discard_log_path", "Absolute pathname of local file where discards "
        "will be logged.  This is intended for debugging.  If unspecified, "
        "logging of discards to a file will be disabled.", false,
        config.DiscardLogPath, "PATH");
    cmd.add(arg_discard_log_path);
    ValueArg<decltype(config.DiscardLogMaxFileSize)>
        arg_discard_log_max_file_size("", "discard_log_max_file_size",
              "Maximum size (in Kb) of discard logfile.  When the next log "
              "entry e would exceed the maximum, the logfile (with name f) is "
              "renamed to f.N wnere N is the current time in milliseconds "
              "since the epoch.  Then a new file f is opened, and e is "
              "written to f.  See also discard_log_max_archive_size.", false,
              config.DiscardLogMaxFileSize, "MAX_KB");
    cmd.add(arg_discard_log_max_file_size);
    ValueArg<decltype(config.DiscardLogMaxArchiveSize)>
        arg_discard_log_max_archive_size("", "discard_log_max_archive_size",
        "See description of discard_log_max_file_size.  Once a discard "
        "logfile is renamed from f to f.N due to the size restriction imposed "
        "by discard_log_max_file_size, the directory containing f.N is "
        "scanned for all old discard logfiles.  If their combined size "
        "exceeds discard_log_max_archive_size (specified in Kb), then old "
        "logfiles are deleted, starting with the oldest, until their combined "
        "size no longer exceeds the maximum.", false,
        config.DiscardLogMaxArchiveSize, "MAX_KB");
    cmd.add(arg_discard_log_max_archive_size);
    ValueArg<decltype(config.DiscardLogBadMsgPrefixSize)>
        arg_discard_log_bad_msg_prefix_size("",
        "discard_log_bad_msg_prefix_size", "Maximum bad message prefix size "
        "in bytes to write to discard logfile when discarding", false,
        config.DiscardLogBadMsgPrefixSize, "MAX_BYTES");
    cmd.add(arg_discard_log_bad_msg_prefix_size);
    ValueArg<decltype(config.DiscardReportBadMsgPrefixSize)>
        arg_discard_report_bad_msg_prefix_size("",
        "discard_report_bad_msg_prefix_size", "Maximum bad message prefix "
        "size in bytes to write to discard report", false,
        config.DiscardReportBadMsgPrefixSize, "MAX_BYTES");
    cmd.add(arg_discard_report_bad_msg_prefix_size);
    SwitchArg arg_topic_autocreate("", "topic_autocreate", "Enable support "
        "for automatic topic creation.  The Kafka brokers must also be "
        "configured to support this.", cmd, config.TopicAutocreate);
    cmd.parse(argc, &arg_vec[0]);
    config.ConfigPath = arg_config_path.getValue();
    config.LogLevel = StringToLogLevel(arg_log_level.getValue());
    config.LogEcho = arg_log_echo.getValue();
    config.ReceiveSocketName = arg_receive_socket_name.getValue();
    config.ReceiveStreamSocketName = arg_receive_stream_socket_name.getValue();

    if (arg_input_port.isSet()) {
      in_port_t port = arg_input_port.getValue();

      if ((port < 1) && !allow_input_bind_ephemeral) {
        /* A value of 0 requests an ephemeral port, which is only allowed for
           test code. */
        throw TArgParseError("Invalid input port");
      }

      config.InputPort.MakeKnown(port);
    }

    ProcessModeArg(arg_receive_socket_mode.getValue(), "receive_socket_mode",
        config.ReceiveSocketMode);
    ProcessModeArg(arg_receive_stream_socket_mode.getValue(),
        "receive_stream_socket_mode", config.ReceiveStreamSocketMode);

    if (arg_metadata_api_version.isSet()) {
      config.MetadataApiVersion.MakeKnown(arg_metadata_api_version.getValue());
    }

    if (arg_produce_api_version.isSet()) {
      config.ProduceApiVersion.MakeKnown(arg_produce_api_version.getValue());
    }

    config.StatusPort = arg_status_port.getValue();
    config.StatusLoopbackOnly = arg_status_loopback_only.getValue();
    config.MsgBufferMax = arg_msg_buffer_max.getValue();
    config.MaxInputMsgSize = arg_max_input_msg_size.getValue();
    config.MaxStreamInputMsgSize = arg_max_stream_input_msg_size.getValue();
    config.AllowLargeUnixDatagrams = arg_allow_large_unix_datagrams.getValue();
    config.MaxFailedDeliveryAttempts =
        arg_max_failed_delivery_attempts.getValue();
    config.Daemon = arg_daemon.getValue();
    config.ClientId = arg_client_id.getValue();
    config.ClientIdWasEmpty = config.ClientId.empty();

    if (config.ClientIdWasEmpty) {
      /* Workaround for bug in Kafka 0.9.0.0.  See
         https://issues.apache.org/jira/browse/KAFKA-3088 for details. */
      config.ClientId = "dory";
    }

    config.RequiredAcks = arg_required_acks.getValue();
    config.ReplicationTimeout = arg_replication_timeout.getValue();
    config.ShutdownMaxDelay = arg_shutdown_max_delay.getValue();
    config.DispatcherRestartMaxDelay =
        arg_dispatcher_restart_max_delay.getValue();
    config.MetadataRefreshInterval = arg_metadata_refresh_interval.getValue();
    config.KafkaSocketTimeout = arg_kafka_socket_timeout.getValue();
    config.PauseRateLimitInitial = arg_pause_rate_limit_initial.getValue();
    config.PauseRateLimitMaxDouble =
        arg_pause_rate_limit_max_double.getValue();
    config.MinPauseDelay = arg_min_pause_delay.getValue();
    config.DiscardReportInterval = arg_discard_report_interval.getValue();
    config.NoLogDiscard = arg_no_log_discard.getValue();
    config.DebugDir = arg_debug_dir.getValue();
    config.MsgDebugTimeLimit = arg_msg_debug_time_limit.getValue();
    config.MsgDebugByteLimit = arg_msg_debug_byte_limit.getValue();
    config.SkipCompareMetadataOnRefresh =
        arg_skip_compare_metadata_on_refresh.getValue();
    config.DiscardLogPath = arg_discard_log_path.getValue();
    config.DiscardLogMaxFileSize = arg_discard_log_max_file_size.getValue();
    config.DiscardLogMaxArchiveSize =
        arg_discard_log_max_archive_size.getValue();
    config.DiscardLogBadMsgPrefixSize =
        arg_discard_log_bad_msg_prefix_size.getValue();
    config.DiscardReportBadMsgPrefixSize =
        arg_discard_report_bad_msg_prefix_size.getValue();
    config.TopicAutocreate = arg_topic_autocreate.getValue();

    if (!arg_receive_socket_name.isSet() &&
        !arg_receive_stream_socket_name.isSet() && !arg_input_port.isSet()) {
      throw TArgParseError("At least one of (--receive_socket_name, "
          "--receive_stream_socket_name, --input_port) options must be "
          "specified.");
    }

    if (!arg_receive_socket_name.isSet()) {
      if (arg_receive_socket_mode.isSet()) {
        throw TArgParseError("Option --receive_socket_mode is only allowed "
            "when --receive_socket_name is specified.");
      }

      if (arg_allow_large_unix_datagrams.isSet()) {
        throw TArgParseError("Option --allow_large_unix_datagrams is only "
            "allowed when --receive_socket_name is specified.");
      }
    }

    if (!arg_receive_stream_socket_name.isSet() &&
        arg_receive_stream_socket_mode.isSet()) {
      throw TArgParseError("Option --receive_stream_socket_mode is only "
          "allowed when --receive_stream_socket_name is specified.");
    }
  } catch (const ArgException &x) {
    throw TArgParseError(x.error(), x.argId());
  }

  if (config.StatusPort < 1) {
    throw TArgParseError("Invalid value specified for option --status_port.");
  }
}

TConfig::TConfig(int argc, char *argv[], bool allow_input_bind_ephemeral)
    : LogLevel(LOG_NOTICE),
      LogEcho(false),
      StatusPort(9090),
      StatusLoopbackOnly(false),
      MsgBufferMax(256 * 1024),
      MaxInputMsgSize(64 * 1024),
      MaxStreamInputMsgSize(2 * 1024 * 1024),
      AllowLargeUnixDatagrams(false),
      MaxFailedDeliveryAttempts(5),
      Daemon(false),
      ClientIdWasEmpty(true),
      RequiredAcks(-1),
      ReplicationTimeout(10000),
      ShutdownMaxDelay(30000),
      DispatcherRestartMaxDelay(5000),
      MetadataRefreshInterval(15),
      KafkaSocketTimeout(60),
      PauseRateLimitInitial(5000),
      PauseRateLimitMaxDouble(4),
      MinPauseDelay(5000),
      DiscardReportInterval(600),
      NoLogDiscard(false),
      DebugDir("/home/dory/debug"),
      MsgDebugTimeLimit(3600),
      MsgDebugByteLimit(2UL * 1024UL * 1024UL * 1024UL),
      SkipCompareMetadataOnRefresh(false),
      DiscardLogMaxFileSize(1024),
      DiscardLogMaxArchiveSize(8 * 1024),
      DiscardLogBadMsgPrefixSize(256),
      DiscardReportBadMsgPrefixSize(256),
      TopicAutocreate(false) {
  ParseArgs(argc, argv, *this, allow_input_bind_ephemeral);
}

static std::string BuildModeString(const TOpt<mode_t> &opt_mode) {
  char socket_mode[32];

  if (opt_mode.IsKnown()) {
    std::snprintf(socket_mode, sizeof(socket_mode), "0%o", *opt_mode);
  } else {
    strncpy(socket_mode, "<unspecified>", sizeof(socket_mode));
  }

  socket_mode[sizeof(socket_mode) - 1] = '\0';
  return socket_mode;
}

void Dory::LogConfig(const TConfig &config) {
  if (config.ClientIdWasEmpty) {
    syslog(LOG_WARNING, "Using \"dory\" for client ID since none was "
        "specified with --client_id option.  This is a workaround for a bug "
        "in Kafka 0.9.0.0 that causes broker to crash on receipt of produce "
        "request with empty client ID.  See "
        "https://issues.apache.org/jira/browse/KAFKA-3088 for details.");
  }

  syslog(LOG_NOTICE, "Version: [%s]", dory_build_id);
  syslog(LOG_NOTICE, "Config file: [%s]", config.ConfigPath.c_str());

  if (config.ReceiveSocketName.empty()) {
    syslog(LOG_NOTICE, "UNIX domain datagram input socket disabled");
  } else {
    syslog(LOG_NOTICE, "UNIX domain datagram input socket [%s]",
        config.ReceiveSocketName.c_str());
  }

  if (config.ReceiveStreamSocketName.empty()) {
    syslog(LOG_NOTICE, "UNIX domain stream input socket disabled");
  } else {
    syslog(LOG_NOTICE, "UNIX domain stream input socket [%s]",
        config.ReceiveStreamSocketName.c_str());
  }

  if (config.InputPort.IsKnown()) {
    syslog(LOG_NOTICE, "Listening on input port %u",
        static_cast<unsigned>(*config.InputPort));
  } else {
    syslog(LOG_NOTICE, "Input port disabled");
  }

  if (!config.ReceiveSocketName.empty()) {
    syslog(LOG_NOTICE, "UNIX domain datagram input socket mode %s",
        BuildModeString(config.ReceiveSocketMode).c_str());
  }

  if (!config.ReceiveStreamSocketName.empty()) {
    syslog(LOG_NOTICE, "UNIX domain stream input socket mode %s",
        BuildModeString(config.ReceiveStreamSocketMode).c_str());
  }

  if (config.MetadataApiVersion.IsKnown()) {
    syslog(LOG_NOTICE, "Metadata API version is specified as %lu",
        static_cast<unsigned long>(*config.MetadataApiVersion));
  } else {
    syslog(LOG_NOTICE, "Metadata API version is unspecified");
  }

  if (config.ProduceApiVersion.IsKnown()) {
    syslog(LOG_NOTICE, "Produce API version is specified as %lu",
        static_cast<unsigned long>(*config.ProduceApiVersion));
  } else {
    syslog(LOG_NOTICE, "Produce API version is unspecified");
  }

  syslog(LOG_NOTICE, "Listening on status port %u",
         static_cast<unsigned>(config.StatusPort));
  syslog(LOG_NOTICE, "Web interface loopback only: %s",
         config.StatusLoopbackOnly ? "true" : "false");
  syslog(LOG_NOTICE, "Buffered message limit %lu kbytes",
         static_cast<unsigned long>(config.MsgBufferMax));
  syslog(LOG_NOTICE, "Max datagram input message size %lu bytes",
         static_cast<unsigned long>(config.MaxInputMsgSize));
  syslog(LOG_NOTICE, "Max stream input message size %lu bytes",
         static_cast<unsigned long>(config.MaxStreamInputMsgSize));

  if (!config.ReceiveSocketName.empty()) {
    syslog(LOG_NOTICE, "Allow large UNIX datagrams: %s",
           config.AllowLargeUnixDatagrams ? "true" : "false");
  }

  syslog(LOG_NOTICE, "Max failed delivery attempts %lu",
         static_cast<unsigned long>(config.MaxFailedDeliveryAttempts));
  syslog(LOG_NOTICE, config.Daemon ?
         "Running as daemon" : "Not running as daemon");
  syslog(LOG_NOTICE, "Client ID [%s]", config.ClientId.c_str());
  syslog(LOG_NOTICE, "Required ACKs %d",
         static_cast<int>(config.RequiredAcks));
  syslog(LOG_NOTICE, "Replication timeout %d milliseconds",
         static_cast<int>(config.ReplicationTimeout));
  syslog(LOG_NOTICE, "Shutdown send grace period %lu milliseconds",
         static_cast<unsigned long>(config.ShutdownMaxDelay));
  syslog(LOG_NOTICE, "Kafka dispatch restart grace period %lu milliseconds",
         static_cast<unsigned long>(config.DispatcherRestartMaxDelay));
  syslog(LOG_NOTICE, "Metadata refresh interval %lu minutes",
         static_cast<unsigned long>(config.MetadataRefreshInterval));
  syslog(LOG_NOTICE, "Kafka socket timeout %lu seconds",
         static_cast<unsigned long>(config.KafkaSocketTimeout));
  syslog(LOG_NOTICE, "Pause rate limit initial %lu milliseconds",
         static_cast<unsigned long>(config.PauseRateLimitInitial));
  syslog(LOG_NOTICE, "Pause rate limit max double %lu",
         static_cast<unsigned long>(config.PauseRateLimitMaxDouble));
  syslog(LOG_NOTICE, "Minimum pause delay %lu milliseconds",
         static_cast<unsigned long>(config.MinPauseDelay));
  syslog(LOG_NOTICE, "Discard reporting interval %lu seconds",
         static_cast<unsigned long>(config.DiscardReportInterval));
  syslog(LOG_NOTICE, "Debug directory [%s]", config.DebugDir.c_str());
  syslog(LOG_NOTICE, "Message debug time limit %lu seconds",
         static_cast<unsigned long>(config.MsgDebugTimeLimit));
  syslog(LOG_NOTICE, "Message debug byte limit %lu",
         static_cast<unsigned long>(config.MsgDebugByteLimit));
  syslog(LOG_NOTICE, "Skip comparing metadata on refresh: %s",
         config.SkipCompareMetadataOnRefresh ? "true" : "false");

  if (config.DiscardLogPath.empty()) {
    syslog(LOG_NOTICE, "Discard logfile creation is disabled");
  } else {
    syslog(LOG_NOTICE, "Discard logfile: [%s]", config.DiscardLogPath.c_str());
    syslog(LOG_NOTICE, "Discard log max file size: %lu kbytes",
           static_cast<unsigned long>(config.DiscardLogMaxFileSize));
    syslog(LOG_NOTICE, "Discard log max archive size: %lu kbytes",
           static_cast<unsigned long>(config.DiscardLogMaxArchiveSize));
    syslog(LOG_NOTICE, "Discard log bad msg prefix size: %lu bytes",
           static_cast<unsigned long>(config.DiscardLogBadMsgPrefixSize));
  }

  syslog(LOG_NOTICE, "Discard report bad msg prefix size: %lu bytes",
         static_cast<unsigned long>(config.DiscardReportBadMsgPrefixSize));
  syslog(LOG_NOTICE, config.TopicAutocreate ?
         "Automatic topic creation enabled" :
         "Automatic topic creation disabled");
}
