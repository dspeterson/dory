/* <thread/managed_thread_pool.test.cc>
 
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
 
   Unit test for <thread/managed_thread_pool.h>.
 */
  
#include <thread/managed_thread_pool.h>
#include <thread/managed_thread_pool_config.h>
#include <thread/managed_thread_pool_stats.h>

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <system_error>

#include <unistd.h>

#include <base/time_util.h>
#include <base/tmp_file.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace TestUtil;
using namespace Thread;

using TManagedThreadStdFnPool = TManagedThreadPool<std::function<void()>>;

class TStressTest2WorkFn;

using TManagedThreadFnObjPool = TManagedThreadPool<TStressTest2WorkFn>;

typedef void (*TThreadFnPtr)();

using TManagedThreadFnPtrPool = TManagedThreadPool<TThreadFnPtr>;

static const char EXCEPTION_BLURB[] = "nasty stuff";

class TSimpleWorkFn {
  public:
  static const char ERROR_BLURB[];

  enum class TThrowAction {
    ThrowNothing,
    ThrowStdException,
    ThrowCrap
  };  // TThrowAction

  explicit TSimpleWorkFn(std::atomic<size_t> &counter)
      : Counter(counter),
        ThrowAction(TThrowAction::ThrowNothing) {
  }

  void SetThrowAction(TThrowAction action) {
    assert(this);
    ThrowAction = action;
  }

  void operator()() {
    assert(this);
    ++Counter;

    switch (ThrowAction) {
      case TThrowAction::ThrowNothing:
        break;
      case TThrowAction::ThrowStdException:
        throw std::runtime_error(ERROR_BLURB);
      case TThrowAction::ThrowCrap:
        throw EXCEPTION_BLURB;
    }
  }

  private:
  std::atomic<size_t> &Counter;

  TThrowAction ThrowAction;
};  // TSimpleWorkFn

const char TSimpleWorkFn::ERROR_BLURB[] = "no smoking in powder magazine";

namespace {

  std::atomic<bool> CalledThreadWorkFn(false);

  void ThreadWorkFn() {
    CalledThreadWorkFn = true;
  }

  void HandleOutOfMemory() {
    std::cerr << "Failed to create worker thread due to not enough memory.  "
              << "Try running the stress tests on a system with more memory, "
              << "or modify them to create fewer worker threads."
        << std::endl;
    _exit(1);
  }

}  // namespace

class TStressTest1WorkFn {
  public:
  TStressTest1WorkFn(TManagedThreadStdFnPool &pool, uint64_t start_time,
      std::atomic<size_t> &counter, std::atomic<size_t> &working_count,
      size_t initial_thread_count)
      : Pool(pool),
        StartTime(start_time),
        Counter(counter),
        WorkingCount(working_count),
        InitialThreadCount(initial_thread_count) {
  }

  void operator()() {
    assert(this);
    size_t num_working = WorkingCount.load();
    ++Counter;
    uint64_t now = GetMonotonicRawMilliseconds();
    uint64_t elapsed = now - StartTime;
    size_t launch_count = 0;

    if (elapsed < 5000) {
      launch_count = 1;
    } else if (elapsed < 10000) {
      launch_count = (num_working < (InitialThreadCount * 5 / 4)) ? 2 : 1;
    } else if (elapsed < 15000) {
      launch_count = 1;
    }

    switch (launch_count) {
      case 0:
        --WorkingCount;
        break;
      case 1:
        break;
      default:
        WorkingCount += launch_count - 1;
        break;
    }

    size_t launched = 0;

    while (launched < launch_count) {
      TManagedThreadStdFnPool::TReadyWorker w = Pool.GetReadyWorker();

      if (!w.IsLaunchable()) {
        /* Unable to allocate worker because pool is at its configured max
           size. */
        break;
      }

      auto &fn = w.GetWorkFn();
      ASSERT_TRUE(fn == nullptr);
      fn = *this;
      bool out_of_memory = false;

      /* If we run out of memory while trying to create a new worker thread,
         report the problem and shut down immediately.  We want it to be
         obvious that the test failed due to not enough memory rather than
         buggy code. */
      try {
        w.Launch();
        ++launched;
      } catch (const std::bad_alloc &) {
        out_of_memory = true;
      } catch (const std::system_error &x) {
        int errno_value = x.code().value();

        if ((errno_value != ENOMEM) && (errno_value != EAGAIN)) {
          throw;
        }

        out_of_memory = true;
      }

      if (out_of_memory) {
        HandleOutOfMemory();
      }
    }

    if (launched < launch_count) {
      WorkingCount -= launch_count - launched;
    }
  }

  private:
  TManagedThreadStdFnPool &Pool;

  uint64_t StartTime;

  std::atomic<size_t> &Counter;

  std::atomic<size_t> &WorkingCount;

  size_t InitialThreadCount;
};  // TStressTest1WorkFn

class TStressTest2WorkFn {
  public:
  explicit TStressTest2WorkFn(nullptr_t) {
  }

  TStressTest2WorkFn &operator=(nullptr_t) {
    assert(this);
    Clear();
    return *this;
  }

  void SetPool(TManagedThreadFnObjPool &pool) {
    assert(this);
    Pool = &pool;
  }

  void SetCounter(std::atomic<size_t> &counter) {
    assert(this);
    Counter = &counter;
  }

  void SetWorkingCount(std::atomic<size_t> &working_count) {
    assert(this);
    WorkingCount = &working_count;
  }

  void SetRemainingCount(size_t count) {
    assert(this);
    RemainingCount = count;
  }

  bool IsClear() const {
    assert(this);
    return (Pool == nullptr) && (Counter == nullptr) &&
        (WorkingCount == nullptr) && (RemainingCount == 1);
  }

  void operator()() {
    assert(this);
    ++*Counter;

    if (--RemainingCount == 0) {
      --*WorkingCount;
      return;
    }

    TManagedThreadFnObjPool::TReadyWorker w = Pool->GetReadyWorker();
    auto &fn = w.GetWorkFn();
    ASSERT_TRUE(fn.IsClear());
    fn.SetPool(*Pool);
    fn.SetCounter(*Counter);
    fn.SetWorkingCount(*WorkingCount);
    fn.SetRemainingCount(RemainingCount);
    bool out_of_memory = false;

    /* If we run out of memory while trying to create a new worker thread,
       report the problem and shut down immediately.  We want it to be obvious
       that the test failed due to not enough memory rather than buggy code. */
    try {
      w.Launch();
    } catch (const std::bad_alloc &) {
      out_of_memory = true;
    } catch (const std::system_error &x) {
      int errno_value = x.code().value();

      if ((errno_value != ENOMEM) && (errno_value != EAGAIN)) {
        throw;
      }

      out_of_memory = true;
    }

    if (out_of_memory) {
      HandleOutOfMemory();
    }

    if ((RemainingCount % 10) == 0) {
      /* Exercise RAII behavior where TReadyWorker destructor returns
         unlaunched worker to idle list. */
      w = Pool->GetReadyWorker();
      ASSERT_TRUE(w.IsLaunchable());
      ASSERT_TRUE(w.GetWorkFn().IsClear());
    }
  }

  private:
  TStressTest2WorkFn() = default;

  void Clear() {
    assert(this);
    *this = TStressTest2WorkFn();
  }

  TManagedThreadFnObjPool *Pool = nullptr;

  std::atomic<size_t> *Counter = nullptr;

  std::atomic<size_t> *WorkingCount = nullptr;

  size_t RemainingCount = 1;
};  // TStressTest2WorkFn

namespace {

  void PrintStats(const TManagedThreadPoolStats &stats) {
    std::cout
        << "--- summary pool stats ---------------------------" << std::endl
        << "SetConfigCount: " << stats.SetConfigCount << std::endl
        << "ReconfigCount: " << stats.ReconfigCount << std::endl
        << "PruneOpCount: " << stats.PruneOpCount << std::endl
        << "PrunedThreadCount: " << stats.PrunedThreadCount << std::endl
        << "MinPrunedByOp: " << stats.MinPrunedByOp << std::endl
        << "MaxPrunedByOp: " << stats.MaxPrunedByOp << std::endl
        << "PoolHitCount: " << stats.PoolHitCount << std::endl
        << "PoolMissCount: " << stats.PoolMissCount << std::endl
        << "PoolMaxSizeEnforceCount: " << stats.PoolMaxSizeEnforceCount
        << std::endl
        << "CreateWorkerCount: " << stats.CreateWorkerCount << std::endl
        << "PutBackCount: " << stats.PutBackCount << std::endl
        << "FinishWorkCount: " << stats.FinishWorkCount << std::endl
        << "QueueErrorCount: " << stats.QueueErrorCount << std::endl
        << "NotifyErrorCount: " << stats.NotifyErrorCount << std::endl
        << "LiveWorkerCount: " << stats.LiveWorkerCount << std::endl
        << "IdleWorkerCount: " << stats.IdleWorkerCount << std::endl
        << "--------------------------------------------------" << std::endl;
  }

  /* The fixture for testing class TManagedThreadPool. */
  class TManagedThreadPoolTest : public ::testing::Test {
    protected:
    static void HandleFatalError(const char *msg) {
      std::cerr << "Fatal thread pool error: " << msg << std::endl;
      std::exit(EXIT_FAILURE);
    }

    TManagedThreadPoolTest() = default;

    ~TManagedThreadPoolTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TManagedThreadPoolTest

  TEST_F(TManagedThreadPoolTest, ReadyWorkerTest) {
    TManagedThreadStdFnPool pool(TManagedThreadPoolTest::HandleFatalError);
    ASSERT_FALSE(pool.IsStarted());
    pool.Start();
    ASSERT_TRUE(pool.IsStarted());
    pool.RequestShutdown();
    ASSERT_TRUE(pool.IsStarted());
    pool.WaitForShutdown();
    ASSERT_FALSE(pool.IsStarted());

    pool.Start();
    ASSERT_TRUE(pool.IsStarted());
    TManagedThreadPoolStats stats;

    {
      TManagedThreadStdFnPool::TReadyWorker w1 = pool.GetReadyWorker();
      ASSERT_TRUE(w1.IsLaunchable());
      ASSERT_TRUE(w1.IsNew());
      ASSERT_EQ(&w1.GetPool(), &pool);
      ASSERT_EQ(w1.GetWorkFn(), nullptr);
      stats = pool.GetStats();
      ASSERT_EQ(stats.SetConfigCount, 0U);
      ASSERT_EQ(stats.ReconfigCount, 0U);
      ASSERT_EQ(stats.PruneOpCount, 0U);
      ASSERT_EQ(stats.PrunedThreadCount, 0U);
      ASSERT_EQ(stats.MinPrunedByOp, 0U);
      ASSERT_EQ(stats.MaxPrunedByOp, 0U);
      ASSERT_EQ(stats.PoolHitCount, 0U);
      ASSERT_EQ(stats.PoolMissCount, 1U);
      ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
      ASSERT_EQ(stats.CreateWorkerCount, 1U);
      ASSERT_EQ(stats.PutBackCount, 0U);
      ASSERT_EQ(stats.FinishWorkCount, 0U);
      ASSERT_EQ(stats.QueueErrorCount, 0U);
      ASSERT_EQ(stats.NotifyErrorCount, 0U);
    }

    stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 0U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 1U);
    ASSERT_EQ(stats.PutBackCount, 1U);
    ASSERT_EQ(stats.FinishWorkCount, 0U);
    pool.RequestShutdown();
    ASSERT_TRUE(pool.IsStarted());
    pool.WaitForShutdown();
    ASSERT_FALSE(pool.IsStarted());
  }

  TEST_F(TManagedThreadPoolTest, SimplePoolTest) {
    std::atomic<size_t> counter(0);
    TSimpleWorkFn work_fn(counter);
    TManagedThreadPoolConfig config;
    config.SetMaxPruneFraction(0);  // disable pruning
    TManagedThreadStdFnPool pool(TManagedThreadPoolTest::HandleFatalError,
        config);
    ASSERT_FALSE(pool.IsStarted());
    pool.Start();
    ASSERT_TRUE(pool.IsStarted());
    TManagedThreadPoolStats stats;

    {
      TManagedThreadStdFnPool::TReadyWorker w1 = pool.GetReadyWorker();
      ASSERT_TRUE(w1.IsLaunchable());
      ASSERT_TRUE(w1.IsNew());
      ASSERT_EQ(&w1.GetPool(), &pool);
      ASSERT_EQ(w1.GetWorkFn(), nullptr);
      w1.GetWorkFn() = work_fn;
      TManagedThreadStdFnPool::TReadyWorker w2;
      ASSERT_FALSE(w2.IsLaunchable());
      w2 = std::move(w1);
      ASSERT_TRUE(w2.IsLaunchable());
      ASSERT_TRUE(w2.IsNew());
      ASSERT_FALSE(w1.IsLaunchable());
      TManagedThreadStdFnPool::TReadyWorker w3(std::move(w2));
      ASSERT_TRUE(w3.IsLaunchable());
      ASSERT_TRUE(w3.IsNew());
      ASSERT_FALSE(w2.IsLaunchable());
      w3.Swap(w2);
      ASSERT_FALSE(w3.IsLaunchable());
      ASSERT_TRUE(w2.IsLaunchable());
      ASSERT_TRUE(w2.IsNew());
      stats = pool.GetStats();
      ASSERT_EQ(stats.PoolHitCount, 0U);
      ASSERT_EQ(stats.PoolMissCount, 1U);
      ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
      ASSERT_EQ(stats.CreateWorkerCount, 1U);
      ASSERT_EQ(stats.PutBackCount, 0U);
      ASSERT_EQ(stats.FinishWorkCount, 0U);
      std::thread::id worker_id = w2.Launch();
      ASSERT_NE(worker_id, std::thread::id());
      ASSERT_FALSE(w2.IsLaunchable());
    }

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount == 0);
        ++i) {
      sleep(1);
    }

    ASSERT_TRUE(counter.load() == 1U);
    stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 0U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 1U);
    ASSERT_EQ(stats.PutBackCount, 0U);
    ASSERT_EQ(stats.FinishWorkCount, 1U);

    {
      TManagedThreadStdFnPool::TReadyWorker w4 = pool.GetReadyWorker();
      ASSERT_TRUE(w4.IsLaunchable());
      ASSERT_FALSE(w4.IsNew());
      ASSERT_EQ(&w4.GetPool(), &pool);
      ASSERT_EQ(w4.GetWorkFn(), nullptr);
      stats = pool.GetStats();
      ASSERT_EQ(stats.PoolHitCount, 1U);
      ASSERT_EQ(stats.PoolMissCount, 1U);
      ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
      ASSERT_EQ(stats.CreateWorkerCount, 1U);
      ASSERT_EQ(stats.PutBackCount, 0U);
      ASSERT_EQ(stats.FinishWorkCount, 1U);
      w4.GetWorkFn() = work_fn;
    }

    stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 1U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 1U);
    ASSERT_EQ(stats.PutBackCount, 1U);
    ASSERT_EQ(stats.FinishWorkCount, 1U);

    {
      TManagedThreadStdFnPool::TReadyWorker w5 = pool.GetReadyWorker();
      ASSERT_TRUE(w5.IsLaunchable());
      ASSERT_FALSE(w5.IsNew());
      ASSERT_EQ(&w5.GetPool(), &pool);
      ASSERT_EQ(w5.GetWorkFn(), nullptr);
      stats = pool.GetStats();
      ASSERT_EQ(stats.PoolHitCount, 2U);
      ASSERT_EQ(stats.PoolMissCount, 1U);
      ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
      ASSERT_EQ(stats.CreateWorkerCount, 1U);
      ASSERT_EQ(stats.PutBackCount, 1U);
      ASSERT_EQ(stats.FinishWorkCount, 1U);
      w5.GetWorkFn() = work_fn;
      std::thread::id worker_id = w5.Launch();
      ASSERT_NE(worker_id, std::thread::id());
      ASSERT_FALSE(w5.IsLaunchable());
    }

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount == 1);
        ++i) {
      sleep(1);
    }

    ASSERT_TRUE(counter.load() == 2U);
    stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 2U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 1U);
    ASSERT_EQ(stats.PutBackCount, 1U);
    ASSERT_EQ(stats.FinishWorkCount, 2U);
    pool.RequestShutdown();
    ASSERT_TRUE(pool.IsStarted());
    pool.WaitForShutdown();
    ASSERT_FALSE(pool.IsStarted());
  }

  TEST_F(TManagedThreadPoolTest, FnPtrTest) {
    TManagedThreadPoolConfig config;
    config.SetMaxPruneFraction(0);  // disable pruning
    TManagedThreadFnPtrPool pool(TManagedThreadPoolTest::HandleFatalError,
        config);
    pool.Start();
    CalledThreadWorkFn = false;
    TManagedThreadFnPtrPool::TReadyWorker w = pool.GetReadyWorker();

    {
      auto &fn = w.GetWorkFn();
      ASSERT_TRUE(fn == nullptr);
      fn = ThreadWorkFn;
    }

    w.Launch();

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount < 1);
        ++i) {
      sleep(1);
    }

    ASSERT_TRUE(CalledThreadWorkFn.load());
    CalledThreadWorkFn = false;
    w = pool.GetReadyWorker();

    {
      auto &fn = w.GetWorkFn();
      ASSERT_TRUE(fn == nullptr);
      fn = ThreadWorkFn;
    }

    w.Launch();

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount < 2);
        ++i) {
      sleep(1);
    }

    ASSERT_TRUE(CalledThreadWorkFn.load());
    pool.RequestShutdown();
    pool.WaitForShutdown();

    TManagedThreadPoolStats stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 1U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 1U);
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
    ASSERT_FALSE(pool.GetErrorPendingFd().IsReadableIntr());
  }

  TEST_F(TManagedThreadPoolTest, ExceptionTest) {
    std::atomic<size_t> counter(0);
    TSimpleWorkFn work_fn(counter);
    TManagedThreadPoolConfig config;
    config.SetMinPoolSize(2);
    config.SetMaxPruneFraction(0);  // disable pruning
    TManagedThreadStdFnPool pool(TManagedThreadPoolTest::HandleFatalError,
        config);
    const TFd &error_fd = pool.GetErrorPendingFd();
    pool.Start();
    TManagedThreadPoolStats stats;
    TManagedThreadStdFnPool::TReadyWorker w1 = pool.GetReadyWorker();
    TManagedThreadStdFnPool::TReadyWorker w2 = pool.GetReadyWorker();
    TManagedThreadStdFnPool::TReadyWorker w3 = pool.GetReadyWorker();
    work_fn.SetThrowAction(TSimpleWorkFn::TThrowAction::ThrowStdException);
    w1.GetWorkFn() = work_fn;
    work_fn.SetThrowAction(TSimpleWorkFn::TThrowAction::ThrowNothing);
    w2.GetWorkFn() = work_fn;
    work_fn.SetThrowAction(TSimpleWorkFn::TThrowAction::ThrowCrap);
    w3.GetWorkFn() = work_fn;

    std::thread::id w1_id = w1.Launch();
    ASSERT_TRUE(error_fd.IsReadableIntr(10000));
    std::list<TManagedThreadPoolBase::TWorkerError> error_list =
        pool.GetAllPendingErrors();
    ASSERT_FALSE(error_fd.IsReadableIntr());
    ASSERT_EQ(error_list.size(), 1U);
    TManagedThreadPoolBase::TWorkerError error = error_list.front();
    error_list.pop_front();
    ASSERT_EQ(error.ThreadId, w1_id);

    try {
      std::rethrow_exception(error.ThrownException);
    } catch (const std::runtime_error &x) {
      ASSERT_EQ(std::strcmp(x.what(), TSimpleWorkFn::ERROR_BLURB), 0);
    }

    error_list = pool.GetAllPendingErrors();
    ASSERT_TRUE(error_list.empty());

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount == 0);
        ++i) {
      sleep(1);
    }

    ASSERT_EQ(pool.GetStats().FinishWorkCount, 1U);
    ASSERT_EQ(counter.load(), 1U);

    w2.Launch();
    std::thread::id w3_id = w3.Launch();

    ASSERT_TRUE(error_fd.IsReadableIntr(10000));

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount < 3);
        ++i) {
      sleep(1);
    }

    ASSERT_EQ(pool.GetStats().FinishWorkCount, 3U);
    ASSERT_EQ(counter.load(), 3U);
    ASSERT_TRUE(error_fd.IsReadableIntr());
    error_list = pool.GetAllPendingErrors();
    ASSERT_FALSE(error_fd.IsReadableIntr());
    ASSERT_EQ(error_list.size(), 1U);
    error = error_list.front();
    error_list.pop_front();
    ASSERT_EQ(error.ThreadId, w3_id);

    try {
      std::rethrow_exception(error.ThrownException);
    } catch (const char *x) {
      ASSERT_EQ(std::strcmp(x, EXCEPTION_BLURB), 0);
    }

    error_list = pool.GetAllPendingErrors();
    ASSERT_TRUE(error_list.empty());
    ASSERT_EQ(counter.load(), 3U);

    stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 2U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 3U);
    ASSERT_EQ(stats.QueueErrorCount, 2U);
    ASSERT_EQ(stats.NotifyErrorCount, 2U);
    ASSERT_EQ(stats.LiveWorkerCount, 3U);

    work_fn.SetThrowAction(TSimpleWorkFn::TThrowAction::ThrowNothing);
    w1 = pool.GetReadyWorker();
    w2 = pool.GetReadyWorker();
    w3 = pool.GetReadyWorker();
    w1.GetWorkFn() = work_fn;
    w2.GetWorkFn() = work_fn;
    w3.GetWorkFn() = work_fn;
    w1.Launch();
    w2.Launch();
    w3.Launch();

    for (size_t i = 0;
        (i < 30) && (pool.GetStats().FinishWorkCount < 6);
        ++i) {
      sleep(1);
    }

    ASSERT_EQ(counter.load(), 6U);
    ASSERT_FALSE(error_fd.IsReadableIntr());
    error_list = pool.GetAllPendingErrors();
    ASSERT_TRUE(error_list.empty());

    pool.RequestShutdown();
    pool.WaitForShutdown();

    stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 5U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 0U);
    ASSERT_EQ(stats.CreateWorkerCount, 3U);
    ASSERT_EQ(stats.QueueErrorCount, 2U);
    ASSERT_EQ(stats.NotifyErrorCount, 2U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
  }

  TEST_F(TManagedThreadPoolTest, SizeLimitTest) {
    TManagedThreadPoolConfig config;
    config.SetMaxPoolSize(1);
    TManagedThreadStdFnPool pool(TManagedThreadPoolTest::HandleFatalError,
        config);
    pool.Start();
    TManagedThreadStdFnPool::TReadyWorker w1 = pool.GetReadyWorker();
    TManagedThreadStdFnPool::TReadyWorker w2 = pool.GetReadyWorker();
    ASSERT_TRUE(w1.IsLaunchable());
    ASSERT_FALSE(w2.IsLaunchable());
    w1.PutBack();
    ASSERT_FALSE(w1.IsLaunchable());

    pool.RequestShutdown();
    pool.WaitForShutdown();

    TManagedThreadPoolStats stats = pool.GetStats();
    ASSERT_EQ(stats.PoolHitCount, 0U);
    ASSERT_EQ(stats.PoolMissCount, 1U);
    ASSERT_EQ(stats.PoolMaxSizeEnforceCount, 1U);
    ASSERT_EQ(stats.CreateWorkerCount, 1U);
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
    ASSERT_FALSE(pool.GetErrorPendingFd().IsReadableIntr());
  }

  void CheckForWorkerThreadErrors(
      std::list<TManagedThreadPoolBase::TWorkerError> &errors) {
    if (!errors.empty()) {
      for (auto &e : errors) {
        try {
          std::rethrow_exception(e.ThrownException);
        } catch (const std::exception &x) {
          std::cerr << "Worker thread threw exception: " << x.what()
              << std::endl;
        } catch (...) {
          std::cerr << "Worker thread threw unknown exception" << std::endl;
        }
      }

      _exit(1);
    }
  }

  TEST_F(TManagedThreadPoolTest, StressTest1) {
    std::cout
        << "Running stress test 1.  This should take about 15-30 seconds."
        << std::endl;
    const size_t initial_thread_count = 60;
    std::atomic<size_t> counter(0);
    std::atomic<size_t> working_count(initial_thread_count);
    TManagedThreadPoolConfig config;

    /* Put a hard upper bound on the pool size.  This will reduce our risk of
       running out of memory on a test machine without much memory. */
    config.SetMaxPoolSize(250);

    config.SetPruneQuantumMs(300);
    config.SetPruneQuantumCount(5);
    TManagedThreadStdFnPool pool(TManagedThreadPoolTest::HandleFatalError,
        config);
    pool.Start();
    std::vector<TManagedThreadStdFnPool::TReadyWorker>
        initial_workers(initial_thread_count);
    uint64_t start_time = GetMonotonicRawMilliseconds();
    TStressTest1WorkFn work_fn(pool, start_time, counter, working_count,
        initial_thread_count);

    for (auto &w : initial_workers) {
      w = pool.GetReadyWorker();
      auto &fn = w.GetWorkFn();
      ASSERT_TRUE(fn == nullptr);
      fn = work_fn;
    }

    for (auto &w : initial_workers) {
      w.Launch();
    }

    std::list<TManagedThreadPoolBase::TWorkerError> errors;

    /* Hopefully 600 seconds will be long enough for the test to finish, even
       on a slow machine. */
    for (size_t i = 0; (i < 600); ++i) {
      TManagedThreadPoolStats stats = pool.GetStats();

      if ((working_count.load() == 0) && (stats.IdleWorkerCount == 0) &&
          (stats.LiveWorkerCount == 0) &&
          (stats.FinishWorkCount == counter.load())) {
        break;
      }

      errors = pool.GetAllPendingErrors();
      CheckForWorkerThreadErrors(errors);
      sleep(1);
    }

    TManagedThreadPoolStats stats = pool.GetStats();
    std::cout << "final count: " << counter.load() << std::endl;
    PrintStats(stats);
    errors = pool.GetAllPendingErrors();
    CheckForWorkerThreadErrors(errors);
    ASSERT_EQ(working_count.load(), 0U);
    ASSERT_EQ(stats.IdleWorkerCount, 0U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
    ASSERT_EQ(stats.FinishWorkCount, counter.load());
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);
    ASSERT_FALSE(pool.GetErrorPendingFd().IsReadableIntr());
    pool.RequestShutdown();
    pool.WaitForShutdown();
    errors = pool.GetAllPendingErrors();
    CheckForWorkerThreadErrors(errors);
  }

  TEST_F(TManagedThreadPoolTest, StressTest2) {
    std::cout
        << "Running stress test 2 part 1.  This should take about 30-60 "
        << "seconds." << std::endl;
    const size_t initial_thread_count = 50;
    std::atomic<size_t> counter(0);
    std::atomic<size_t> working_count(initial_thread_count);
    TManagedThreadPoolConfig config;
    config.SetPruneQuantumMs(300);
    config.SetPruneQuantumCount(5);
    TManagedThreadFnObjPool pool(TManagedThreadPoolTest::HandleFatalError,
        config);
    pool.Start();
    std::vector<TManagedThreadFnObjPool::TReadyWorker>
        initial_workers(initial_thread_count);
    const size_t count_per_worker = 2000;

    for (auto &w : initial_workers) {
      w = pool.GetReadyWorker();
      auto &fn = w.GetWorkFn();
      ASSERT_TRUE(fn.IsClear());
      fn.SetPool(pool);
      fn.SetCounter(counter);
      fn.SetWorkingCount(working_count);
      fn.SetRemainingCount(count_per_worker);
    }

    for (auto &w : initial_workers) {
      w.Launch();
    }

    std::list<TManagedThreadPoolBase::TWorkerError> errors;

    /* Hopefully 600 seconds will be long enough for the test to finish, even
       on a slow machine. */
    for (size_t i = 0; (i < 600); ++i) {
      TManagedThreadPoolStats stats = pool.GetStats();

      /* Exercise the code path where we don't wait for the manager to finish
         pruning idle threads before we shut down. */
      if ((working_count.load() == 0) &&
          (stats.IdleWorkerCount == stats.LiveWorkerCount) &&
          (stats.FinishWorkCount == counter.load())) {
        break;
      }

      errors = pool.GetAllPendingErrors();
      CheckForWorkerThreadErrors(errors);
      sleep(1);
    }

    TManagedThreadPoolStats stats = pool.GetStats();
    PrintStats(stats);
    errors = pool.GetAllPendingErrors();
    CheckForWorkerThreadErrors(errors);
    ASSERT_EQ(working_count.load(), 0U);
    ASSERT_EQ(stats.IdleWorkerCount, stats.LiveWorkerCount);
    ASSERT_EQ(stats.FinishWorkCount, counter.load());
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);
    ASSERT_FALSE(pool.GetErrorPendingFd().IsReadableIntr());
    ASSERT_EQ(counter.load(), count_per_worker * initial_thread_count);

    pool.RequestShutdown();
    pool.WaitForShutdown();
    errors = pool.GetAllPendingErrors();
    CheckForWorkerThreadErrors(errors);
    stats = pool.GetStats();
    ASSERT_EQ(working_count.load(), 0U);
    ASSERT_EQ(stats.IdleWorkerCount, 0U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
    ASSERT_EQ(stats.FinishWorkCount, counter.load());
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);

    /* Rerun the above test, to make sure the pool behaves properly when
       restarted. */

    std::cout
        << "Running stress test 2 part 2.  This should take about 30-60 "
        << "seconds."
        << std::endl;
    counter = 0;
    working_count = initial_thread_count;
    pool.Start();

    for (auto &w : initial_workers) {
      w = pool.GetReadyWorker();
      auto &fn = w.GetWorkFn();
      ASSERT_TRUE(fn.IsClear());
      fn.SetPool(pool);
      fn.SetCounter(counter);
      fn.SetWorkingCount(working_count);
      fn.SetRemainingCount(count_per_worker);
    }

    for (auto &w : initial_workers) {
      w.Launch();
    }

    /* Hopefully 600 seconds will be long enough for the test to finish, even
       on a slow machine. */
    for (size_t i = 0; (i < 600); ++i) {
      stats = pool.GetStats();

      /* This time, exercise the code path where we wait for the manager to
         finish pruning idle threads before we shut down. */
      if ((working_count.load() == 0) && (stats.IdleWorkerCount == 0) &&
          (stats.LiveWorkerCount == 0) &&
          (stats.FinishWorkCount == counter.load())) {
        break;
      }

      errors = pool.GetAllPendingErrors();
      CheckForWorkerThreadErrors(errors);
      sleep(1);
    }

    stats = pool.GetStats();
    PrintStats(stats);
    errors = pool.GetAllPendingErrors();
    CheckForWorkerThreadErrors(errors);
    ASSERT_EQ(working_count.load(), 0U);
    ASSERT_EQ(stats.IdleWorkerCount, 0U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
    ASSERT_EQ(stats.FinishWorkCount, counter.load());
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);
    ASSERT_FALSE(pool.GetErrorPendingFd().IsReadableIntr());
    ASSERT_EQ(counter.load(), count_per_worker * initial_thread_count);

    pool.RequestShutdown();
    pool.WaitForShutdown();
    errors = pool.GetAllPendingErrors();
    CheckForWorkerThreadErrors(errors);
    ASSERT_EQ(working_count.load(), 0U);
    ASSERT_EQ(stats.IdleWorkerCount, 0U);
    ASSERT_EQ(stats.LiveWorkerCount, 0U);
    ASSERT_EQ(stats.FinishWorkCount, counter.load());
    ASSERT_EQ(stats.QueueErrorCount, 0U);
    ASSERT_EQ(stats.NotifyErrorCount, 0U);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
