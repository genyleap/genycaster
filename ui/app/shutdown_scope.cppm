/*!
 * @file        ui/app/shutdown_scope.cppm
 * @brief       Native application background-work shutdown ownership.
 * @details
 * Owns native application shutdown ordering, stopping blocking HTTP requests
 * and draining global Qt pool work before application event delivery ends.
 * @author      Kambiz Asadzadeh
 * @since       06 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <memory>
#include <functional>

export module genycaster.ui.shutdown;

export namespace genycaster::ui::shutdown {

/**
 * @brief Stops blocking HTTP and joins global Qt jobs before application teardown.
 *
 * Create on the application thread before starting workers, after declaring their
 * borrowed owners, and destroy before those owners and QCoreApplication. Connects
 * to aboutToQuit and also handles early returns through RAII. Only one scope may
 * exist. Pool jobs must not synchronously wait for the GUI thread; dedicated
 * executors remain responsible for joining their own workers. No work is detached
 * or force-terminated. Local storage jobs are drained, not discarded.
 */
class ShutdownScope final {
public:
    /** @brief Binds to the active QCoreApplication; throws if none exists. */
    ShutdownScope();

    /** @brief Stops network admission and drains pending/running global Qt jobs. */
    ~ShutdownScope();

    ShutdownScope(const ShutdownScope&) = delete;
    auto operator=(const ShutdownScope&) -> ShutdownScope& = delete;

    /** @brief Idempotent shutdown on the application thread, before Qt teardown. */
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Runs a short platform operation on the application thread.
 *
 * Requires an active ShutdownScope. Returns false if dispatch fails, shutdown
 * starts, or the GUI does not service the request within five seconds. Pending
 * callbacks are then suppressed. The operation owns its captures and returns
 * its actual success value; an already executing platform call cannot be undone.
 * Shutdown wakes waiting workers without needing another GUI event.
 */
[[nodiscard]] bool invokeOnApplication(std::function<bool()> operation);

}
