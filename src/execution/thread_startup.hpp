/*!
 * @file src/execution/thread_startup.hpp
 * @brief Exception-safe launch of the execution worker groups.
 *
 * A partially constructed std::vector<std::thread> must not unwind while
 * its elements are joinable: ~std::thread would call std::terminate().
 * This helper is used only during construction, before jobs are accepted.
 */
#pragma once

#include <cstddef>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace farcaster::execution::detail {

// launch() returns a newly started std::thread; requestStop() must be noexcept
// and wake workers blocked in their condition_variable. When thread creation
// fails, join every worker before allowing the original exception to escape.
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

} // namespace farcaster::execution::detail
