// Joining bootstrap worker; the GUI thread owns runtime publication.
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
