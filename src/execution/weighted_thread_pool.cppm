module;

#include <cstddef>
#include <functional>
#include <memory>

export module farcaster.execution.weighted_thread_pool;

import farcaster.foundation.result;

export namespace farcaster::execution::weighted_thread_pool {

/** @brief Owned unit of CPU work whose retained memory is accounted explicitly. */
using Job = std::function<void()>;

/**
 * @brief Bounded CPU pool with both queue-count and retained-byte backpressure.
 *
 * The weight supplied to post() represents bytes retained by the queued job,
 * such as an HTTP response awaiting JSON decode. Running jobs are no longer
 * counted against queued weight because their memory is actively being
 * processed by a bounded number of workers.
 */
class WeightedThreadPoolExecutor final {
public:
    WeightedThreadPoolExecutor(
        std::size_t workerCount,
        std::size_t maximumPendingJobs,
        std::size_t maximumQueuedWeightBytes);
    ~WeightedThreadPoolExecutor();
    WeightedThreadPoolExecutor(const WeightedThreadPoolExecutor&) = delete;
    auto operator=(const WeightedThreadPoolExecutor&) -> WeightedThreadPoolExecutor& = delete;
    WeightedThreadPoolExecutor(WeightedThreadPoolExecutor&&) = delete;
    auto operator=(WeightedThreadPoolExecutor&&) -> WeightedThreadPoolExecutor& = delete;

    /** @brief Enqueues work when both count and byte budgets have capacity. */
    [[nodiscard]] foundation::result::Status post(std::size_t retainedBytes, Job job);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
