/* <thread/managed_thread_pool.h>

   ----------------------------------------------------------------------------
   Copyright 2015 Dave Peterson <dave@dspeterson.com>

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

   Managed thread pool, where work to be done by thread is supplied by a
   callable object whose type is given as a template parameter.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <exception>
#include <functional>
#include <utility>

#include <base/no_copy_semantics.h>
#include <thread/managed_thread_pool_base.h>
#include <thread/managed_thread_pool_config.h>

namespace Thread {

  /* Managed thread pool class, where threads perform work by calling a
     callable object of type TWorkCallable.  The requirements for TWorkCallable
     are as follows:

         1.  It must be constructible as follows:

                 TWorkCallable(nullptr)

         2.  It must be possible to assign nullptr to it.  For instance, if
             TWorkCallable is a class then an assignment operator method such
             as this would satisfy the requirement:

                 TWorkCallable &TWorkCallable::operator=(nullptr_t);

             The assignment should have the effect of releasing any resources
             held within the function object, and should not throw.  Any
             exception escaping from the assignment operator will cause
             invocation of TManagedThreadPoolBase::HandleFatalError().

         3.  It must be callable, taking no parameters.  Any returned value
             will be ignored.

      Note that both std::function<void()> and function pointers of type
      void (*)() satisfy all of these requirements. */
  template <typename TWorkCallable>
  class TManagedThreadPool : public TManagedThreadPoolBase {
    NO_COPY_SEMANTICS(TManagedThreadPool);

    class TWorker;

    public:
    /* Wrapper class for thread obtained from pool. */
    class TReadyWorker final
        : public TManagedThreadPoolBase::TReadyWorkerBase {
      NO_COPY_SEMANTICS(TReadyWorker);

      public:
      /* Construct an empty wrapper (i.e. a wrapper that contains no thread).
       */
      TReadyWorker() = default;

      /* Any thread contained by donor is moved into the wrapper being
         constructed, leaving the donor empty. */
      TReadyWorker(TReadyWorker &&) noexcept = default;

      /* Releases the thread (i.e. puts it back in the pool if appropriate) if
         base class Launch() method has not been called. */
      ~TReadyWorker() override = default;

      /* Move any thread contained by donor into the assignment target, leaving
         the donor empty.  Release any thread prevoiusly contained by
         assignment target. */
      TReadyWorker &operator=(TReadyWorker &&) noexcept = default;

      /* Swap internal state with 'that'. */
      void Swap(TReadyWorker &that) noexcept {
        TManagedThreadPoolBase::TReadyWorkerBase::Swap(that);
      }

      /* Get the function object for the ready worker.  The caller can then
         assign a value to it before calling Launch().  Must only be called
         when wrapper is nonempty (i.e. IsLaunchable() returns true). */
      TWorkCallable &GetWorkFn() noexcept {
        assert(GetWorker());
        return GetWorker()->GetWorkFn();
      }

      /* Get the pool that the contained worker belongs to.  Must only be
         called when wrapper is nonempty (i.e. IsLaunchable() returns true). */
      TManagedThreadPool &GetPool() const {
        assert(GetWorker());
        return static_cast<TManagedThreadPool &>(GetWorker()->GetPool());
      }

      private:
      /* Called by TManagedThreadPool to wrap thread obtained from pool. */
      explicit TReadyWorker(TWorker *worker) noexcept
          : TManagedThreadPoolBase::TReadyWorkerBase(worker) {
      }

      /* Return pointer to contained worker, or nullptr if wrapper is empty.
         Pool maintains ownership of worker and controls its lifetime. */
      TWorker *GetWorker() const noexcept {
        return static_cast<TWorker *>(GetWorkerBase());
      }

      /* so TManagedThreadPool can call private constructor */
      friend class TManagedThreadPool;
    };  // TReadyWorker

    /* Construct thread pool with given configuration.
     */
    explicit TManagedThreadPool(const TManagedThreadPoolConfig &cfg)
        : TManagedThreadPoolBase(cfg) {
    }

    /* Construct thread pool with default configuration. */
    TManagedThreadPool() = default;

    /* Allocate a worker from the pool and return a wrapper object containing
       it.  If the pool idle list is empty prior to successful allocation, a
       new worker will be created and added to pool.  In the case where a
       maximum pool size has been configured, an empty wrapper will be returned
       when allocation fails due to the size limit.  Call method IsLaunchable()
       of TReadyWorker object to verify that the wrapper is nonempty before
       attempting to launch the thread it contains (not necessary if no max
       pool size is configured). */
    TReadyWorker GetReadyWorker() {
      return TReadyWorker(static_cast<TWorker *>(GetAvailableWorker()));
    }

    protected:
    /* Our base class calls this when it needs to create a new thread to add to
       the pool. */
    TWorkerBase *CreateWorker(bool start) override {
      return new TWorker(*this, start);
    }

    private:
    /* A worker thread.  Our base class creates these, adds them to the pool as
       needed, and destroys them when they have been idle too long (as defined
       by pool config parameters). */
    class TWorker final : public TManagedThreadPoolBase::TWorkerBase {
      NO_COPY_SEMANTICS(TWorker);

      public:
      /* If 'start' is true, then the thread is created immediately and enters
         the idle state.  Otherwise, the thread is not created immediately
         (i.e. the worker starts out empty). */
      TWorker(TManagedThreadPool &my_pool, bool start)
          : TManagedThreadPoolBase::TWorkerBase(my_pool, start),
            WorkFn(nullptr) {
      }

      ~TWorker() override = default;

      /* Return function pointer or object that worker calls to perform work.
       */
      TWorkCallable &GetWorkFn() noexcept {
        return WorkFn;
      }

      protected:
      /* Perform work by calling the client-defined callable object. */
      void DoWork() override {
        WorkFn();
      }

      void DoClearClientState() override {
        /* Depending on the type of TWorkCallable, this may invoke an
           overloaded operator function, which must not throw since any
           exception that escapes from here will cause invocation of the fatal
           error handler. */
        WorkFn = nullptr;
      }

      private:
      /* Function pointer or callable object that worker calls to perform work.
       */
      TWorkCallable WorkFn;
    };  // TWorker
  };  // TManagedThreadPool

}  // Thread
