module;

#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

module genycaster.ui.bootstrap.worker;

namespace genycaster::ui::bootstrap {
struct BootstrapWorker::Impl final {
    std::thread worker{};
};

BootstrapWorker::BootstrapWorker() : m_impl{std::make_unique<Impl>()} {}
BootstrapWorker::~BootstrapWorker() { join(); }

void BootstrapWorker::start(std::function<void()> operation)
{
    if (!operation || m_impl->worker.joinable())
        throw std::logic_error{"Bootstrap worker already active or task missing"};
    m_impl->worker = std::thread{std::move(operation)};
}

void BootstrapWorker::join() noexcept
{
    if (m_impl && m_impl->worker.joinable())
        m_impl->worker.join();
}
} // namespace genycaster::ui::bootstrap
