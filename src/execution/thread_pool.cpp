module;

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "thread_startup.hpp"

module farcaster.execution.thread_pool;

import farcaster.application.executor;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::execution::thread_pool {

struct ThreadPoolExecutor::Impl final {
    Impl(
        const std::size_t requestedWorkerCount,
        const std::size_t requestedMaximumPendingJobs)
        : maximumPendingJobs(std::max<std::size_t>(requestedMaximumPendingJobs, 1U))
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
                condition.notify_all();
            });
    }

    ~Impl() = default;
    Impl(const Impl&) = delete;
    auto operator=(const Impl&) -> Impl& = delete;
    Impl(Impl&&) = delete;
    auto operator=(Impl&&) -> Impl& = delete;

    void run()
    {
        while (true) {
            application::executor::Job job{};
            {
                std::unique_lock lock{mutex};
                condition.wait(lock, [this] { return isStopping || !jobs.empty(); });
                if (isStopping && jobs.empty()) {
                    return;
                }
                job = std::move(jobs.front());
                jobs.pop_front();
                ++activeJobs;
            }

#if defined(__cpp_exceptions)
            try {
                job();
            } catch (...) {
                // User-supplied jobs must not terminate the entire process.
                // As in the serial/weighted executors, restore drain
                // bookkeeping and continue with subsequent admitted work.
            }
#else
            job();
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
    std::condition_variable condition{};
    std::condition_variable drained{};
    std::deque<application::executor::Job> jobs{};
    std::size_t activeJobs{};
    std::size_t maximumPendingJobs{96U};
    bool isAccepting{true};
    bool isStopping{false};
    std::vector<std::thread> workers{};
};

ThreadPoolExecutor::ThreadPoolExecutor(
    const std::size_t workerCount,
    const std::size_t maximumPendingJobs)
    : m_impl(std::make_unique<Impl>(workerCount, maximumPendingJobs))
{
}

ThreadPoolExecutor::~ThreadPoolExecutor()
{
    {
        std::unique_lock lock{m_impl->mutex};
        m_impl->isAccepting = false;
        m_impl->drained.wait(lock, [this] {
            return m_impl->jobs.empty() && m_impl->activeJobs == 0U;
        });
        m_impl->isStopping = true;
    }
    m_impl->condition.notify_all();

    for (auto& worker : m_impl->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

foundation::result::Status ThreadPoolExecutor::post(application::executor::Job job)
{
    if (!job) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Executor job must not be empty."));
    }
    {
        std::scoped_lock lock{m_impl->mutex};
        if (!m_impl->isAccepting) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Executor is shutting down."));
        }
        if (m_impl->jobs.size() >= m_impl->maximumPendingJobs) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::RateLimited,
                "Executor pending queue is full."));
        }
        m_impl->jobs.push_back(std::move(job));
    }
    m_impl->condition.notify_one();
    return {};
}

}
