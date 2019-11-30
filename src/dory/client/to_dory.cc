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
#include <dory/util/invalid_arg_error.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Client;
using namespace Dory::Util;

struct TCmdLineArgs {
  /* Throws TInvalidArgError on error parsing args. */
  TCmdLineArgs(int argc, const char *const argv[]);

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
};  // TCmdLineArgs

static void ParseArgs(int argc, const char *const argv[],
    TCmdLineArgs &args) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd("Utility for sending messages to Dory.", ' ', dory_build_id);
    ValueArg<decltype(args.SocketPath)> arg_socket_path("", "socket-path",
        "Pathname of UNIX domain datagram socket for sending messages to "
        "Dory.",
        false, args.SocketPath, "PATH");
    cmd.add(arg_socket_path);
    ValueArg<decltype(args.StreamSocketPath)> arg_stream_socket_path("",
        "stream-socket-path",
        "Pathname of UNIX domain stream socket for sending messages to Dory.",
        false, args.StreamSocketPath, "PATH");
    cmd.add(arg_stream_socket_path);
    ValueArg<std::remove_reference<decltype(*args.Port)>::type>
        arg_port("", "port", "Local TCP port for sending messages to Dory.",
        false, 0, "PORT");
    cmd.add(arg_port);
    ValueArg<decltype(args.Topic)> arg_topic("", "topic", "Kafka topic.",
        true, args.Topic, "TOPIC");
    cmd.add(arg_topic);
    ValueArg<decltype(args.PartitionKey)> arg_partition_key("",
        "partition-key", "Partition key.", false, args.PartitionKey,
        "PARTITION_KEY");
    cmd.add(arg_partition_key);
    ValueArg<decltype(args.Key)> arg_key("", "key", "Message key.", false,
        args.Key, "KEY");
    cmd.add(arg_key);
    ValueArg<decltype(args.Value)> arg_value("", "value",
        "Message value (option is invalid if --stdin is specified).", false,
        args.Value, "VALUE");
    cmd.add(arg_value);
    SwitchArg arg_stdin("", "stdin", "Read message value from standard input.",
        cmd, args.Stdin);
    ValueArg<decltype(args.Count)> arg_count("", "count",
        "Number of messages to send.", false, args.Count, "COUNT");
    cmd.add(arg_count);
    ValueArg<decltype(args.Interval)> arg_interval("", "interval",
        "Message interval in microseconds.  A value of 0 means \"send "
        "messages as fast as possible\".",
        false, args.Interval, "INTERVAL");
    cmd.add(arg_interval);
    SwitchArg arg_seq("", "seq",
        "Prepend incrementing count to message value.", cmd, args.Seq);
    ValueArg<decltype(args.Pad)> arg_pad("", "pad",
        "Pad incrementing count with leading 0s to fill this many spaces.",
        false, args.Pad, "PAD");
    cmd.add(arg_pad);
    SwitchArg arg_bad("", "bad", "Send a malformed message.", cmd, args.Bad);
    ValueArg<decltype(args.Print)> arg_print("", "print",
        "If nonzero, print message number every nth message.", false,
        args.Print, "PRINT");
    cmd.add(arg_print);
    cmd.parse(argc, &arg_vec[0]);
    args.SocketPath = arg_socket_path.getValue();
    args.StreamSocketPath = arg_stream_socket_path.getValue();
    size_t input_type_count = 0;

    if (arg_port.isSet()) {
      in_port_t port = arg_port.getValue();

      if (port < 1) {
        throw TInvalidArgError("Invalid port");
      }

      args.Port.MakeKnown(port);
      ++input_type_count;
    }

    args.Topic = arg_topic.getValue();
    args.PartitionKey = arg_partition_key.getValue();
    args.UsePartitionKey = arg_partition_key.isSet();
    args.Key = arg_key.getValue();
    args.Value = arg_value.getValue();
    args.ValueSpecified = arg_value.isSet();
    args.Stdin = arg_stdin.getValue();
    args.Count = arg_count.getValue();
    args.Interval = arg_interval.getValue();
    args.Seq = arg_seq.getValue();
    args.Pad = arg_pad.getValue();
    args.Bad = arg_bad.getValue();
    args.Print = arg_print.getValue();

    if (arg_socket_path.isSet()) {
      ++input_type_count;
    }

    if (arg_stream_socket_path.isSet()) {
      ++input_type_count;
    }

    if (input_type_count != 1) {
      throw TInvalidArgError(
          "Exactly one of (--socket-path, --stream-socket-path, --port) "
          "options must be specified.");
    }
  } catch (const ArgException &x) {
    throw TInvalidArgError(x.error(), x.argId());
  }

  if (args.Stdin && args.ValueSpecified) {
    throw TInvalidArgError(
        "You cannot specify --value <VALUE> and --stdin simultaneously.");
  }
}

TCmdLineArgs::TCmdLineArgs(int argc, const char *const argv[]) {
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

bool CreateDg(std::vector<uint8_t> &buf, const TCmdLineArgs &cfg,
    size_t msg_count) {
  std::string value;

  if (cfg.Seq) {
    std::string seq = std::to_string(msg_count);

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

static std::unique_ptr<TClientSenderBase> CreateSender(
    const TCmdLineArgs &cfg) {
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

static int ToDoryMain(int argc, const char *const argv[]) {
  std::unique_ptr<TCmdLineArgs> args;

  try {
    args.reset(new TCmdLineArgs(argc, argv));
  } catch (const TInvalidArgError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::unique_ptr<TClientSenderBase> sender(CreateSender(*args));
  sender->PrepareToSend();
  std::vector<uint8_t> dg_buf;
  const clockid_t CLOCK_TYPE = CLOCK_MONOTONIC_RAW;

  /* The constructor initializes this to the epoch.  On the first iteration the
     deadline will be in the past, so the sleep time will be 0. */
  TTime deadline;

  for (size_t i = 1; i <= args->Count; ++i) {
    if (!CreateDg(dg_buf, *args, i)) {
      return EXIT_FAILURE;
    }

    SleepMicroseconds(deadline.RemainingMicroseconds(CLOCK_TYPE));
    deadline.Now(CLOCK_TYPE);
    sender->Send(&dg_buf[0], dg_buf.size());
    deadline.AddMicroseconds(args->Interval);

    if (args->Print && ((i % args->Print) == 0)) {
        std::cout << i << " messages written" << std::endl;
    }
  }

  return EXIT_SUCCESS;
}

int main(int argc, const char *const argv[]) {
  try {
    return ToDoryMain(argc, argv);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << std::endl;
  } catch (...) {
    std::cerr << "error: unknown exception" << std::endl;
  }

  return EXIT_FAILURE;
}
