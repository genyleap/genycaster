module;

#include <condition_variable>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

module farcaster.execution.serial;

import farcaster.application.executor;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::execution::serial {

struct SerialExecutor::Impl final {
    Impl()
        : worker([this](const std::stop_token stopToken) { run(stopToken); })
    {
    }

    void run(const std::stop_token stopToken)
    {
        while (true) {
            application::executor::Job job{};
            {
                std::unique_lock lock{mutex};
                condition.wait(lock, stopToken, [this] { return !jobs.empty(); });
                if (jobs.empty()) {
                    return;
                }
                job = std::move(jobs.front());
                jobs.pop_front();
            }
            job();
        }
    }

    std::mutex mutex{};
    std::condition_variable_any condition{};
    std::deque<application::executor::Job> jobs{};
    bool accepting{true};
    std::jthread worker;
};

SerialExecutor::SerialExecutor()
    : m_impl(std::make_unique<Impl>())
{
}

SerialExecutor::~SerialExecutor()
{
    {
        std::scoped_lock lock{m_impl->mutex};
        m_impl->accepting = false;
    }
    while (true) {
        {
            std::scoped_lock lock{m_impl->mutex};
            if (m_impl->jobs.empty()) {
                break;
            }
        }
        std::this_thread::yield();
    }
    m_impl->worker.request_stop();
    m_impl->condition.notify_all();
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
        if (!m_impl->accepting) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Executor is shutting down."));
        }
        m_impl->jobs.push_back(std::move(job));
    }
    m_impl->condition.notify_one();
    return {};
}

}
