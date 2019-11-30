/* <dory/mock_kafka_server/inject_error.cc>

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

   Quick and dirty program for sending an error injection command to a mock
   Kafka server.
 */

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <netinet/in.h>

#include <base/basename.h>
#include <dory/build_id.h>
#include <dory/mock_kafka_server/cmd.h>
#include <dory/mock_kafka_server/error_injector.h>
#include <dory/util/invalid_arg_error.h>
#include <tclap/CmdLine.h>

using namespace Base;
using namespace Dory;
using namespace Dory::MockKafkaServer;
using namespace Dory::Util;

using TErrorInjectCmd = Dory::MockKafkaServer::TCmd;

struct TCmdLineArgs {
  /* Throws TInvalidArgError on error parsing args. */
  TCmdLineArgs(int argc, const char *const argv[]);

  std::string Host;

  in_port_t Port = 9080;

  int16_t AckError = 0;

  bool AckDisconnect = false;

  int16_t SingleTopicMdError = 0;

  int16_t AllTopicsMdError = 0;

  bool SingleTopicMdDisconnect = false;

  bool AllTopicsMdDisconnect = false;

  std::string ClientAddr;

  std::string MsgBody;

  std::string Topic;

  std::string CmdFile;
};  // TCmdLineArgs

static void ParseArgs(int argc, const char *const argv[], TCmdLineArgs &args) {
  using namespace TCLAP;
  const std::string prog_name = Basename(argv[0]);
  std::vector<const char *> arg_vec(&argv[0], &argv[0] + argc);
  arg_vec[0] = prog_name.c_str();

  try {
    CmdLine cmd(
        "Utility for sending error injection command to mock Kafka server.",
        ' ', dory_build_id);
    ValueArg<decltype(args.Host)> arg_host("", "host", "Host to connect to.",
        true, args.Host, "HOST");
    cmd.add(arg_host);
    ValueArg<decltype(args.Port)> arg_port("", "port", "Port to connect to.",
        false, args.Port, "PORT");
    cmd.add(arg_port);
    ValueArg<decltype(args.AckError)> arg_ack_error("", "ack-error",
        "Inject ACK error.", false, args.AckError, "ACK_ERROR");
    cmd.add(arg_ack_error);
    SwitchArg arg_ack_disconnect("", "ack-disconnect",
        "Inject disconnect before sending ACK.", cmd, args.AckDisconnect);
    ValueArg<decltype(args.SingleTopicMdError)> arg_single_topic_md_error("",
        "single-topic-md-error", "Inject single topic metadata error.", false,
        args.SingleTopicMdError, "MD_ERROR");
    cmd.add(arg_single_topic_md_error);
    ValueArg<decltype(args.AllTopicsMdError)> arg_all_topics_md_error("",
        "all-topics-md-error", "Inject all topics metadata error.", false,
        args.AllTopicsMdError, "MD_ERROR");
    cmd.add(arg_all_topics_md_error);
    SwitchArg arg_single_topic_md_disconnect("", "single-topic-md-disconnect",
        "Inject disconnect before single topic metadata response.", cmd,
        args.SingleTopicMdDisconnect);
    SwitchArg arg_all_topics_md_disconnect("", "all-topics-md-disconnect",
        "Inject disconnect before all topics metadata response.", cmd,
        args.AllTopicsMdDisconnect);
    ValueArg<decltype(args.ClientAddr)> arg_client_addr("", "client-addr",
        "Client (specified by IP address) to direct injected error at.", false,
        args.ClientAddr, "ADDR");
    cmd.add(arg_client_addr);
    ValueArg<decltype(args.MsgBody)> arg_msg_body("", "msg-body",
        "Message body to match for ACK error injection.", false,
        args.MsgBody, "MSG");
    cmd.add(arg_msg_body);
    ValueArg<decltype(args.Topic)> arg_topic("", "topic",
        "Topic to match for metadata error injection.", false, args.Topic,
        "TOPIC");
    cmd.add(arg_topic);
    ValueArg<decltype(args.CmdFile)> arg_cmd_file("", "cmd-file",
        "File to read error injection commands from.", false, args.CmdFile,
        "FILE");
    cmd.add(arg_cmd_file);
    cmd.parse(argc, &arg_vec[0]);
    args.Host = arg_host.getValue();
    args.Port = arg_port.getValue();
    args.AckError = arg_ack_error.getValue();
    args.AckDisconnect = arg_ack_disconnect.getValue();
    args.SingleTopicMdError = arg_single_topic_md_error.getValue();
    args.AllTopicsMdError = arg_all_topics_md_error.getValue();
    args.SingleTopicMdDisconnect = arg_single_topic_md_disconnect.getValue();
    args.AllTopicsMdDisconnect = arg_all_topics_md_disconnect.getValue();
    args.ClientAddr = arg_client_addr.getValue();
    args.MsgBody = arg_msg_body.getValue();
    args.Topic = arg_topic.getValue();
    args.CmdFile = arg_cmd_file.getValue();
  } catch (const ArgException &x) {
    throw TInvalidArgError(x.error(), x.argId());
  }
}

TCmdLineArgs::TCmdLineArgs(int argc, const char *const argv[]) {
  ParseArgs(argc, argv, *this);
}

static void CmdFileErr(size_t line_num, const std::string &blurb) {
    std::cerr << "Error on line " << line_num << " of cmd file: " << blurb
        << std::endl;
}

static bool IsBlankOrComment(const std::string &line) {
  size_t i = 0;

  for (; i < line.size() && std::isspace(line[i]); ++i);

  return (i == line.size()) || (line[i] == '#');
}

static bool ReadCmdFile(std::istream &in,
    std::vector<TErrorInjectCmd> &cmd_vec) {
  std::string line;
  std::string client_addr, cmd, arg1, arg2;
  const char *client_addr_ptr = nullptr;
  int16_t err_code = 0;
  size_t line_num = 0;

  while (std::getline(in, line)) {
    ++line_num;

    if (IsBlankOrComment(line)) {
      continue;
    }

    client_addr.clear();

    if (line[0] == '@') {
      /* Command is directed at a specific client IP address. */
      size_t i = line.find(' ');

      if (i == std::string::npos) {
        CmdFileErr(line_num, "no delimiter after client address");
        return false;
      }

      client_addr.assign(line, 1, i - 1);
      line.erase(0, i + 1);
    }

    client_addr_ptr = client_addr.empty() ? nullptr : client_addr.c_str();
    size_t index = line.find(' ');
    bool no_cmd_delimiter = false;

    if (index == std::string::npos) {
      no_cmd_delimiter = true;
      cmd = line;
    }

    cmd.assign(line, 0, index);

    if (cmd == "InjectAckError") {
      if (no_cmd_delimiter) {
        CmdFileErr(line_num, "no cmd delimiter");
        return false;
      }

      size_t index2 = line.find(' ', index + 1);
      size_t n = index + 1;

      if (index2 == std::string::npos) {  // unspecified msg body
        arg1.assign(line, n, line.size() - n);
      } else {
        arg1.assign(line, n, index2 - n);
        n = index2 + 1;
        arg2.assign(line, n, line.size() - n);
      }

      try {
        err_code = boost::lexical_cast<int16_t>(arg1);
      } catch (const boost::bad_lexical_cast &) {
        CmdFileErr(line_num, "invalid ACK error");
        return false;
      }

      cmd_vec.push_back(std::move(
          TErrorInjector::MakeCmdAckError(err_code,
              arg2.empty() ? nullptr : arg2.c_str(), client_addr_ptr)));
    } else if (cmd == "InjectDisconnectBeforeAck") {
      if (no_cmd_delimiter) {
        arg1.clear();
      } else {
        size_t n = index + 1;
        arg1.assign(line, n, line.size() - n);
      }

      cmd_vec.push_back(std::move(
          TErrorInjector::MakeCmdDisconnectBeforeAck(
              arg1.empty() ? nullptr : arg1.c_str(), client_addr_ptr)));
    } else if (cmd == "InjectMetadataResponseError") {
      if (no_cmd_delimiter) {
        CmdFileErr(line_num, "no cmd delimiter");
        return false;
      }

      size_t index2 = line.find(' ', index + 1);

      if (index2 == std::string::npos) {
        CmdFileErr(line_num, "no ACK error delimiter");
        return false;
      }

      size_t n = index + 1;
      arg1.assign(line, n, index2 - n);

      try {
        err_code = boost::lexical_cast<int16_t>(arg1);
      } catch (const boost::bad_lexical_cast &) {
        CmdFileErr(line_num, "invalid ACK error");
        return false;
      }

      n = index2 + 1;
      arg2.assign(line, n, line.size() - n);
      cmd_vec.push_back(std::move(
          TErrorInjector::MakeCmdMetadataResponseError(err_code,
              arg2.c_str(), client_addr_ptr)));
    } else if (cmd == "InjectAllTopicsMetadataResponseError") {
      if (no_cmd_delimiter) {
        CmdFileErr(line_num, "no cmd delimiter");
        return false;
      }

      size_t index2 = line.find(' ', index + 1);

      if (index2 == std::string::npos) {
        CmdFileErr(line_num, "no ACK error delimiter");
        return false;
      }

      size_t n = index + 1;
      arg1.assign(line, n, index2 - n);

      try {
        err_code = boost::lexical_cast<int16_t>(arg1);
      } catch (const boost::bad_lexical_cast &) {
        CmdFileErr(line_num, "invalid ACK error");
        return false;
      }

      n = index2 + 1;
      arg2.assign(line, n, line.size() - n);
      cmd_vec.push_back(std::move(
          TErrorInjector::MakeCmdAllTopicsMetadataResponseError(err_code,
              arg2.c_str(), client_addr_ptr)));
    } else if (cmd == "InjectDisconnectBeforeMetadataResponse") {
      if (no_cmd_delimiter) {
        CmdFileErr(line_num, "no cmd delimiter");
        return false;
      }

      size_t n = index + 1;
      arg1.assign(line, n, line.size() - n);
      cmd_vec.push_back(std::move(
          TErrorInjector::MakeCmdDisconnectBeforeMetadataResponse(
              arg1.c_str(), client_addr_ptr)));
    } else if (cmd == "InjectDisconnectBeforeAllTopicsMetadataResponse") {
      if (line != cmd) {
        CmdFileErr(line_num,
            "extra junk after InjectDisconnectBeforeAllTopicsMetadataResponse "
            "cmd");
        return false;
      }

      cmd_vec.push_back(std::move(
          TErrorInjector::MakeCmdDisconnectBeforeAllTopicsMetadataResponse(
              client_addr_ptr)));
    } else {
      CmdFileErr(line_num, "unknown cmd");
      return false;
    }
  }

  return true;
}

static bool FillCmdVec(const TCmdLineArgs &cfg,
    std::vector<TErrorInjectCmd> &cmd_vec) {
  std::vector<TErrorInjectCmd> result;
  const char *msg_body_str = cfg.MsgBody.empty() ?
                             nullptr : cfg.MsgBody.c_str();
  const char *client_addr_ptr =
      cfg.ClientAddr.empty() ? nullptr : cfg.ClientAddr.c_str();

  if (cfg.AckError) {
    result.push_back(std::move(
        TErrorInjector::MakeCmdAckError(cfg.AckError, msg_body_str,
                                        client_addr_ptr)));
  }

  if (cfg.AckDisconnect) {
    result.push_back(std::move(
        TErrorInjector::MakeCmdDisconnectBeforeAck(msg_body_str,
                                                   client_addr_ptr)));
  }

  if (cfg.SingleTopicMdError) {
    if (cfg.Topic.empty()) {
      std::cerr << "No topic specified for single topic metadata error"
          << std::endl;
      return false;
    }

    result.push_back(std::move(
        TErrorInjector::MakeCmdMetadataResponseError(cfg.SingleTopicMdError,
                                                     cfg.Topic.c_str(),
                                                     client_addr_ptr)));
  }

  if (cfg.AllTopicsMdError) {
    if (cfg.Topic.empty()) {
      std::cerr << "No error topic specified for all topics metadata error"
          << std::endl;
      return false;
    }

    result.push_back(std::move(
        TErrorInjector::MakeCmdAllTopicsMetadataResponseError(
            cfg.SingleTopicMdError, cfg.Topic.c_str(), client_addr_ptr)));
  }

  if (cfg.SingleTopicMdDisconnect) {
    if (cfg.Topic.empty()) {
      std::cerr << "No topic specified for single topic metadata disconnect"
          << std::endl;
      return false;
    }

    result.push_back(std::move(
        TErrorInjector::MakeCmdDisconnectBeforeMetadataResponse(
            cfg.Topic.c_str(), client_addr_ptr)));
  }

  if (cfg.AllTopicsMdDisconnect) {
    result.push_back(std::move(
        TErrorInjector::MakeCmdDisconnectBeforeAllTopicsMetadataResponse(
            client_addr_ptr)));
  }

  if (!cfg.CmdFile.empty()) {
    std::ifstream infile(cfg.CmdFile);

    if (!infile.is_open()) {
      std::cerr << "Failed to open file [" << cfg.CmdFile << "] for reading"
          << std::endl;
      return false;
    }

    infile.exceptions(std::ifstream::badbit);

    try {
      if (!ReadCmdFile(infile, result)) {
        return false;
      }
    } catch (const std::ifstream::failure &) {
      std::cerr << "Error reading file [" << cfg.CmdFile << "]" << std::endl;
      return false;
    }
  }

  result.swap(cmd_vec);
  return true;
}

static int inject_error_main(int argc, const char *const *argv) {
  std::unique_ptr<TCmdLineArgs> args;

  try {
    args.reset(new TCmdLineArgs(argc, argv));
  } catch (const TInvalidArgError &x) {
    /* Error parsing command line arguments. */
    std::cerr << x.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<TErrorInjectCmd> cmd_vec;

  if (!FillCmdVec(*args, cmd_vec)) {
    return EXIT_FAILURE;
  }

  TErrorInjector inj;

  if (!inj.Connect(args->Host.c_str(), args->Port)) {
    std::cerr << "Failed to connect" << std::endl;
    return EXIT_FAILURE;
  }

  int ret = inj.InjectCmdSeq(cmd_vec.begin(), cmd_vec.end());

  if (ret > 0) {
    std::cerr << "Error sending cmd " << ret << " of " << cmd_vec.size()
        << std::endl;
    return EXIT_FAILURE;
  }

  if (ret < 0) {
    std::cerr << "Failed to receive ok ACK for cmd " << ret << " of "
        << cmd_vec.size() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int main(int argc, const char *const *argv) {
  int ret = EXIT_SUCCESS;

  try {
    ret = inject_error_main(argc, argv);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    ret = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "error: uncaught unknown exception" << std::endl;
    ret = EXIT_FAILURE;
  }

  return ret;
}
