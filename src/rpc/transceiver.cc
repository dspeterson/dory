/* <rpc/transceiver.cc>

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

   Implements <rpc/transceiver.h>.
 */

#include <rpc/transceiver.h>

#include <cassert>
#include <cstdlib>
#include <new>

#include <base/error_util.h>
#include <base/wr/net_util.h>
#include <base/zero.h>

using namespace Base;
using namespace Rpc;

TTransceiver::TDisconnected::TDisconnected()
    : std::runtime_error("RPC transceiver hung up on") {
}

TTransceiver::TTransceiver()
    : AvailStart(nullptr),
      AvailLimit(nullptr),
      DataStart(nullptr),
      DataLimit(nullptr) {
}

TTransceiver::~TTransceiver() {
  free(AvailStart);
}

TTransceiver::operator bool() const noexcept {
  for (auto *desc = DataStart; desc < DataLimit; ++desc) {
    if (desc->iov_len) {
      return true;
    }
  }
  return false;
}

TTransceiver &TTransceiver::operator+=(size_t size) {
  /* Advance past all descriptors which have been entirely accounted for. */
  while (DataStart < DataLimit && size >= DataStart->iov_len) {
    size -= DataStart->iov_len;
    ++DataStart;
  }

  if (DataStart < DataLimit) {
    /* Adjust the next descriptor to account for the balance. */
    reinterpret_cast<char *&>(DataStart->iov_base) += size;
    DataStart->iov_len -= size;
  } else if (size != 0) {
    /* We're out of descriptors and there's still data to account for.
       This means 'size' was impossibly large. */
    Die("TTransceiver past end of data");
  }

  return *this;
}

iovec *TTransceiver::GetIoVecs(size_t size) {
  size_t avail_size = AvailLimit - AvailStart;

  if (size > avail_size) {
    AvailStart = static_cast<iovec *>(
        realloc(AvailStart, sizeof(iovec) * size));

    if (!AvailStart) {
      AvailLimit = nullptr;
      DataStart  = nullptr;
      DataLimit  = nullptr;
      throw std::bad_alloc();
    }

    AvailLimit = AvailStart + size;
  }

  DataStart = AvailStart;
  DataLimit = DataStart + size;
  return DataStart;
}

size_t TTransceiver::Recv(int sock_fd, int flags) {
  assert(sock_fd >= 0);
  msghdr hdr;
  InitHdr(hdr);
  return GetActualIoSize(Wr::recvmsg(sock_fd, &hdr, flags));
}

size_t TTransceiver::Send(int sock_fd, int flags) {
  assert(sock_fd >= 0);
  msghdr hdr;
  InitHdr(hdr);
  return GetActualIoSize(Wr::sendmsg(sock_fd, &hdr, flags | MSG_NOSIGNAL));
}

void TTransceiver::InitHdr(msghdr &hdr) const noexcept {
  Zero(hdr);
  hdr.msg_iov = DataStart;
  hdr.msg_iovlen = DataLimit - DataStart;
}

size_t TTransceiver::GetActualIoSize(ssize_t io_result) {
  /* A negative value indicates a system error.  However, if the error is
     EPIPE, it just means our peer has hung up on us. */
  if (io_result < 0) {
    if (errno == EPIPE) {
      throw TDisconnected();
    }

    ThrowSystemError(errno);
  }
  /* A non-negative value is the number of bytes successfully transferred.
     If we transferred zero bytes, our peer has hung up on us. */
  if (!io_result) {
    throw TDisconnected();
  }

  /* A positive value is ok to return as-is. */
  return static_cast<size_t>(io_result);
}
