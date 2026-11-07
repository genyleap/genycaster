#include <atomic>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>

import genycaster.ui.bootstrap.worker;

int main()
{
    using genycaster::ui::bootstrap::BootstrapWorker;
    std::atomic_int completed{};
    {
        BootstrapWorker worker{};
        worker.start([&completed] {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
            ++completed;
        });
        // Replacing an active thread would terminate the process.
        bool refused{};
        try {
            worker.start([] {});
        } catch (const std::logic_error&) {
            refused = true;
        }
        if (!refused) return 1;
        worker.join();
        worker.join(); // idempotent
        if (completed != 1) return 2;
    }
    {
        BootstrapWorker worker{};
        worker.start([&completed] { ++completed; });
        // Early return/destruction must join before captured stack state dies.
    }
    return completed == 2 ? 0 : 3;
}
