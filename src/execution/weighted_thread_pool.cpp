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

#include "thread_startup.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

module farcaster.execution.weighted_thread_pool;

import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::execution::weighted_thread_pool {
#if defined(__EMSCRIPTEN__)
// Browser builds have no native worker threads. Process one admitted job per
// browser event-loop turn; the callback owns its queue state until delivery.
struct CooperativeWeightedState final : std::enable_shared_from_this<CooperativeWeightedState> {
    struct Entry final {
        std::size_t retainedBytes{};
        Job job{};
    };

    explicit CooperativeWeightedState(
        const std::size_t maximumJobs,
        const std::size_t maximumBytes)
        : maximumPendingJobs(std::max<std::size_t>(maximumJobs, 1U)),
          maximumQueuedWeightBytes(std::max<std::size_t>(maximumBytes, 1U))
    {
    }

    void schedule()
    {
        if (!accepting || scheduled || jobs.empty()) {
            return;
        }
        scheduled = true;
        auto* const ticket = new std::shared_ptr<CooperativeWeightedState>{shared_from_this()};
        emscripten_async_call([](void* context) {
            std::unique_ptr<std::shared_ptr<CooperativeWeightedState>> owned{
                static_cast<std::shared_ptr<CooperativeWeightedState>*>(context)};
            const auto state = *owned;
            state->scheduled = false;
            if (!state->accepting || state->jobs.empty()) {
                return;
            }
            Entry entry = std::move(state->jobs.front());
            state->jobs.pop_front();
            state->queuedWeightBytes -= entry.retainedBytes;
#if defined(__cpp_exceptions)
            try {
                entry.job();
            } catch (...) {
                // Keep cooperative dispatch alive after a bad callback.
            }
#else
            entry.job();
#endif
            // A job may destroy its executor; the shared state remains valid.
            state->schedule();
        }, ticket, 0);
    }

    std::deque<Entry> jobs{};
    std::size_t maximumPendingJobs{};
    std::size_t maximumQueuedWeightBytes{};
    std::size_t queuedWeightBytes{};
    bool scheduled{false};
    bool accepting{true};
};

struct WeightedThreadPoolExecutor::Impl final {
    Impl(const std::size_t /*workerCount*/,
         const std::size_t maximumPendingJobs,
         const std::size_t maximumQueuedWeightBytes)
        : state(std::make_shared<CooperativeWeightedState>(
              maximumPendingJobs, maximumQueuedWeightBytes))
    {
    }

    std::shared_ptr<CooperativeWeightedState> state;
};

WeightedThreadPoolExecutor::WeightedThreadPoolExecutor(
    const std::size_t workerCount,
    const std::size_t maximumPendingJobs,
    const std::size_t maximumQueuedWeightBytes)
    : m_impl(std::make_unique<Impl>(
          workerCount, maximumPendingJobs, maximumQueuedWeightBytes))
{
}

WeightedThreadPoolExecutor::~WeightedThreadPoolExecutor()
{
    // No blocking wait is possible in a single-threaded browser event loop.
    m_impl->state->accepting = false;
    m_impl->state->jobs.clear();
}

foundation::result::Status WeightedThreadPoolExecutor::post(
    const std::size_t retainedBytes, Job job)
{
    if (!job) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Weighted executor job must not be empty."));
    }
    const auto& state = m_impl->state;
    if (!state->accepting) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Weighted executor is shutting down."));
    }
    if (state->jobs.size() >= state->maximumPendingJobs) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::RateLimited,
            "Weighted executor pending queue is full."));
    }
    if (retainedBytes > state->maximumQueuedWeightBytes ||
        state->queuedWeightBytes > state->maximumQueuedWeightBytes - retainedBytes) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::RateLimited,
            "Weighted executor retained-byte budget is exhausted."));
    }
    // Commit accounting only after deque insertion succeeds. An allocation
    // failure must not permanently consume the retained-byte budget.
    state->jobs.push_back(CooperativeWeightedState::Entry{
        .retainedBytes = retainedBytes,
        .job = std::move(job),
    });
    state->queuedWeightBytes += retainedBytes;
    state->schedule();
    return {};
}
#else
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
        detail::startWorkerGroup(workers, workerCount,
            [this] { return std::thread{[this] { run(); }}; },
            [this]() noexcept {
                {
                    std::scoped_lock lock{mutex};
                    isStopping = true;
                    isAccepting = false;
                }
                available.notify_all();
            });
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

#if defined(__cpp_exceptions)
            try {
                weightedJob.job();
            } catch (...) {
                // Restore active-work accounting and allow later jobs to run.
            }
#else
            weightedJob.job();
#endif

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
        // Preserve accounting if deque insertion throws bad_alloc. The job
        // is movable without throwing; enqueue is the only commit point.
        m_impl->jobs.push_back(WeightedJob{
            .retainedBytes = retainedBytes,
            .job = std::move(job),
        });
        m_impl->queuedWeightBytes += retainedBytes;
    }
    m_impl->available.notify_one();
    return {};
}
#endif

}
