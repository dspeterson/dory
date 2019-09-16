/* <base/counter.test.cc>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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

   Unit test for <base/counter.h>.
 */

#include <base/counter.h>

#include <cstdio>
#include <cstring>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/fd.h>
#include <base/zero.h>

#include <gtest/gtest.h>

using namespace Base;

namespace {

  DEFINE_COUNTER(Connections);
  DEFINE_COUNTER(Requests);

  const size_t BufSize = 1024;

  static void ServerMain(int die, int sock, bool *success) {
    try {
      *success = true;
      TFd ep(epoll_create1(0));
      epoll_event event;
      Zero(event);
      event.data.fd = die;
      event.events = EPOLLIN;
      IfLt0(epoll_ctl(ep, EPOLL_CTL_ADD, die, &event));
      event.data.fd = sock;
      event.events = EPOLLIN;
      IfLt0(epoll_ctl(ep, EPOLL_CTL_ADD, sock, &event));

      for (; ; ) {
        IfLt0(epoll_wait(ep, &event, 1, -1));

        if (event.data.fd == die) {
          break;
        }

        if (event.data.fd == sock) {
          Connections.Increment();
          TFd cli(accept(sock, 0, 0));

          for (; ; ) {
            char buf[BufSize];
            ssize_t size;
            IfLt0(size = read(cli, buf, BufSize));

            if (!size) {
              break;
            }

            Requests.Increment();
            IfLt0(write(cli, buf, size));
          }
        }
      }
    } catch (...) {
      *success = false;
    }
  }

  static void ClientMain(int id, in_port_t port, uint32_t request_count,
      bool *success) {
    try {
      *success = true;
      TFd my_socket(socket(AF_INET, SOCK_STREAM, 0));
      sockaddr_in addr;
      Zero(addr);
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      IfLt0(connect(my_socket, reinterpret_cast<sockaddr *>(&addr),
          sizeof(addr)));

      for (uint32_t i = 0; i < request_count; ++i) {
        char request[BufSize];
        sprintf(request, "%d %d", id, i);
        ssize_t request_size = strlen(request);
        ssize_t size;
        IfLt0(size = write(my_socket, request, request_size));

        if (size != request_size) {
          throw 0;
        }

        char reply[BufSize];
        IfLt0(size = read(my_socket, reply, BufSize));

        if ((size != request_size) || strncmp(request, reply, request_size)) {
          throw 0;
        }
      }
    } catch (...) {
      *success = false;
    }
  }

  in_port_t GetBindPort(int listening_socket) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    IfLt0(getsockname(listening_socket,
        reinterpret_cast<struct sockaddr *>(&addr), &addrlen));
    return ntohs(addr.sin_port);
  }

  /* The fixture for testing counters. */
  class TCounterTest : public ::testing::Test {
    protected:
    TCounterTest() = default;

    ~TCounterTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TCounterTest

  TEST_F(TCounterTest, Typical) {
    int fds[2];
    IfLt0(pipe(fds));
    TFd recv_die(fds[0]), send_die(fds[1]);
    TFd listening_socket(socket(AF_INET, SOCK_STREAM, 0));
    sockaddr_in server_addr;
    Zero(server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = 0;  // bind() to ephemeral port
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    IfLt0(bind(listening_socket,
        reinterpret_cast<const sockaddr *>(&server_addr), sizeof(server_addr)));
    const in_port_t bind_port = GetBindPort(listening_socket);
    IfLt0(listen(listening_socket, 5));
    bool server_success = false,
        client_1_success = false,
        client_2_success = false,
        client_3_success = false;
    static const uint32_t request_count = 5;
    std::thread
        server(&ServerMain, static_cast<int>(recv_die),
               static_cast<int>(listening_socket), &server_success),
        client_1(&ClientMain, 101, bind_port, request_count,
            &client_1_success),
        client_2(&ClientMain, 102, bind_port, request_count,
            &client_2_success),
        client_3(&ClientMain, 103, bind_port, request_count,
            &client_3_success);
    client_1.join();
    client_2.join();
    client_3.join();
    IfLt0(write(send_die, "x", 1));
    server.join();
    ASSERT_TRUE(server_success);
    ASSERT_TRUE(client_1_success);
    ASSERT_TRUE(client_2_success);
    ASSERT_TRUE(client_3_success);
    TCounter::Sample();
    ASSERT_EQ(Connections.GetCount(), 3U);
    ASSERT_EQ(Requests.GetCount(), request_count * 3);
    TCounter::Reset();
    ASSERT_FALSE(Connections.GetCount());
    ASSERT_FALSE(Requests.GetCount());
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
