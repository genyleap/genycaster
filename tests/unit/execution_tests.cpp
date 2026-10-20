#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>

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

#if defined(__cpp_exceptions)
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
    return 0;
}
