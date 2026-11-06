/*!
 * @file        src/execution/serial.cppm
 * @brief       Bounded serial task executor.
 *
 * @details
 * Provides ordered execution with RAII worker ownership and queue limits
 * for operations requiring serialization.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <memory>

export module farcaster.execution.serial;

import farcaster.application.executor;
import farcaster.foundation.result;

export namespace farcaster::execution::serial {

/**
 * @brief RAII single-worker executor for ordered bounded work.
 *
 * Accepted jobs execute sequentially on one owned worker. The pending queue is
 * bounded so a stalled persistence/session lane cannot grow process memory
 * without limit. Destruction stops accepting work, drains accepted jobs, and
 * joins the worker without busy-spinning.
 */
class SerialExecutor final : public application::executor::IExecutor {
public:
    /** @brief Starts the worker with a bounded pending queue. */
    explicit SerialExecutor(std::size_t maximumPendingJobs = 128U);
    /** @brief Drains accepted work and joins the worker thread. */
    ~SerialExecutor() override;
    SerialExecutor(const SerialExecutor&) = delete;
    auto operator=(const SerialExecutor&) -> SerialExecutor& = delete;
    SerialExecutor(SerialExecutor&&) = delete;
    auto operator=(SerialExecutor&&) -> SerialExecutor& = delete;

    /** @inheritdoc */
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
