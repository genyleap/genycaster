/*!
 * @file        ui/app/bootstrap/worker.cppm
 * @brief       Joining startup worker ownership.
 *
 * @details
 * Owns the native bootstrap thread and guarantees joining before referenced GUI and runtime controllers can be destroyed.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include <functional>
#include <memory>

export module genycaster.ui.bootstrap.worker;

export namespace genycaster::ui::bootstrap {
class BootstrapWorker final {
public:
    BootstrapWorker();
    ~BootstrapWorker();
    BootstrapWorker(const BootstrapWorker&) = delete;
    BootstrapWorker& operator=(const BootstrapWorker&) = delete;

    // Starts one bootstrap task. The owning application must keep borrowed
    // controllers alive until join() completes.
    void start(std::function<void()> operation);
    void join() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
