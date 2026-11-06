/*!
 * @file        src/execution/thread_pool.cppm
 * @brief       Bounded general-purpose thread pool.
 *
 * @details
 * Schedules independent CPU or compatibility work with explicit worker
 * and pending-queue ownership.
 *
 * @author      Kambiz Asadzadeh
 * @since       04 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <memory>

export module farcaster.execution.thread_pool;

import farcaster.application.executor;
import farcaster.foundation.result;

export namespace farcaster::execution::thread_pool {

/**
 * @brief RAII bounded worker pool for independent CPU or compatibility work.
 *
 * Both worker count and pending queue length are bounded. Network waiting should
 * use the asynchronous transport reactor instead of occupying these workers.
 */
class ThreadPoolExecutor final : public application::executor::IExecutor {
public:
    /** @brief Starts a bounded worker pool. Zero values are normalized to one. */
    explicit ThreadPoolExecutor(
        std::size_t workerCount = 4U,
        std::size_t maximumPendingJobs = 96U);
    /** @brief Drains accepted work and joins all worker threads. */
    ~ThreadPoolExecutor() override;
    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    auto operator=(const ThreadPoolExecutor&) -> ThreadPoolExecutor& = delete;
    ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
    auto operator=(ThreadPoolExecutor&&) -> ThreadPoolExecutor& = delete;

    /** @inheritdoc */
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
