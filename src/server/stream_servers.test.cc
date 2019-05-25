/* <server/stream_servers.test.cc>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

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

   Unit tests for <server/stream_server_base.h>, <server/tcp_ipv4_server.h>,
   <server/tcp_ipv6_server.h>, and <server/unix_stream_server.h>.
 */

#include <server/stream_server_base.h>
#include <server/tcp_ipv4_server.h>
#include <server/tcp_ipv6_server.h>
#include <server/unix_stream_server.h>

#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <exception>
#include <list>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <base/error_utils.h>
#include <base/io_utils.h>
#include <base/no_copy_semantics.h>
#include <base/tmp_file.h>

using namespace Base;
using namespace Server;
using namespace Thread;

namespace {

  class TConnectionWorker final {
    NO_COPY_SEMANTICS(TConnectionWorker);

    public:
    TConnectionWorker(TFd &&sock, const struct sockaddr *addr,
        socklen_t addr_len)
        : Sockaddr(CopySockaddr(addr, addr_len)),
          AddrLen(addr_len),
          Sock(std::move(sock)),
          Worker(
              [this]() {
                Run();
              }) {
    }

    ~TConnectionWorker() {
      try {
        Worker.join();
      } catch (...) {
      }
    }

    void Join() {
      assert(this);
      Worker.join();
    }

    private:
    static std::vector<uint8_t> CopySockaddr(const struct sockaddr *addr,
        socklen_t addr_len) {
      const auto *begin = reinterpret_cast<const uint8_t *>(addr);
      return std::vector<uint8_t>(begin, begin + addr_len);
    }

    const struct sockaddr *GetSockaddr() const noexcept {
      assert(this);
      return reinterpret_cast<const struct sockaddr *>(&Sockaddr[0]);
    }

    /* This implements a simple "addition server".  Read a pair of integers
       from the socket and write back their sum.  Keep going until the client
       closes the socket.  Don't worry about putting stuff in network byte
       order or making the integer size unambiguous (for instance, using
       int32_t rather than int) because both communication endpoints are on the
       same host. */
    void Run() {
      assert(this);
      int values[2];

      for (; ; ) {
        try {
          if (!TryReadExactly(Sock, values, sizeof(values))) {
            break;
          }
        } catch (const TUnexpectedEnd &) {
          ASSERT_TRUE(false);
        }

        int result = values[0] + values[1];

        try {
          bool ok = TryWriteExactly(Sock, &result, sizeof(result));
          ASSERT_TRUE(ok);
        } catch (const TUnexpectedEnd &) {
          ASSERT_TRUE(false);
        }
      }
    }

    const std::vector<uint8_t> Sockaddr;

    const socklen_t AddrLen;

    TFd Sock;

    std::thread Worker;
  };  // TConnectionWorker

  class TTestServerConnectionHandler
      : public TStreamServerBase::TConnectionHandlerApi {
    NO_COPY_SEMANTICS(TTestServerConnectionHandler);

    public:
    explicit TTestServerConnectionHandler(
        std::list<TConnectionWorker> &workers)
        : Workers(workers) {
    }

    ~TTestServerConnectionHandler() override {
      for (auto &w : Workers) {
        try {
          w.Join();
        } catch (...) {
        }
      }
    }

    void HandleConnection(TFd &&sock, const struct sockaddr *addr,
        socklen_t addr_len) override {
      assert(this);
      Workers.emplace_back(std::move(sock), addr, addr_len);
    }

    private:
    std::list<TConnectionWorker> &Workers;
  };  // TTestServerConnectionHandler

  /* The fixture for testing class TFdManagedThread. */
  class TStreamServerTest : public ::testing::Test {
    protected:
    TStreamServerTest() = default;

    ~TStreamServerTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TStreamServerTest

  TFd Ipv4ConnectToLocalPort(in_port_t port) {
    TFd fd(socket(AF_INET, SOCK_STREAM, 0));
    struct sockaddr_in servaddr;
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    IfLt0(connect(fd, reinterpret_cast<struct sockaddr *>(&servaddr),
        sizeof(servaddr)));
    return fd;
  }

  TFd Ipv6ConnectToLocalPort(in_port_t port) {
    TFd fd(socket(AF_INET6, SOCK_STREAM, 0));
    struct sockaddr_in6 servaddr;
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_flowinfo = 0;
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_port = htons(port);
    servaddr.sin6_addr/*.s6_addr*/ = in6addr_loopback;
    IfLt0(connect(fd, reinterpret_cast<struct sockaddr *>(&servaddr),
        sizeof(servaddr)));
    return fd;
  }

  TFd UnixStreamConnect(const char *path) {
    TFd fd(socket(AF_LOCAL, SOCK_STREAM, 0));
    struct sockaddr_un servaddr;
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    assert(std::strlen(path) < sizeof(servaddr.sun_path));
    std::strncpy(servaddr.sun_path, path, sizeof(servaddr.sun_path));
    servaddr.sun_path[sizeof(servaddr.sun_path) - 1] = '\0';
    IfLt0(connect(fd, reinterpret_cast<struct sockaddr *>(&servaddr),
        sizeof(servaddr)));
    return fd;
  }

  TEST_F(TStreamServerTest, TcpIpv4Test) {
    std::list<TConnectionWorker> workers;
    TTcpIpv4Server server(16, htonl(INADDR_LOOPBACK), 0,
        std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>(
            new TTestServerConnectionHandler(workers)),
        [](const char *) noexcept {
          ASSERT_TRUE(false);
        });
    ASSERT_FALSE(server.IsBound());
    server.Bind();
    ASSERT_TRUE(server.IsBound());
    ASSERT_EQ(server.GetPort(), 0U);
    in_port_t port = server.GetBindPort();
    ASSERT_FALSE(server.IsStarted());
    bool ok = server.SyncStart();
    ASSERT_TRUE(ok);
    ASSERT_TRUE(server.IsStarted());

    {
      TFd sock_1 = Ipv4ConnectToLocalPort(port);
      TFd sock_2 = Ipv4ConnectToLocalPort(port);
      int input[2] = { 2, 3 };
      ASSERT_TRUE(TryWriteExactly(sock_1, input, sizeof(input)));
      int output = 0;
      ASSERT_TRUE(TryReadExactly(sock_1, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 5);

      input[0] = 100;
      input[1] = 200;
      ASSERT_TRUE(TryWriteExactly(sock_2, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_2, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 300);

      input[0] = 25;
      input[1] = 50;
      ASSERT_TRUE(TryWriteExactly(sock_1, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_1, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 75);

      input[0] = 321;
      input[1] = 123;
      ASSERT_TRUE(TryWriteExactly(sock_2, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_2, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 444);
    }

    server.RequestShutdown();
    server.Join();
    ASSERT_FALSE(server.IsStarted());

    for (auto &w : workers) {
      w.Join();
    }
  }

  TEST_F(TStreamServerTest, TcpIpv6Test) {
    std::list<TConnectionWorker> workers;
    TTcpIpv6Server server(16, in6addr_loopback, 0,
        std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>(
            new TTestServerConnectionHandler(workers)),
        [](const char *) noexcept {
          ASSERT_TRUE(false);
        });
    ASSERT_FALSE(server.IsBound());
    server.Bind();
    ASSERT_TRUE(server.IsBound());
    ASSERT_EQ(server.GetPort(), 0U);
    in_port_t port = server.GetBindPort();
    ASSERT_FALSE(server.IsStarted());
    bool ok = server.SyncStart();
    ASSERT_TRUE(ok);
    ASSERT_TRUE(server.IsStarted());

    {
      TFd sock_1 = Ipv6ConnectToLocalPort(port);
      TFd sock_2 = Ipv6ConnectToLocalPort(port);
      int input[2] = { 2, 3 };
      ASSERT_TRUE(TryWriteExactly(sock_1, input, sizeof(input)));
      int output = 0;
      ASSERT_TRUE(TryReadExactly(sock_1, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 5);

      input[0] = 100;
      input[1] = 200;
      ASSERT_TRUE(TryWriteExactly(sock_2, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_2, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 300);

      input[0] = 25;
      input[1] = 50;
      ASSERT_TRUE(TryWriteExactly(sock_1, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_1, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 75);

      input[0] = 321;
      input[1] = 123;
      ASSERT_TRUE(TryWriteExactly(sock_2, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_2, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 444);
    }

    server.RequestShutdown();
    server.Join();
    ASSERT_FALSE(server.IsStarted());

    for (auto &w : workers) {
      w.Join();
    }
  }

  TEST_F(TStreamServerTest, UnixStreamTest) {
    std::list<TConnectionWorker> workers;
    TTmpFile tmp_file;
    tmp_file.SetDeleteOnDestroy(true);
    TUnixStreamServer server(16, tmp_file.GetName(),
        std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>(
            new TTestServerConnectionHandler(workers)),
        [](const char *) noexcept {
          ASSERT_TRUE(false);
        });
    ASSERT_FALSE(server.IsBound());
    server.Bind();
    ASSERT_TRUE(server.IsBound());
    ASSERT_EQ(server.GetPath(), tmp_file.GetName());
    ASSERT_FALSE(server.IsStarted());
    bool ok = server.SyncStart();
    ASSERT_TRUE(ok);
    ASSERT_TRUE(server.IsStarted());

    {
      TFd sock_1 = UnixStreamConnect(tmp_file.GetName());
      TFd sock_2 = UnixStreamConnect(tmp_file.GetName());
      int input[2] = { 2, 3 };
      ASSERT_TRUE(TryWriteExactly(sock_1, input, sizeof(input)));
      int output = 0;
      ASSERT_TRUE(TryReadExactly(sock_1, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 5);

      input[0] = 100;
      input[1] = 200;
      ASSERT_TRUE(TryWriteExactly(sock_2, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_2, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 300);

      input[0] = 25;
      input[1] = 50;
      ASSERT_TRUE(TryWriteExactly(sock_1, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_1, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 75);

      input[0] = 321;
      input[1] = 123;
      ASSERT_TRUE(TryWriteExactly(sock_2, input, sizeof(input)));
      output = 0;
      ASSERT_TRUE(TryReadExactly(sock_2, &output, sizeof(output), 15000));
      ASSERT_EQ(output, 444);
    }

    server.RequestShutdown();
    server.Join();
    ASSERT_FALSE(server.IsStarted());

    for (auto &w : workers) {
      w.Join();
    }
  }

  TEST_F(TStreamServerTest, UnixStreamFailureTest) {
    std::list<TConnectionWorker> workers;
    char bad_path[] = "/nonexistent/path";
    TUnixStreamServer server(16, bad_path,
        std::unique_ptr<TStreamServerBase::TConnectionHandlerApi>(
            new TTestServerConnectionHandler(workers)),
        [](const char *) noexcept {
          ASSERT_TRUE(false);
        });
    ASSERT_FALSE(server.IsBound());
    ASSERT_EQ(server.GetPath(), bad_path);
    ASSERT_FALSE(server.IsStarted());
    bool ok = server.SyncStart();
    ASSERT_FALSE(ok);

    /* Even though the server failed during initialization, it is still
       considered "started" until its Join() method is called. */
    ASSERT_TRUE(server.IsStarted());
    bool threw = false;

    try {
      server.Join();
    } catch (const TFdManagedThread::TWorkerError &x) {
      try {
        std::rethrow_exception(x.ThrownException);
      } catch (const std::exception &) {
        threw = true;
      }
    }

    ASSERT_TRUE(threw);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
