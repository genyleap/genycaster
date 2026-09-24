#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

import farcaster.execution.thread_pool;

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
    return 0;
}
