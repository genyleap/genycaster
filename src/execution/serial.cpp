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

module farcaster.execution.serial;

import farcaster.application.executor;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::execution::serial {

struct SerialExecutor::Impl final {
    explicit Impl(const std::size_t requestedMaximumPendingJobs)
        : maximumPendingJobs(std::max<std::size_t>(requestedMaximumPendingJobs, 1U)),
          worker([this] { run(); })
    {
    }

    void run()
    {
        while (true) {
            application::executor::Job job{};
            {
                std::unique_lock lock{mutex};
                workAvailable.wait(lock, [this] { return isStopping || !jobs.empty(); });
                if (isStopping && jobs.empty()) {
                    return;
                }
                job = std::move(jobs.front());
                jobs.pop_front();
                isExecuting = true;
            }

            job();

            {
                std::scoped_lock lock{mutex};
                isExecuting = false;
                if (jobs.empty()) {
                    drained.notify_all();
                }
            }
        }
    }

    std::mutex mutex{};
    std::condition_variable workAvailable{};
    std::condition_variable drained{};
    std::deque<application::executor::Job> jobs{};
    std::size_t maximumPendingJobs{128U};
    bool isExecuting{false};
    bool isAccepting{true};
    bool isStopping{false};
    std::thread worker{};
};

SerialExecutor::SerialExecutor(const std::size_t maximumPendingJobs)
    : m_impl(std::make_unique<Impl>(maximumPendingJobs))
{
}

SerialExecutor::~SerialExecutor()
{
    {
        std::unique_lock lock{m_impl->mutex};
        m_impl->isAccepting = false;
        m_impl->drained.wait(lock, [this] {
            return m_impl->jobs.empty() && !m_impl->isExecuting;
        });
        m_impl->isStopping = true;
    }
    m_impl->workAvailable.notify_all();
    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }
}

foundation::result::Status SerialExecutor::post(application::executor::Job job)
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
    m_impl->workAvailable.notify_one();
    return {};
}

}
