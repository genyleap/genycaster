module;

#include <algorithm>
#include <chrono>
#include <vector>

module farcaster.domain.signer;

namespace farcaster::domain::signer {

std::vector<Scope> standardSocialScopes()
{
    return {
        Scope::CastAdd,
        Scope::CastRemove,
        Scope::ReactionAdd,
        Scope::ReactionRemove,
        Scope::LinkAdd,
        Scope::LinkRemove,
    };
}

bool isAdmissibleScope(const Scope scope) noexcept
{
    switch (scope) {
    case Scope::CastAdd:
    case Scope::CastRemove:
    case Scope::ReactionAdd:
    case Scope::ReactionRemove:
    case Scope::LinkAdd:
    case Scope::LinkRemove:
    case Scope::VerificationAddEthAddress:
    case Scope::VerificationRemove:
    case Scope::UserDataAdd:
    case Scope::UsernameProof:
    case Scope::FrameAction:
    case Scope::LinkCompactState:
    case Scope::LendStorage:
        return true;
    }
    return false;
}

bool isExpired(
    const Registration& registration,
    const foundation::time::Timestamp now) noexcept
{
    if (registration.kind == RegistrationKind::LegacyOnchain) {
        return false;
    }
    if (registration.ttlSeconds == 0U || !registration.lastUsedAt) {
        return true;
    }
    const auto ttl = std::chrono::seconds{registration.ttlSeconds};
    return *registration.lastUsedAt +
        std::chrono::duration_cast<std::chrono::milliseconds>(ttl) < now;
}

bool permits(
    const Registration& registration,
    const Scope requiredScope,
    const foundation::time::Timestamp now) noexcept
{
    if (registration.state != RegistrationState::Active) {
        return false;
    }
    if (registration.kind == RegistrationKind::LegacyOnchain) {
        return true;
    }
    if (isExpired(registration, now)) {
        return false;
    }
    return std::find(registration.scopes.begin(), registration.scopes.end(), requiredScope) != registration.scopes.end();
}

}
