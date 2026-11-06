module;

#include <QCoreApplication>
#include <QObject>
#include <QThread>
#include <QThreadPool>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

module genycaster.ui.shutdown;

import farcaster.transport.qtnetwork;

namespace genycaster::ui::shutdown {
namespace {

struct DispatchState final {
    std::mutex mutex{};
    std::condition_variable changed{};
    bool isStopping{};
};

struct PendingDispatch final {
    bool isComplete{};
    bool isAbandoned{};
    bool result{};
};

std::mutex dispatchScopeMutex{};
std::weak_ptr<DispatchState> activeDispatchScope{};

}

class ShutdownScope::Impl final {
public:
    farcaster::transport::qtnetwork::RequestShutdownScope requests{};
    std::shared_ptr<DispatchState> dispatch{std::make_shared<DispatchState>()};
    QObject context{};
};

ShutdownScope::ShutdownScope() : m_impl(std::make_unique<Impl>())
{
    auto* const application = QCoreApplication::instance();
    if (application == nullptr) {
        throw std::logic_error{"ShutdownScope requires an active QCoreApplication."};
    }
    {
        std::scoped_lock lock{dispatchScopeMutex};
        activeDispatchScope = m_impl->dispatch;
    }
    QObject::connect(application, &QCoreApplication::aboutToQuit,
        &m_impl->context, [this] { stop(); });
}

ShutdownScope::~ShutdownScope()
{
    stop();
}

void ShutdownScope::stop()
{
    {
        std::scoped_lock lock{m_impl->dispatch->mutex};
        m_impl->dispatch->isStopping = true;
    }
    m_impl->dispatch->changed.notify_all();
    m_impl->requests.stop();
#if !defined(__EMSCRIPTEN__)
    // Worker event loops must still receive abort/finished/timer events. Qt's
    // application destructor disables that delivery before waiting on this pool.
    // Drain again on scope destruction in case another aboutToQuit handler
    // submitted cleanup work after the first drain.
    QThreadPool::globalInstance()->waitForDone();
#endif
}

bool invokeOnApplication(std::function<bool()> operation)
{
    std::shared_ptr<DispatchState> state{};
    {
        std::scoped_lock lock{dispatchScopeMutex};
        state = activeDispatchScope.lock();
    }
    auto* const application = QCoreApplication::instance();
    if (!state || !application || !operation) {
        return false;
    }
    std::unique_lock lock{state->mutex};
    if (state->isStopping) {
        return false;
    }
    if (QThread::currentThread() == application->thread()) {
        lock.unlock();
        return operation();
    }
    auto pending = std::make_shared<PendingDispatch>();
    const bool queued = QMetaObject::invokeMethod(application,
        [state, pending, operation = std::move(operation)] {
            {
                std::scoped_lock callbackLock{state->mutex};
                if (state->isStopping || pending->isAbandoned) {
                    return;
                }
            }
            const bool result = operation();
            {
                std::scoped_lock callbackLock{state->mutex};
                pending->result = result;
                pending->isComplete = true;
            }
            state->changed.notify_all();
        }, Qt::QueuedConnection);
    if (!queued) {
        return false;
    }
    state->changed.wait_for(lock, std::chrono::seconds{5}, [&] {
        return pending->isComplete || state->isStopping;
    });
    pending->isAbandoned = !pending->isComplete;
    return pending->isComplete && pending->result;
}

}
