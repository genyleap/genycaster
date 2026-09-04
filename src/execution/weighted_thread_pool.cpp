module;

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

module farcaster.execution.weighted_thread_pool;

import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::execution::weighted_thread_pool {
namespace {

struct WeightedJob final {
    std::size_t retainedBytes{};
    Job job{};
};

}

struct WeightedThreadPoolExecutor::Impl final {
    Impl(
        const std::size_t requestedWorkerCount,
        const std::size_t requestedMaximumPendingJobs,
        const std::size_t requestedMaximumQueuedWeightBytes)
        : maximumPendingJobs(std::max<std::size_t>(requestedMaximumPendingJobs, 1U)),
          maximumQueuedWeightBytes(std::max<std::size_t>(requestedMaximumQueuedWeightBytes, 1U))
    {
        const std::size_t workerCount = std::max<std::size_t>(requestedWorkerCount, 1U);
        workers.reserve(workerCount);
        for (std::size_t index = 0; index < workerCount; ++index) {
            workers.emplace_back([this] { run(); });
        }
    }

    void run()
    {
        while (true) {
            WeightedJob weightedJob{};
            {
                std::unique_lock lock{mutex};
                available.wait(lock, [this] { return isStopping || !jobs.empty(); });
                if (isStopping && jobs.empty()) {
                    return;
                }
                weightedJob = std::move(jobs.front());
                jobs.pop_front();
                queuedWeightBytes -= weightedJob.retainedBytes;
                ++activeJobs;
            }

            weightedJob.job();

            {
                std::scoped_lock lock{mutex};
                --activeJobs;
                if (jobs.empty() && activeJobs == 0U) {
                    drained.notify_all();
                }
            }
        }
    }

    std::mutex mutex{};
    std::condition_variable available{};
    std::condition_variable drained{};
    std::deque<WeightedJob> jobs{};
    std::vector<std::thread> workers{};
    std::size_t maximumPendingJobs{};
    std::size_t maximumQueuedWeightBytes{};
    std::size_t queuedWeightBytes{};
    std::size_t activeJobs{};
    bool isAccepting{true};
    bool isStopping{false};
};

WeightedThreadPoolExecutor::WeightedThreadPoolExecutor(
    const std::size_t workerCount,
    const std::size_t maximumPendingJobs,
    const std::size_t maximumQueuedWeightBytes)
    : m_impl(std::make_unique<Impl>(
          workerCount,
          maximumPendingJobs,
          maximumQueuedWeightBytes))
{
}

WeightedThreadPoolExecutor::~WeightedThreadPoolExecutor()
{
    {
        std::unique_lock lock{m_impl->mutex};
        m_impl->isAccepting = false;
        m_impl->drained.wait(lock, [this] {
            return m_impl->jobs.empty() && m_impl->activeJobs == 0U;
        });
        m_impl->isStopping = true;
    }
    m_impl->available.notify_all();
    for (auto& worker : m_impl->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

foundation::result::Status WeightedThreadPoolExecutor::post(
    const std::size_t retainedBytes,
    Job job)
{
    if (!job) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Weighted executor job must not be empty."));
    }

    {
        std::scoped_lock lock{m_impl->mutex};
        if (!m_impl->isAccepting) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Weighted executor is shutting down."));
        }
        if (m_impl->jobs.size() >= m_impl->maximumPendingJobs) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::RateLimited,
                "Weighted executor pending queue is full."));
        }
        if (retainedBytes > m_impl->maximumQueuedWeightBytes ||
            m_impl->queuedWeightBytes > m_impl->maximumQueuedWeightBytes - retainedBytes) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::RateLimited,
                "Weighted executor retained-byte budget is exhausted."));
        }
        m_impl->queuedWeightBytes += retainedBytes;
        m_impl->jobs.push_back(WeightedJob{
            .retainedBytes = retainedBytes,
            .job = std::move(job),
        });
    }
    m_impl->available.notify_one();
    return {};
}

}
