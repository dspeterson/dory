/* <thread/managed_thread_pool_base.cc>

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

   Implements <thread/managed_thread_pool_base.h>.
 */

#include <thread/managed_thread_pool_base.h>

#include <algorithm>
#include <array>

#include <base/error_utils.h>
#include <base/time_util.h>

#include <poll.h>

using namespace Base;
using namespace Thread;

TManagedThreadPoolBase::~TManagedThreadPoolBase() {
  try {
    if (IsStarted()) {
      /* This handles the case where a fatal exception is causing unexpected
         destruction of the thread pool before it has been properly shut down.
         Under normal operation, we should never get here. */
      RequestShutdown();
      WaitForShutdown();
    }
  } catch (...) {
  }
}

void TManagedThreadPoolBase::SetConfig(const TManagedThreadPoolConfig &cfg) {
  assert(this);
  bool notify = false;

  {
    std::lock_guard<std::mutex> lock(PoolLock);

    if (cfg != Config) {
      ++Stats.SetConfigCount;
      Config = cfg;
      notify = !ReconfigPending;
      ReconfigPending = true;
    }
  }

  if (notify) {
    /* Tell manager thread to update pool config. */
    ReconfigSem.Push();
  }
}

void TManagedThreadPoolBase::Start(bool populate) {
  assert(this);

  if (Manager.IsStarted()) {
    throw std::logic_error("Thread pool is already started");
  }

  assert(!AllWorkersFinished.GetFd().IsReadable());

  /* Reset any remaining state from previous run. */
  ErrorPendingSem.Reset();

  size_t create_count = populate ? Config.GetMinPoolSize() : 0;
  std::list<TWorkerBasePtr> initial_workers(create_count);

  for (auto &item : initial_workers) {
    item.reset(CreateWorker(true));
  }

  std::list<TWorkerError> old_worker_errors;

  {
    std::lock_guard<std::mutex> lock(PoolLock);
    assert(IdleList.Empty());
    assert(IdleList.SegmentCount() == 1);
    assert(BusyList.empty());
    assert(LiveWorkerCount == 0);

    /* Reset any remaining state from previous run. */
    old_worker_errors = std::move(WorkerErrorList);
    Stats = TManagedThreadPoolStats();

    Stats.CreateWorkerCount += create_count;
    IdleList.AddNew(initial_workers);
    LiveWorkerCount = create_count;
    PoolIsReady = true;
  }

  Manager.Start();
}

std::list<TManagedThreadPoolBase::TWorkerError>
TManagedThreadPoolBase::GetAllPendingErrors() {
  assert(this);
  std::list<TWorkerError> result;

  {
    std::lock_guard<std::mutex> lock(PoolLock);
    result = std::move(WorkerErrorList);
  }

  if (!result.empty()) {
    ErrorPendingSem.Pop();
  }

  return result;
}

TManagedThreadPoolStats TManagedThreadPoolBase::GetStats() const {
  assert(this);

  std::lock_guard<std::mutex> lock(PoolLock);
  Stats.LiveWorkerCount = LiveWorkerCount;
  Stats.IdleWorkerCount = IdleList.Size();
  return Stats;
}

void TManagedThreadPoolBase::RequestShutdown() {
  assert(this);

  if (!Manager.IsStarted()) {
    throw std::logic_error(
        "Cannot call RequestShutdown() on thread pool that is not started");
  }

  Manager.RequestShutdown();
}

void TManagedThreadPoolBase::WaitForShutdown() {
  assert(this);

  if (!Manager.IsStarted()) {
    throw std::logic_error(
        "Cannot call WaitForShutdown() on thread pool that is not started");
  }

  try {
    Manager.Join();
  } catch (const TFdManagedThread::TWorkerError &x) {
    try {
      std::rethrow_exception(x.ThrownException);
    } catch (const std::exception &y) {
      std::string msg("Thread pool manager threw exception: ");
      msg += y.what();
      HandleFatalError(msg.c_str());
    } catch (...) {
      HandleFatalError("Thread pool manager threw unknown exception");
    }
  }

  /* At this point the manager and all workers have terminated, so we shouldn't
     need to acquire 'PoolLock'.  Acquire it anyway, just in case a possibly
     buggy client tries to access the pool while we are still shutting down.
     Defensive programming doesn't cost us anything here. */
  {
    std::lock_guard<std::mutex> lock(PoolLock);
    assert(IdleList.Empty());
    assert(BusyList.empty());
    assert(LiveWorkerCount == 0);
    assert(!PoolIsReady);
    ReconfigPending = false;
  }

  AllWorkersFinished.Reset();
  ReconfigSem.Reset();
}

void TManagedThreadPoolBase::TWorkerBase::PutBack(
    TWorkerBase *worker) noexcept {
  /* Note: This code may be invoked from a destructor, so we avoid letting
     exceptions escape. */
  assert(worker);

  /* Get pool here, since worker may no longer exist when we need to use pool
     below.  This is guaranteed not to throw. */
  TManagedThreadPoolBase &pool = worker->GetPool();

  try {
    DoPutBack(worker);
    /* At this point, the worker may no longer exist. */
  } catch (const std::exception &x) {
    std::string msg(
        "Fatal exception when releasing unused thread pool worker: ");
    msg += x.what();
    pool.HandleFatalError(msg.c_str());
  } catch (...) {
    pool.HandleFatalError("Fatal unknown exception when releasing unused "
        "thread pool worker");
  }
}

TManagedThreadPoolBase::TWorkerBase::~TWorkerBase() {
  if (WorkerThread.joinable()) {
    /* should happen only on fatal error */
    WorkerThread.join();
  }
}

void TManagedThreadPoolBase::TWorkerBase::Activate() {
  assert(this);

  if (WorkerThread.joinable()) {
    /* The thread was obtained from the idle list, and has been placed on the
       busy list but not yet awakened.  When we release 'WakeupWait' below, it
       will awaken and interpret the value we set here as indication that it
       has work to do.  It would interpret the opposite value as a request to
       terminate from the manager pruning the idle list. */
    TerminateRequested = false;
  } else {
    /* The pool had no available threads, so we are creating a new one.  Create
       the thread, and start it running in the busy state.  At this point its
       TWorkerBase object (whose Activate() method we are now executing) has
       already been added to the busy list. */
    WorkerThread = std::thread(
        [this]() {
          BusyRun();
        });
  }

  assert(WorkerThread.joinable());

  /* If the thread was obtained from the idle list, this starts it working.

     If we created the thread above, it may finish its work before we finish
     the above move-assignment to 'WorkerThread'.  In this case, it sleeps
     until we wake it up here, _after_ we have finished the move-assignment.
     Thus the lock prevents the following race condition:

         1.  The thread quickly finishes its work, before we assign to
             'WorkerThread' above.  It then returns itself (i.e. the
             TWorkerBase object whose Activate() method we are now executing)
             to the idle list.

         2.  The TWorkerBase object is then allocated from the idle list to
             satisfy some other client request.

         3.  Bad things happen when we try to assign to 'WorkerThread' while
             the other client thinks it owns our TWorkerBase object.

     An alternative way to prevent the above problem is to acquire 'PoolLock'
     before creating the thread.  Instead, we do things this way to avoid
     performing a potentially time-consuming thread creation operation while
     holding 'PoolLock'.  In the typical case, the thread avoids sleeping
     because it acquires the lock long after we release it here. */
  WakeupWait.unlock();
}

TManagedThreadPoolBase::TWorkerBase::TWorkerBase(
    TManagedThreadPoolBase &my_pool, bool start)
    : MyPool(my_pool),
      BusyListPos(my_pool.BusyList.end()) {
  /* Lock this _before_ starting the worker, so it blocks when attempting to
     acquire the lock.  The worker sleeps on this when in the idle state. */
  WakeupWait.lock();

  if (start) {
    WorkerThread = std::thread(
        [this]() {
          IdleRun();
        });
  }
}

void TManagedThreadPoolBase::TWorkerBase::DoPutBack(TWorkerBase *worker) {
  assert(worker);

  /* Allow worker to release any resources it holds before possibly returning
     to idle list. */
  worker->ClearClientState();

  /* This is either empty or contains a single item: the smart pointer that
     owns the object pointed to by 'worker'.  If it goes out of scope nonempty
     then the worker object gets destroyed. */
  std::list<TWorkerBasePtr> w_ptr;

  bool shutdown_notify = false;
  TManagedThreadPoolBase &pool = worker->GetPool();

  /* When true, we are putting back a worker object that was obtained from the
     idle list, and contains an idle thread.  When false, we are disposing of a
     worker object that was created because the pool had no idle workers.  In
     the latter case, the worker object doesn't yet contain a thread, but it is
     on the busy list and pool.LiveWorkerCount has been incremented for it. */
  bool from_pool = worker->IsStarted();

  {
    std::lock_guard<std::mutex> lock(pool.PoolLock);
    ++pool.Stats.PutBackCount;
    assert(worker->BusyListPos != pool.BusyList.end());
    worker->XferFromBusyList(w_ptr);
    assert(w_ptr.size() == 1);
    assert(pool.LiveWorkerCount);

    if (pool.PoolIsReady) {
      /* Return worker to idle list if it was obtained from there.  Otherwise
         we will destroy worker below. */
      if (from_pool) {
        pool.IdleList.AddNew(w_ptr);
        assert(w_ptr.empty());
      }
    } else {
      /* The pool is shutting down, so we will destroy the worker regardless of
         whether it came from the idle list.  If we are destroying the last
         remaining worker, we must notify the pool manager that the shutdown is
         complete. */
      shutdown_notify = (pool.LiveWorkerCount == 1);
    }

    if (!w_ptr.empty()) {
      /* We are destroying the worker, so we must decrement this. */
      --pool.LiveWorkerCount;
    }
  }

  if (from_pool && !w_ptr.empty()) {
    /* The worker we are destroying came from the idle list, and therefore
       contains a thread (in the idle state).  We must terminate the thread. */
    TWorkerBasePtr &p = w_ptr.front();
    p->Terminate();
    p->Join();
  }

  assert(w_ptr.empty() || !worker->IsStarted());

  if (shutdown_notify) {
    pool.AllWorkersFinished.Push();
  }
}

void TManagedThreadPoolBase::TWorkerBase::ClearClientState() noexcept {
  assert(this);

  try {
    /* This should not throw, but be prepared just in case it does. */
    DoClearClientState();
  } catch (const std::exception &x) {
    std::string msg(
        "Fatal exception while clearing thread pool worker state: ");
    msg += x.what();
    HandleFatalError(msg.c_str());
  } catch (...) {
    HandleFatalError("Fatal unknown exception while clearing thread pool "
        "worker state");
  }
}

void TManagedThreadPoolBase::TWorkerBase::Terminate() {
  assert(this);
  assert(WorkerThread.joinable());
  TerminateRequested = true;
  WakeupWait.unlock();
}

void TManagedThreadPoolBase::TWorkerBase::XferFromBusyList(
    std::list<TWorkerBasePtr> &dst) noexcept {
  assert(this);
  assert(BusyListPos != MyPool.BusyList.end());
  dst.splice(dst.end(), MyPool.BusyList, BusyListPos);
  BusyListPos = MyPool.BusyList.end();
}

TManagedThreadPoolBase::TWorkerBase::TAfterBusyAction
TManagedThreadPoolBase::TWorkerBase::LeaveBusyList() noexcept {
  assert(this);
  assert(BusyListPos != MyPool.BusyList.end());
  TAfterBusyAction next_action = TAfterBusyAction::BecomeIdle;
  std::list<TWorkerBasePtr> my_ptr;
  XferFromBusyList(my_ptr);
  assert(my_ptr.size() == 1);

  if (MyPool.PoolIsReady) {
    /* We are becoming idle so put self back on idle list. */
    MyPool.IdleList.AddNew(my_ptr);
  } else {
    /* Pool is shutting down, so we will terminate.  Put self on join list to
       be cleaned up by manager thread. */
    MyPool.JoinList.splice(MyPool.JoinList.end(), my_ptr);
    assert(MyPool.LiveWorkerCount);
    --MyPool.LiveWorkerCount;

    /* If we are the last remaining thread, we must notify the manager that
       shutdown is complete. */
    next_action = (MyPool.LiveWorkerCount == 0) ?
        TAfterBusyAction::NotifyAndTerminate :
        TAfterBusyAction::Terminate;
  }

  assert(my_ptr.empty());
  return next_action;
}

void TManagedThreadPoolBase::TWorkerBase::DoBusyRun() {
  assert(this);
  std::list<TWorkerError> error;
  TAfterBusyAction next_action = TAfterBusyAction::BecomeIdle;

  do {
    /* enter busy state */
    assert(BusyListPos != MyPool.BusyList.end());

    /* Note that we are accessing MyPool.PoolIsReady even though we don't hold
       MyPool.PoolLock.  In this case, it's ok.  The test is not needed for
       correctness, but helps ensure fast response to a shutdown request. */
    if (MyPool.PoolIsReady) {
      /* Perform work for client.  If client code throws, report error.  Be
         sure to clear client state afterwards, regardless of whether exception
         was thrown. */

      try {
        DoWork();
      } catch (...) {
        ClearClientState();
        error.emplace_back();
      }

      if (error.empty()) {
        ClearClientState();
      }
    }

    bool error_notify = false;

    if (WaitAfterDoWork) {
      /* This prevents a race condition, which could otherwise occur if we
         finished our work quickly, before the client that launched us assigns
         our std::thread object to 'WorkerThread'.  In that case, we sleep here
         to avoid returning to the idle list before 'WorkerThread' has been
         assigned.  In most cases we avoid sleeping because the lock will
         already be available when we get here. */
      WaitAfterDoWork = false;
      WakeupWait.lock();
    }

    {
      std::lock_guard<std::mutex> lock(MyPool.PoolLock);
      ++MyPool.Stats.FinishWorkCount;

      if (!error.empty()) {
        ++MyPool.Stats.QueueErrorCount;
        error_notify = MyPool.WorkerErrorList.empty();

        if (error_notify) {
          ++MyPool.Stats.NotifyErrorCount;
        }

        MyPool.WorkerErrorList.splice(MyPool.WorkerErrorList.end(), error);
      }

      /* Return to idle list unless pool is shutting down. */
      next_action = LeaveBusyList();
    }

    assert(error.empty());

    if (error_notify) {
      MyPool.ErrorPendingSem.Push();
    }

    if (next_action != TAfterBusyAction::BecomeIdle) {
      break;  // terminate because pool is shutting down
    }

    WakeupWait.lock();  // sleep in idle state
  } while (!TerminateRequested);

  if (next_action == TAfterBusyAction::NotifyAndTerminate) {
    /* We are the last remaining worker, so notify manager that shutdown is
       complete. */
    MyPool.AllWorkersFinished.Push();
  }
}

void TManagedThreadPoolBase::TWorkerBase::BusyRun() {
  assert(this);

  /* If an exception escapes from DoBusyRun(), the source is the thread pool
     implementation, not client code.  Any exceptions thrown by client code are
     caught inside DoBusyRun(). */
  try {
    DoBusyRun();
  } catch (const std::exception &x) {
    std::string msg("Fatal exception in thread pool worker: ");
    msg += x.what();
    MyPool.HandleFatalError(msg.c_str());
  } catch (...) {
    MyPool.HandleFatalError("Fatal unknown exception in thread pool worker");
  }
}

void TManagedThreadPoolBase::TWorkerBase::IdleRun() {
  assert(this);

  /* If an exception escapes from DoBusyRun(), the source is the thread pool
     implementation, not client code.  Any exceptions thrown by client code are
     caught inside DoBusyRun(). */
  try {
    WakeupWait.lock();  // sleep in idle state

    if (!TerminateRequested) {
      DoBusyRun();
    }
  } catch (const std::exception &x) {
    std::string msg("Fatal exception in thread pool worker: ");
    msg += x.what();
    MyPool.HandleFatalError(msg.c_str());
  } catch (...) {
    MyPool.HandleFatalError("Fatal unknown exception in thread pool worker");
  }
}

TManagedThreadPoolBase::TManagedThreadPoolBase(
    const TFatalErrorHandler &fatal_error_handler,
    const TManagedThreadPoolConfig &cfg)
    : FatalErrorHandler(fatal_error_handler),
      Config(cfg),
      Manager(*this) {
}

TManagedThreadPoolBase::TManagedThreadPoolBase(
    TFatalErrorHandler &&fatal_error_handler,
    const TManagedThreadPoolConfig &cfg)
    : FatalErrorHandler(std::move(fatal_error_handler)),
      Config(cfg),
      Manager(*this) {
}

TManagedThreadPoolBase::TManagedThreadPoolBase(
    const TFatalErrorHandler &fatal_error_handler)
    : TManagedThreadPoolBase(fatal_error_handler, TManagedThreadPoolConfig()) {
}

TManagedThreadPoolBase::TManagedThreadPoolBase(
    TFatalErrorHandler &&fatal_error_handler)
    : TManagedThreadPoolBase(std::move(fatal_error_handler),
          TManagedThreadPoolConfig()) {
}

TManagedThreadPoolBase::TWorkerBase *
TManagedThreadPoolBase::GetAvailableWorker() {
  assert(this);

  {
    std::lock_guard<std::mutex> lock(PoolLock);
    size_t max_pool_size = Config.GetMaxPoolSize();

    if (max_pool_size && (LiveWorkerCount >= max_pool_size)) {
      ++Stats.PoolMaxSizeEnforceCount;
      return nullptr;
    }

    if (!PoolIsReady) {
      /* pool is shutting down or not yet started */
      throw TPoolNotReady();
    }

    std::list<TWorkerBasePtr> ready_worker = IdleList.RemoveOneNewest();

    if (!ready_worker.empty()) {
      /* We got a worker from the idle list.  Put it on the busy list and
         provide it to the client. */
      ++Stats.PoolHitCount;
      return AddToBusyList(ready_worker);
    }
  }

  /* The idle list was empty so we must create a new worker.  The worker
     initially contains no thread.  The thread is created and immediately
     enters the busy state when the client launches the worker. */
  std::list<TWorkerBasePtr> new_worker(1);
  TWorkerBasePtr &wp = new_worker.front();
  wp.reset(CreateWorker(false));

  /* When the worker finishes working, this ensures that its 'WorkerThread'
     member has been assigned before the worker places itself on the idle list,
     thus avoiding a race condition. */
  wp->SetWaitAfterDoWork();

  /* Even though the worker doesn't yet contain a thread, we still count it as
     "live" and add it to the busy list.  In the case where the pool starts
     shutting down before the client either launches the worker or releases it
     without launching, this forces the manager to wait for the client to
     commit to one action or the other before finishing the shutdown. */
  std::lock_guard<std::mutex> lock(PoolLock);
  ++LiveWorkerCount;
  ++Stats.PoolMissCount;
  ++Stats.CreateWorkerCount;
  return AddToBusyList(new_worker);
}

TManagedThreadPoolBase::TManager::TManager(TManagedThreadPoolBase &my_pool)
    : MyPool(my_pool) {
}

TManagedThreadPoolBase::TManager::~TManager() {
  ShutdownOnDestroy();
}

void TManagedThreadPoolBase::TManager::Run() {
  assert(this);

  try {
    DoRun();
  } catch (const std::exception &x) {
    std::string msg("Fatal exception in thread pool manager: ");
    msg += x.what();
    MyPool.HandleFatalError(msg.c_str());
  } catch (...) {
    MyPool.HandleFatalError("Fatal unknown exception in thread pool manager");
  }
}

uint64_t TManagedThreadPoolBase::TManager::HandleReconfig(
    uint64_t old_prune_at, uint64_t now) {
  assert(this);
  MyPool.ReconfigSem.Pop();
  bool reset_segments = false;
  size_t old_prune_quantum_ms = Config.GetPruneQuantumMs();

  {
    std::lock_guard<std::mutex> lock(MyPool.PoolLock);
    ++MyPool.Stats.ReconfigCount;
    MyPool.ReconfigPending = false;

    if ((MyPool.Config.GetPruneQuantumMs() != Config.GetPruneQuantumMs()) ||
        (MyPool.Config.GetPruneQuantumCount() !=
            Config.GetPruneQuantumCount())) {
      MyPool.IdleList.ResetSegments();
      reset_segments = true;
    }

    /* update private copy of pool config */
    Config = MyPool.Config;
  }

  if (reset_segments) {
    return now + Config.GetPruneQuantumMs();
  }

  return (Config.GetPruneQuantumMs() < old_prune_quantum_ms) ?
      old_prune_at - (old_prune_quantum_ms - Config.GetPruneQuantumMs()) :
      old_prune_at + (Config.GetPruneQuantumMs() - old_prune_quantum_ms);
}

size_t
TManagedThreadPoolBase::TManager::GetMaxThreadsToPrune() const noexcept {
  assert(this);
  assert(MyPool.IdleList.Size() <= MyPool.LiveWorkerCount);

  if (MyPool.LiveWorkerCount <= Config.GetMinPoolSize()) {
    /* Prevent integer wraparound in calculation of 'max1' below. */
    return 0;
  }

  /* Compute max prune count imposed by Config.GetMinIdleFraction().
     Define the following:

       i = initial idle list size
       b = initial busy list size (i.e. total thread count - idle list size)
       F = min idle fraction (from config)
       x = The # of threads one would have to prune to make the final idle
           fraction exactly equal F.  In general, this will not be an integer.

     Then we have the following:

       (i - x) / (i + b - x) = F / 1000

     Solving for x, we get the following:

       x = (((1000 - F) * i) - (F * b)) / (1000 - F)

     Now define the following:

       v = (1000 - F) * i
       w = F * b

     Then the above solution can be rewritten as:

       x = (v - w) / (1000 - F)

     Below we compute x (rounded down since we have to prune an integer number
     of threads), while handling the following special cases:

       case 1: b = 0
         Since all threads are idle, we can prune all of them while satisfying
         F.

       case 2: F = 1000 and b > 0
         Here we can't prune any threads.  This case must be handled specially
         to prevent division by 0 in the above formula.

       case 3: w > v
         In this case we would have to prune a negative number of threads to
         satisfy F exactly.  In other words, we can't prune any threads.
   */

  size_t max2 = 0;  // x above

  if (MyPool.IdleList.Size() == MyPool.LiveWorkerCount) {
    max2 = MyPool.IdleList.Size();  // case 1
  } else {
    size_t d = 1000 - Config.GetMinIdleFraction();
    size_t v = d * MyPool.IdleList.Size();
    size_t w = Config.GetMinIdleFraction() *
        (MyPool.LiveWorkerCount - MyPool.IdleList.Size());

    if (w >= v) {
      /* This handles case 3.  It also handles case 2: If F is 1000 then v is
         0.  Since case 1 didn't apply, w > 0, so we return here. */
      return 0;
    }

    /* Integer division rounds our result down, which is what we want. */
    max2 = (v - w) / d;
  }

  /* Compute max prune count imposed by Config.GetMinPoolSize(). */
  size_t max1 = MyPool.LiveWorkerCount - Config.GetMinPoolSize();

  /* Compute max prune count imposed by Config.GetMaxPruneFraction(). */
  size_t n = MyPool.LiveWorkerCount * Config.GetMaxPruneFraction() / 1000;
  size_t max3 = Config.GetMaxPruneFraction() ? std::max<size_t>(1, n) : 0;

  /* To satisfy all 3 criteria, we must return the minimum of the 3 max prune
     counts. */
  return std::min(std::min(max1, max2), max3);
}

void TManagedThreadPoolBase::TManager::PruneThreadPool() {
  assert(this);
  std::list<TWorkerBasePtr> pruned;

  {
    std::lock_guard<std::mutex> lock(MyPool.PoolLock);
    ++MyPool.Stats.PruneOpCount;

    if (MyPool.IdleList.SegmentCount() < Config.GetPruneQuantumCount()) {
      /* Add empty segment to front of idle list, shifting older segments back
         one position.  The oldest segment isn't yet old enough to prune. */
      MyPool.IdleList.AddNewSegment();
      return;
    }

    /* Try to prune as many threads as possible from oldest segment, according
       to pool config. */
    assert(MyPool.IdleList.SegmentCount() == Config.GetPruneQuantumCount());
    size_t initial_idle_count = MyPool.IdleList.Size();
    pruned = MyPool.IdleList.RemoveOldest(GetMaxThreadsToPrune());
    assert(MyPool.IdleList.Size() <= initial_idle_count);
    assert(MyPool.LiveWorkerCount >= initial_idle_count);
    size_t prune_count = initial_idle_count - MyPool.IdleList.Size();
    MyPool.LiveWorkerCount -= prune_count;
    MyPool.IdleList.RecycleOldestSegment();
    MyPool.Stats.PrunedThreadCount += prune_count;

    if ((MyPool.Stats.PruneOpCount == 1) ||
        (prune_count < MyPool.Stats.MinPrunedByOp)) {
      MyPool.Stats.MinPrunedByOp = prune_count;
    }

    if (prune_count > MyPool.Stats.MaxPrunedByOp) {
      MyPool.Stats.MaxPrunedByOp = prune_count;
    }
  }

  /* Tell pruned workers to terminate. */
  for (auto &worker : pruned) {
    worker->Terminate();
  }

  /* Wait for termination of pruned workers to finish. */
  for (auto &worker : pruned) {
    worker->Join();
  }
}

void TManagedThreadPoolBase::TManager::HandleShutdownRequest() {
  assert(this);
  std::list<TWorkerBasePtr> idle_workers, dead_workers;
  bool wait_for_workers = false;

  {
    /* Remove all idle and terminated workers from pool.  If any busy workers
       remain, we will wait for them to terminate.  Workers performing long-
       running tasks should monitor the pool's shutdown request semaphore, and
       terminate quickly once shutdown has started. */
    std::lock_guard<std::mutex> lock(MyPool.PoolLock);
    assert(MyPool.IdleList.Size() <= MyPool.LiveWorkerCount);
    MyPool.LiveWorkerCount -= MyPool.IdleList.Size();
    idle_workers = MyPool.IdleList.EmptyAllAndResetSegments();
    dead_workers = std::move(MyPool.JoinList);
    wait_for_workers = (MyPool.LiveWorkerCount != 0);
    MyPool.PoolIsReady = false;
  }

  /* Wake up idle workers and tell them to terminate. */
  for (auto &worker : idle_workers) {
    worker->Terminate();
  }

  dead_workers.splice(dead_workers.end(), idle_workers);

  for (auto &worker : dead_workers) {
    worker->Join();
  }

  dead_workers.clear();

  if (wait_for_workers) {
    /* Wait for last busy worker to notify us that it is terminating. */
    MyPool.AllWorkersFinished.Pop();
  }

  assert(MyPool.IdleList.Empty());
  assert(MyPool.BusyList.empty());
  assert(MyPool.LiveWorkerCount == 0);

  /* At this point all workers have terminated, so we shouldn't need to acquire
     MyPool.PoolLock.  Acquire it anyway, just in case a possibly buggy client
     tries to access the pool while we are still shutting down.  Defensive
     programming doesn't cost us anything here. */
  {
    std::lock_guard<std::mutex> lock(MyPool.PoolLock);
    dead_workers = std::move(MyPool.JoinList);
  }

  for (auto &worker : dead_workers) {
    worker->Join();
  }
}

void TManagedThreadPoolBase::TManager::DoRun() {
  assert(this);

  {
    /* make private copy of pool config */
    std::lock_guard<std::mutex> lock(MyPool.PoolLock);
    Config = MyPool.Config;
  }

  std::array<struct pollfd, 2> events;
  struct pollfd &reconfig_event = events[0];
  struct pollfd &shutdown_request_event = events[1];
  reconfig_event.fd = MyPool.ReconfigSem.GetFd();
  reconfig_event.events = POLLIN;
  shutdown_request_event.fd = GetShutdownRequestFd();
  shutdown_request_event.events = POLLIN;
  uint64_t now = GetMonotonicRawMilliseconds();
  uint64_t prune_at = now + Config.GetPruneQuantumMs();

  for (; ; ) {
    for (auto &item : events) {
      item.revents = 0;
    }

    int timeout = Config.GetMaxPruneFraction() ?
        ((prune_at < now) ? 0 : static_cast<int>(prune_at - now)) : -1;
    IfLt0(poll(&events[0], events.size(), timeout));

    if (shutdown_request_event.revents) {
      break;
    }

    now = GetMonotonicRawMilliseconds();

    if (reconfig_event.revents) {
      /* Update pool config, as requested by client. */
      prune_at = HandleReconfig(prune_at, now);
    }

    if (Config.GetMaxPruneFraction() && (now >= prune_at)) {
      /* Terminate threads that have been idle for too long, according to pool
         config. */
      PruneThreadPool();
      prune_at += Config.GetPruneQuantumMs();
      now = GetMonotonicRawMilliseconds();
    }
  }

  /* We got a shutdown request.  Before terminating, clean up all remaining
     workers. */
  HandleShutdownRequest();
}

TManagedThreadPoolBase::TWorkerBase *
TManagedThreadPoolBase::AddToBusyList(
    std::list<TWorkerBasePtr> &ready_worker) noexcept {
  assert(this);
  assert(ready_worker.size() == 1);
  BusyList.splice(BusyList.begin(), ready_worker);
  TWorkerBase &worker = *BusyList.front();
  assert(worker.BusyListPos == BusyList.end());
  worker.BusyListPos = BusyList.begin();
  return &worker;
}
