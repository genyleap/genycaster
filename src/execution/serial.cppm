module;

#include <memory>

export module farcaster.execution.serial;

import farcaster.application.executor;
import farcaster.foundation.result;

export namespace farcaster::execution::serial {

/**
 * @brief RAII single-worker executor for ordered blocking read work.
 *
 * Accepted jobs execute sequentially on one owned worker. Destruction stops
 * accepting work, drains accepted jobs, and joins the worker.
 */
class SerialExecutor final : public application::executor::IExecutor {
public:
    /** @brief Starts the owned worker thread. */
    SerialExecutor();
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
