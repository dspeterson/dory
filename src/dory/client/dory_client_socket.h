/* <dory/client/dory_client_socket.h>

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

   Class for writing messages to a UNIX domain datagram socket.
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <utility>

#include <dory/client/dory_client.h>

namespace Dory {

  namespace Client {

    /* This is a simple C++ wrapper around a dory_client_socket_t structure.
       If desired, you can inherit from it, use it as a member in a class of
       your own, or simply use it as-is. */
    class TDoryClientSocket {
      /* Copying and assignment not permitted. */
      TDoryClientSocket(const TDoryClientSocket &) = delete;
      TDoryClientSocket &operator=(TDoryClientSocket &) = delete;

      public:
      /* Create a new Dory socket object.  You must call Bind() to prepare the
         object for sending messages to Dory. */
      TDoryClientSocket() {
        dory_client_socket_init(&Sock);
      }

      virtual ~TDoryClientSocket() noexcept {
        Close();
      }

      /* Move constructor.  Transplant state from 'that' to object being
         constructed, leaving 'that' in an empty (i.e. newly constructed)
         state. */
      TDoryClientSocket(TDoryClientSocket &&that) {
        MoveState(that);
      }

      /* Move assignment operator.  On assignment to self, this is a no-op.
         Otherwise, close socket and transplant state from 'that', leaving
         'that' in an empty (i.e. newly constructed) state. */
      TDoryClientSocket &operator=(TDoryClientSocket &&that) {
        if (this != &that) {
          Close();
          MoveState(that);
        }

        return *this;
      }

      /* Swap our internal state with internal state of 'that'. */
      void Swap(TDoryClientSocket &that) {
        struct sockaddr_un tmp;
        std::memcpy(&tmp, &that.Sock.server_addr, sizeof(tmp));
        std::memcpy(&that.Sock.server_addr, &Sock.server_addr, sizeof(tmp));
        std::memcpy(&Sock.server_addr, &tmp, sizeof(tmp));
        std::swap(Sock.sock_fd, that.Sock.sock_fd);
      }

      /* After calling this method and getting a return value of DORY_OK, you
         are ready to call Send(). */
      int Bind(const char *server_path) {
        return dory_client_socket_bind(&Sock, server_path);
      }

      /* A true return value indicates that the socket is bound and ready for
         sending messages to Dory via Send() method below.  Otherwise, you
         must call Bind() before sending. */
      bool IsBound() const {
        return (Sock.sock_fd >= 0);
      }

      /* Send a message to Dory.  You must call Bind() above with a successful
         return value before calling this method. */
      int Send(const void *msg, size_t msg_size) const {
        return dory_client_socket_send(&Sock, msg, msg_size);
      }

      /* Call this method when you are done sending messages to Dory.  It is
         harmless to call Close() on an already closed object.  After calling
         Close(), you can call Bind() again if you wish to resume communication
         with Dory. */
      void Close() {
        dory_client_socket_close(&Sock);
      }

      private:
      /* Helper method for move construction and assignment. */
      void MoveState(TDoryClientSocket &that) {
        std::memcpy(&Sock.server_addr, &that.Sock.server_addr,
            sizeof(Sock.server_addr));
        Sock.sock_fd = that.Sock.sock_fd;
        that.Sock.sock_fd = -1;
      }

      dory_client_socket_t Sock;
    };  // TDoryClientSocket

  }  // Client

}  // Dory
