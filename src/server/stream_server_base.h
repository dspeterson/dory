/* <server/stream_server_base.h>

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

   Base class for server that uses TCP or UNIX domain stream sockets for
   communication with clients.
 */

#pragma once

#include <cassert>
#include <functional>
#include <memory>

#include <sys/socket.h>
#include <sys/types.h>

#include <base/event_semaphore.h>
#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <thread/fd_managed_thread.h>

namespace Server {

  /* Base class for server that communicates with clients using some form of
     stream sockets.  Derived classes implement details of specific socket
     types such as TCP/IPv4, TCP/IPv6, and UNIX domain stream sockets.  Each
     instance of this class has its own acceptor thread, which executes in a
     loop, accepting client connections and handing them off to derived class
     code, which will typically assign them to worker threads. */
  class TStreamServerBase : public Thread::TFdManagedThread {
    NO_COPY_SEMANTICS(TStreamServerBase);

    public:
    /* Fatal error handler.  Function should report error and terminate
       program immediately. */
    using TFatalErrorHandler = std::function<void(const char *) noexcept>;

    /* Defines API of caller-supplied class for handling new client
       connections. */
    class TConnectionHandlerApi {
      NO_COPY_SEMANTICS(TConnectionHandlerApi);

      public:
      virtual ~TConnectionHandlerApi() noexcept {
      }

      /* Each time a new client connection is received, this method is called.
         Parameters:

             sock: The socket file descriptor for the new connection.  On
                 return, this must be empty.  Typically, the implementation
                 will transfer the socket to a worker thread.
             addr: A pointer to the sockaddr structure containing info about
                 the connected client.  This is the same pointer passed to the
                 TStreamServerBase constructor, and is passed to accept().  If
                 the implementation code for this method wants to preserve this
                 information, it must make its own copy since the memory will
                 be reused for the next client connection.
             addr_len: The size in bytes of the data pointed to by 'addr'.
                 This value is obtained from the call to accept().  If 'addr'
                 is null, this will be 0.
       */
      virtual void HandleConnection(Base::TFd &&sock,
          const struct sockaddr *addr, socklen_t addr_len) = 0;

      /* Called by acceptor thread each time an accept() system call returns a
         nonfatal error.  The base class implementation does nothing.  Derived
         classes interested in these events should override this method. */
      virtual void HandleNonfatalAcceptError(int errno_value);

      protected:
      TConnectionHandlerApi() = default;
    };  // TConnectionHandlerApi

    virtual ~TStreamServerBase() noexcept;

    void Bind();

    bool IsBound() const noexcept {
      assert(this);
      return ListeningSocket.IsOpen();
    }

    /* To start the server, you can call the Start() method of our base class,
       or you can call this method.  The difference is that this method doesn't
       return until the acceptor thread has either successfully called listen()
       or encountered a failure preventing a successful call to listen().
       Therefore, on return it is guaranteed that you can connect to the server
       without getting "connection refused", provided that the server started
       successfully.  This method returns true if the server initialized
       successfully or false otherwise. */
    bool SyncStart();

    void Reset();

    const TConnectionHandlerApi &GetConectionHandler() const noexcept {
      assert(this);
      return *ConnectionHandler;
    }

    TConnectionHandlerApi &GetConectionHandler() noexcept {
      assert(this);
      return *ConnectionHandler;
    }

    protected:
    /* Derived class constructor should initialize any info, such as port or
       UNIX domain socket path, necessary for bind().  Parameters:

           backlog: Backlog value to pass to listen().
           addr: Pointer to a buffer provided by the derived class, which is
               passed to accept(), or nullptr if caller doesn't want client
               info.
           addr_space: Size in bytes of space pointed to by 'addr', or 0 if
               'addr' is null.
           connection_handler: Pointer to an object whose purpose is to handle
               new client connections.  The TStreamServerBase takes ownership
               of 'connection_handler', and is responsible for its destruction.
           fatal_error_handler: Fatal error hander, which should report error
               message and terminate program immediately.
     */
    TStreamServerBase(int backlog, struct sockaddr *addr,
        socklen_t addr_space, TConnectionHandlerApi *connection_handler,
        const TFatalErrorHandler &fatal_error_handler);

    TStreamServerBase(int backlog, struct sockaddr *addr,
        socklen_t addr_space, TConnectionHandlerApi *connection_handler,
        TFatalErrorHandler &&fatal_error_handler);

    /* On entry, 'sock' is empty.  Derived class calles socket() and bind() in
       here.  Must throw if operation fails. */
    virtual void InitListeningSocket(Base::TFd &sock) = 0;

    /* The base class version simply closes the socket.  Derived classes can
       override this to perform extra steps, such as unlinking the path
       associated with a UNIX domain socket. */
    virtual void CloseListeningSocket(Base::TFd &sock);

    virtual void Run() override;

    const Base::TFd &GetListeningSocket() const noexcept {
      assert(this);
      return ListeningSocket;
    }

    private:
    void AcceptClients();

    /* Client-supplied fatal error handler.  This should report error and
       immediately terminate program. */
    const TFatalErrorHandler FatalErrorHandler;

    const int Backlog;

    struct sockaddr * const Addr;

    const socklen_t AddrSpace;

    bool SyncStartSuccess;

    Base::TEventSemaphore *SyncStartNotify;

    Base::TFd ListeningSocket;

    std::unique_ptr<TConnectionHandlerApi> ConnectionHandler;
  };  // TStreamServerBase

}  // Server
