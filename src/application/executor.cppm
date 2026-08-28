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
