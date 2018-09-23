/* <thread/fd_managed_thread.cc>

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

   Implements <thread/fd_managed_thread.h>.
 */

#include <thread/fd_managed_thread.h>

using namespace Base;
using namespace Thread;

TFdManagedThread::~TFdManagedThread() noexcept {
  assert(this);

  /* This should have already been called by a subclass destructor.  Calling it
     here a second time is harmless and acts as a safeguard, just in case some
     subclass omits calling it. */
  ShutdownOnDestroy();
}

void TFdManagedThread::Start() {
  assert(this);

  if (Thread.joinable()) {
    throw std::logic_error("Worker thread is already started");
  }

  assert(!ShutdownRequestedSem.GetFd().IsReadable());
  assert(!ShutdownFinishedSem.GetFd().IsReadable());
  assert(!ThrownByThread);

  /* Start the thread running. */
  Thread = std::thread(
      [this]() {
        RunAndTerminate();
      });
}

bool TFdManagedThread::IsStarted() const noexcept {
  assert(this);
  return Thread.joinable();
}

void TFdManagedThread::RequestShutdown() {
  assert(this);

  if (!Thread.joinable()) {
    throw std::logic_error(
        "Cannot request shutdown on nonexistent worker thread");
  }

  ShutdownRequestedSem.Push();
}

const TFd &TFdManagedThread::GetShutdownWaitFd() const {
  assert(this);
  return ShutdownFinishedSem.GetFd();
}

void TFdManagedThread::Join() {
  assert(this);

  if (!Thread.joinable()) {
    throw std::logic_error("Cannot join nonexistent worker thread");
  }

  Thread.join();
  ShutdownFinishedSem.Reset();
  ShutdownRequestedSem.Reset();

  if (ThrownByThread) {
    /* Note that the TWorkerError constructor leaves 'ThrownByThread' with a
       null value. */
    throw TWorkerError(ThrownByThread);
  }
}

void TFdManagedThread::ShutdownOnDestroy() {
  assert(this);

  if (Thread.joinable()) {
    ShutdownRequestedSem.Push();

    try {
      Join();
    } catch (...) {
      /* Ignore any uncaught exceptions from thread. */
    }
  }

  assert(!ShutdownFinishedSem.GetFd().IsReadable());
  assert(!Thread.joinable());
}

void TFdManagedThread::RunAndTerminate() {
  assert(this);

  try {
    Run();
  } catch (...) {
    ThrownByThread = std::current_exception();
  }

  /* Let others know that we are about to terminate. */
  ShutdownFinishedSem.Push();

  /* On return, the thread dies. */
}
