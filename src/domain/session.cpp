module;

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <system_error>

module farcaster.domain.session;

namespace farcaster::domain::session {

bool isExpired(
    const ClientSession& session,
    const foundation::time::Timestamp now) noexcept
{
    return session.expiresAt <= now;
}

bool isRegistryCustodyAccount(const ClientSession& session) noexcept
{
    if (session.method != AuthenticationMethod::Custody || session.fid.value == 0U
        || !session.identityIssuer || !session.identitySubject || !session.custodyAddress
        || session.expiresAt <= session.authenticatedAt) return false;

    const std::string_view address = *session.custodyAddress;
    if (address.size() != 42U || !address.starts_with("0x")
        || !std::all_of(address.begin() + 2, address.end(), [](const char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) return false;

    std::string_view prefix;
    if (*session.identityIssuer == "farcaster:optimism-id-registry"
        && session.fid.value <= 9'007'199'254'740'991ULL) {
        prefix = "farcaster:";
    } else if (*session.identityIssuer == "hypersnap:native-registry"
               && session.fid.value >= (1ULL << 32)) {
        prefix = "native-fid:";
    } else {
        return false;
    }
    std::string_view subject = *session.identitySubject;
    if (!subject.starts_with(prefix)) return false;
    subject.remove_prefix(prefix.size());
    if (subject.empty() || subject.front() == '0') return false;
    std::uint64_t fid{};
    const auto parsed = std::from_chars(subject.data(), subject.data() + subject.size(), fid);
    return parsed.ec == std::errc{} && parsed.ptr == subject.data() + subject.size()
        && fid == session.fid.value;
}

bool isProtocolSessionEligible(const ClientSession& session,
                               const foundation::time::Timestamp now) noexcept
{
    return !isExpired(session, now)
        || (session.authenticatedAt <= now && isRegistryCustodyAccount(session));
}

}
