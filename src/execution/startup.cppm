/*!
 * @file        src/execution/startup.cppm
 * @brief       Provides exception-safe native worker-group construction.
 *
 * @details
 * Rolls back partially started thread groups before rethrowing a thread
 * creation failure, preventing joinable-thread destruction on unwind.
 *
 * @author      Kambiz Asadzadeh
 * @since       09 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

export module farcaster.execution.startup;

export namespace farcaster::execution::startup {

/**
 * @brief Starts an owned worker group or joins every started worker on failure.
 *
 * @details
 * The rollback callback must be noexcept and wake any workers waiting on a
 * condition variable. No jobs may be admitted until construction succeeds.
 */
template<class Launch, class RequestStop>
void startWorkerGroup(std::vector<std::thread>& workers, const std::size_t count,
                      Launch&& launch, RequestStop&& requestStop)
{
    static_assert(std::is_nothrow_invocable_v<RequestStop&>,
                  "Worker-startup rollback must not throw while unwinding.");
    workers.reserve(count); // An allocation failure here starts no threads.
#if defined(__cpp_exceptions)
    try {
#endif
        for (std::size_t index = 0; index < count; ++index)
            workers.emplace_back(launch());
#if defined(__cpp_exceptions)
    } catch (...) {
        requestStop();
        for (std::thread& worker : workers) {
            if (worker.joinable())
                worker.join();
        }
        throw;
    }
#endif
}

} // namespace farcaster::execution::startup
