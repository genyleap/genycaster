#include "../../src/execution/thread_startup.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

import farcaster.execution.thread_pool;
import farcaster.execution.serial;
import farcaster.execution.weighted_thread_pool;

int main()
{
    using namespace std::chrono_literals;

    std::mutex mutex{};
    std::condition_variable condition{};
    int startedJobs{};
    bool releaseJobs{false};

    farcaster::execution::thread_pool::ThreadPoolExecutor executor{2U};
    const auto postBlockingJob = [&] {
        return executor.post([&] {
            std::unique_lock lock{mutex};
            ++startedJobs;
            condition.notify_all();
            condition.wait(lock, [&] { return releaseJobs; });
        });
    };

    assert(postBlockingJob());
    assert(postBlockingJob());

    {
        std::unique_lock lock{mutex};
        const bool bothStarted = condition.wait_for(lock, 2s, [&] {
            return startedJobs == 2;
        });
        assert(bothStarted);
        releaseJobs = true;
    }
    condition.notify_all();

#if !defined(__EMSCRIPTEN__) && defined(__cpp_exceptions)
    // Deterministically exercise the same launch guard used by both native
    // pools: after one worker starts, the next thread factory throws.
    // The guard must wake and join the first worker *before* unwinding.
    {
        std::mutex startMutex{};
        std::condition_variable startCondition{};
        bool stopRequested = false;
        std::atomic<int> workersFinished{0};
        int attempts = 0;
        std::vector<std::thread> workers{};
        bool caught = false;
        try {
            farcaster::execution::detail::startWorkerGroup(workers, 3U,
                [&]() -> std::thread {
                    if (++attempts == 2)
                        throw std::system_error{
                            std::make_error_code(std::errc::resource_unavailable_try_again)};
                    return std::thread{[&] {
                        std::unique_lock lock{startMutex};
                        startCondition.wait(lock, [&] { return stopRequested; });
                        ++workersFinished;
                    }};
                },
                [&]() noexcept {
                    {
                        std::scoped_lock lock{startMutex};
                        stopRequested = true;
                    }
                    startCondition.notify_all();
                });
        } catch (const std::system_error&) {
            caught = true;
        }
        assert(caught);
        assert(attempts == 2);
        assert(workersFinished.load() == 1);
        assert(workers.size() == 1 && !workers.front().joinable());
    }
#endif

#if defined(__cpp_exceptions)
    // An executor with one worker must survive a throwing callback, drain
    // that job and execute the next callback on that same worker.
    {
        farcaster::execution::thread_pool::ThreadPoolExecutor guarded{1U, 4U};
        std::promise<void> completed{};
        auto observed = completed.get_future();
        assert(guarded.post([] {
            throw std::runtime_error{"test pooled callback failure"};
        }));
        assert(guarded.post([&completed] { completed.set_value(); }));
        assert(observed.wait_for(2s) == std::future_status::ready);
    }
    // A throwing user callback must not terminate the executor thread or
    // poison drain bookkeeping. The next admitted job still completes.
    {
        farcaster::execution::serial::SerialExecutor serial{4U};
        std::promise<void> completed{};
        auto observed = completed.get_future();
        assert(serial.post([] { throw std::runtime_error{"test callback failure"}; }));
        assert(serial.post([&completed] { completed.set_value(); }));
        assert(observed.wait_for(2s) == std::future_status::ready);
    }
    {
        farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor weighted{
            1U, 4U, 4096U};
        std::promise<void> completed{};
        auto observed = completed.get_future();
        assert(weighted.post(512U, [] {
            throw std::runtime_error{"test decode callback failure"};
        }));
        assert(weighted.post(512U, [&completed] { completed.set_value(); }));
        assert(observed.wait_for(2s) == std::future_status::ready);
    }
#endif
#if !defined(__EMSCRIPTEN__)
    // Bounded decode admission must recover capacity after an active worker
    // releases its queued jobs. A rejected third enqueue must not charge
    // the retained-byte budget or starve a later full-budget operation.
    {
        farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor bounded{
            1U, 2U, 512U};
        std::mutex queueMutex{};
        std::condition_variable queueCondition{};
        bool entered = false;
        bool release = false;
        std::promise<void> secondCompleted{};
        std::promise<void> thirdCompleted{};
        auto secondFuture = secondCompleted.get_future();
        auto thirdFuture = thirdCompleted.get_future();
        assert(bounded.post(64U, [&] {
            std::unique_lock lock{queueMutex};
            entered = true;
            queueCondition.notify_all();
            queueCondition.wait(lock, [&] { return release; });
        }));
        {
            std::unique_lock lock{queueMutex};
            assert(queueCondition.wait_for(lock, 2s, [&] { return entered; }));
        }
        assert(bounded.post(256U, [&] { secondCompleted.set_value(); }));
        assert(bounded.post(256U, [&] { thirdCompleted.set_value(); }));
        assert(!bounded.post(1U, [] {}));
        {
            std::scoped_lock lock{queueMutex};
            release = true;
        }
        queueCondition.notify_all();
        assert(secondFuture.wait_for(2s) == std::future_status::ready);
        assert(thirdFuture.wait_for(2s) == std::future_status::ready);
        std::promise<void> reused{};
        auto reusedFuture = reused.get_future();
        assert(bounded.post(512U, [&] { reused.set_value(); }));
        assert(reusedFuture.wait_for(2s) == std::future_status::ready);
    }
#endif
    return 0;
}
