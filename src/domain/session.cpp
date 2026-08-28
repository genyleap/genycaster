module;

#include <chrono>

module farcaster.domain.session;

namespace farcaster::domain::session {

bool isExpired(
    const ClientSession& session,
    const foundation::time::Timestamp now) noexcept
{
    return session.expiresAt <= now;
}

}
