/* <dory/client/to_dory.cc>

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

   Simple client program that sends messages to Dory daemon.
 */

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <netinet/in.h>

#include <base/basename.h>
#include <base/error_util.h>
#include <base/fd.h>
#include <base/field_access.h>
#include <base/no_copy_semantics.h>
#include <base/no_default_case.h>
#include <base/opt.h>
#include <base/time.h>
#include <base/time_util.h>
#include <dory/build_id.h>
#include <dory/client/dory_client.h>
#include <dory/client/dory_client_socket.h>
#include <dory/client/path_too_long.h>
#include <dory/client/status_codes.h>
#include <dory/client/tcp_sender.h>
#include <dory/client/unix_dg_sender.h>
#include <dory/client/unix_stream_sender.h>
#include <dory/util/arg_parse_error.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Client;
using namespace Dory::Util;

struct TConfig {
  /* Throws TArgParseError on error parsing args. */
  TConfig(int argc, char *argv[]);

  /* For UNIX domain datagram socket input to Dory. */
  std::string SocketPath;

  /* For UNIX domain stream socket input to Dory. */
  std::string StreamSocketPath;

  /* For local TCP input to Dory. */
  TOpt<in_port_t> Port;

  std::string Topic;

  uint32_t PartitionKey = 0;

  bool UsePartitionKey = false;

  std::string Key;

  std::string Value;

  bool ValueSpecified = false;

  bool Stdin = false;

  size_t Count = 1;

  size_t Interval = 0;

  bool Seq = false;

  size_t Pad = 0;

  bool Bad = false;

  size_t Print = 0;
};  // TConfig

static void ParseArgs(int argc, char *argv[], TConfig &config) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd("Utility for sending messages to Dory.", ' ', dory_build_id);
    ValueArg<decltype(config.SocketPath)> arg_socket_path("", "socket_path",
        "Pathname of UNIX domain datagram socket for sending messages to Dory.",
        false, config.SocketPath, "PATH");
    cmd.add(arg_socket_path);
    ValueArg<decltype(config.StreamSocketPath)> arg_stream_socket_path("",
        "stream_socket_path",
        "Pathname of UNIX domain stream socket for sending messages to Dory.",
        false, config.StreamSocketPath, "PATH");
    cmd.add(arg_stream_socket_path);
    ValueArg<std::remove_reference<decltype(*config.Port)>::type>
        arg_port("", "port", "Local TCP port for sending messages to Dory.",
        false, 0, "PORT");
    cmd.add(arg_port);
    ValueArg<decltype(config.Topic)> arg_topic("", "topic", "Kafka topic.",
        true, config.Topic, "TOPIC");
    cmd.add(arg_topic);
    ValueArg<decltype(config.PartitionKey)> arg_partition_key("",
        "partition_key", "Partition key.", false, config.PartitionKey,
        "PARTITION_KEY");
    cmd.add(arg_partition_key);
    ValueArg<decltype(config.Key)> arg_key("", "key", "Message key.", false,
        config.Key, "KEY");
    cmd.add(arg_key);
    ValueArg<decltype(config.Value)> arg_value("", "value",
        "Message value (option is invalid if --stdin is specified).", false,
        config.Value, "VALUE");
    cmd.add(arg_value);
    SwitchArg arg_stdin("", "stdin", "Read message value from standard input.",
        cmd, config.Stdin);
    ValueArg<decltype(config.Count)> arg_count("", "count",
        "Number of messages to send.", false, config.Count, "COUNT");
    cmd.add(arg_count);
    ValueArg<decltype(config.Interval)> arg_interval("", "interval",
        "Message interval in microseconds.  A value of 0 means \"send messages as fast as possible\".",
        false, config.Interval, "INTERVAL");
    cmd.add(arg_interval);
    SwitchArg arg_seq("", "seq",
        "Prepend incrementing count to message value.", cmd, config.Seq);
    ValueArg<decltype(config.Pad)> arg_pad("", "pad",
        "Pad incrementing count with leading 0s to fill this many spaces.",
        false, config.Pad, "PAD");
    cmd.add(arg_pad);
    SwitchArg arg_bad("", "bad", "Send a malformed message.", cmd, config.Bad);
    ValueArg<decltype(config.Print)> arg_print("", "print",
        "If nonzero, print message number every nth message.", false,
        config.Print, "PRINT");
    cmd.add(arg_print);
    cmd.parse(argc, &arg_vec[0]);
    config.SocketPath = arg_socket_path.getValue();
    config.StreamSocketPath = arg_stream_socket_path.getValue();
    size_t input_type_count = 0;

    if (arg_port.isSet()) {
      in_port_t port = arg_port.getValue();

      if (port < 1) {
        throw TArgParseError("Invalid port");
      }

      config.Port.MakeKnown(port);
      ++input_type_count;
    }

    config.Topic = arg_topic.getValue();
    config.PartitionKey = arg_partition_key.getValue();
    config.UsePartitionKey = arg_partition_key.isSet();
    config.Key = arg_key.getValue();
    config.Value = arg_value.getValue();
    config.ValueSpecified = arg_value.isSet();
    config.Stdin = arg_stdin.getValue();
    config.Count = arg_count.getValue();
    config.Interval = arg_interval.getValue();
    config.Seq = arg_seq.getValue();
    config.Pad = arg_pad.getValue();
    config.Bad = arg_bad.getValue();
    config.Print = arg_print.getValue();

    if (arg_socket_path.isSet()) {
      ++input_type_count;
    }

    if (arg_stream_socket_path.isSet()) {
      ++input_type_count;
    }

    if (input_type_count != 1) {
      throw TArgParseError(
          "Exactly one of (--socket_path, --stream_socket_path, --port) options must be specified.");
    }
  } catch (const ArgException &x) {
    throw TArgParseError(x.error(), x.argId());
  }

  if (config.Stdin && config.ValueSpecified) {
    throw TArgParseError(
        "You cannot specify --value <VALUE> and --stdin simultaneously.");
  }
}

TConfig::TConfig(int argc, char *argv[]) {
  ParseArgs(argc, argv, *this);
}

std::string GetValueFromStdin() {
  std::string result;
  char buf[4096];

  for (; ; ) {
    ssize_t nbytes = read(0, buf, sizeof(buf));

    if (nbytes < 0) {
      if (errno == EINTR) {
        continue;
      }

      IfLt0(nbytes);  // this will throw
    }

    if (nbytes == 0) {
      break;
    }

    result.insert(result.size(), buf, static_cast<size_t>(nbytes));
  }

  return result;
}

bool CreateDg(std::vector<uint8_t> &buf, const TConfig &cfg,
    size_t msg_count) {
  std::string value;

  if (cfg.Seq) {
    std::string seq = boost::lexical_cast<std::string>(msg_count);

    if (seq.size() < cfg.Pad) {
      value.assign(cfg.Pad - seq.size(), '0');
    }

    value += seq;
    value.push_back(' ');
  }

  value += cfg.Stdin ? GetValueFromStdin() : cfg.Value;
  uint64_t ts = GetEpochMilliseconds();

  if (cfg.UsePartitionKey) {
    size_t msg_size = 0;

    switch (dory_find_partition_key_msg_size(cfg.Topic.size(),
        cfg.Key.size(), value.size(), &msg_size)) {
      case DORY_OK:
        break;
      case DORY_TOPIC_TOO_LARGE:
        std::cerr << "Topic is too large." << std::endl;
        return false;
      case DORY_MSG_TOO_LARGE:
        std::cerr << "Message is too large." << std::endl;
        return false;
      NO_DEFAULT_CASE;
    }

    buf.resize(msg_size);
    int ret = dory_write_partition_key_msg(&buf[0], buf.size(),
        cfg.PartitionKey, cfg.Topic.c_str(), ts, cfg.Key.data(),
        cfg.Key.size(), value.data(), value.size());
    assert(ret == DORY_OK);
  } else {
    size_t msg_size = 0;

    switch (dory_find_any_partition_msg_size(cfg.Topic.size(),
        cfg.Key.size(), value.size(), &msg_size)) {
      case DORY_OK:
        break;
      case DORY_TOPIC_TOO_LARGE:
        std::cerr << "Topic is too large." << std::endl;
        return false;
      case DORY_MSG_TOO_LARGE:
        std::cerr << "Message is too large." << std::endl;
        return false;
      NO_DEFAULT_CASE;
    }

    buf.resize(msg_size);
    int ret = dory_write_any_partition_msg(&buf[0], buf.size(),
        cfg.Topic.c_str(), ts, cfg.Key.data(), cfg.Key.size(), value.data(),
        value.size());
    assert(ret == DORY_OK);
  }

  if (cfg.Bad) {
    /* To make the message malformed, we change the size field to an incorrect
       value. */
    assert(buf.size() >= sizeof(int32_t));
    WriteInt32ToHeader(&buf[0], static_cast<int32_t>(buf.size() - 1));
  }

  return true;
}

static std::unique_ptr<TClientSenderBase> CreateSender(const TConfig &cfg) {
  if (!cfg.SocketPath.empty()) {
    return std::unique_ptr<TClientSenderBase>(
        new TUnixDgSender(cfg.SocketPath.c_str()));
  }

  if (!cfg.StreamSocketPath.empty()) {
    return std::unique_ptr<TClientSenderBase>(
        new TUnixStreamSender(cfg.StreamSocketPath.c_str()));
  }

  assert(cfg.Port.IsKnown());
  return std::unique_ptr<TClientSenderBase>(new TTcpSender(*cfg.Port));
}

static int ToDoryMain(int argc, char *argv[]) {
  std::unique_ptr<TConfig> cfg;

  try {
    cfg.reset(new TConfig(argc, argv));
  } catch (const TArgParseError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::unique_ptr<TClientSenderBase> sender(CreateSender(*cfg));
  sender->PrepareToSend();
  std::vector<uint8_t> dg_buf;
  const clockid_t CLOCK_TYPE = CLOCK_MONOTONIC_RAW;

  /* The constructor initializes this to the epoch.  On the first iteration the
     deadline will be in the past, so the sleep time will be 0. */
  TTime deadline;

  for (size_t i = 1; i <= cfg->Count; ++i) {
    if (!CreateDg(dg_buf, *cfg, i)) {
      return EXIT_FAILURE;
    }

    SleepMicroseconds(deadline.RemainingMicroseconds(CLOCK_TYPE));
    deadline.Now(CLOCK_TYPE);
    sender->Send(&dg_buf[0], dg_buf.size());
    deadline.AddMicroseconds(cfg->Interval);

    if (cfg->Print && ((i % cfg->Print) == 0)) {
        std::cout << i << " messages written" << std::endl;
    }
  }

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  try {
    return ToDoryMain(argc, argv);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << std::endl;
  } catch (...) {
    std::cerr << "error: unknown exception" << std::endl;
  }

  return EXIT_FAILURE;
}
