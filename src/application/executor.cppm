/*!
 * @file        src/application/executor.cppm
 * @brief       Application scheduling abstraction.
 *
 * @details
 * Defines ownership of scheduled jobs and the execution boundary used to
 * keep blocking work away from presentation threads.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <functional>

export module farcaster.application.executor;

import farcaster.foundation.result;

export namespace farcaster::application::executor {

/** @brief Owned unit of work accepted by an application executor. */
using Job = std::function<void()>;

/**
 * @brief Scheduling boundary for blocking application work.
 *
 * Implementations own the execution context. A successful post transfers the
 * job to that context; destruction must finish or cancel owned work safely.
 */
class IExecutor {
public:
    /** @brief Destroys the executor boundary. */
    virtual ~IExecutor() = default;

    /**
     * @brief Schedules a move-only job outside the caller's execution context.
     * @param job Work whose captures remain owned until execution completes.
     * @return Success or an unavailable failure when work cannot be accepted.
     */
    [[nodiscard]] virtual foundation::result::Status post(Job job) = 0;
};

}
