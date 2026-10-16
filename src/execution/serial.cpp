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

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

module farcaster.execution.serial;

import farcaster.application.executor;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::execution::serial {
#if defined(__EMSCRIPTEN__)
// In the browser, preserve asynchronous FIFO dispatch without std::thread.
// Each posted job runs in its own event-loop turn.
struct CooperativeSerialState final : std::enable_shared_from_this<CooperativeSerialState> {
    explicit CooperativeSerialState(const std::size_t limit)
        : maximumPendingJobs(std::max<std::size_t>(limit, 1U))
    {
    }

    void schedule()
    {
        if (!accepting || scheduled || jobs.empty()) {
            return;
        }
        scheduled = true;
        auto* const ticket = new std::shared_ptr<CooperativeSerialState>{shared_from_this()};
        emscripten_async_call([](void* context) {
            std::unique_ptr<std::shared_ptr<CooperativeSerialState>> owned{
                static_cast<std::shared_ptr<CooperativeSerialState>*>(context)};
            const auto state = *owned;
            state->scheduled = false;
            if (!state->accepting || state->jobs.empty()) {
                return;
            }
            auto job = std::move(state->jobs.front());
            state->jobs.pop_front();
            job();
            state->schedule();
        }, ticket, 0);
    }

    std::deque<application::executor::Job> jobs{};
    std::size_t maximumPendingJobs{};
    bool accepting{true};
    bool scheduled{false};
};

struct SerialExecutor::Impl final {
    explicit Impl(const std::size_t maximumPendingJobs)
        : state(std::make_shared<CooperativeSerialState>(maximumPendingJobs))
    {
    }

    std::shared_ptr<CooperativeSerialState> state;
};

SerialExecutor::SerialExecutor(const std::size_t maximumPendingJobs)
    : m_impl(std::make_unique<Impl>(maximumPendingJobs))
{
}

SerialExecutor::~SerialExecutor()
{
    // Cancel outstanding callbacks safely instead of blocking the browser.
    m_impl->state->accepting = false;
    m_impl->state->jobs.clear();
}

foundation::result::Status SerialExecutor::post(application::executor::Job job)
{
    if (!job) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Executor job must not be empty."));
    }
    const auto& state = m_impl->state;
    if (!state->accepting) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Executor is shutting down."));
    }
    if (state->jobs.size() >= state->maximumPendingJobs) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::RateLimited,
            "Executor pending queue is full."));
    }
    state->jobs.push_back(std::move(job));
    state->schedule();
    return {};
}
#else
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
#endif

}
