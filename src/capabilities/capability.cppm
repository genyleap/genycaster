module;

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

export module farcaster.capability.model;

export namespace farcaster::capability::model {

/** @brief Stable operation-level capability identifier. */
struct CapabilityId final {
    std::string value{};
};

/** @brief Semantic version of a capability contract. */
struct CapabilityVersion final {
    std::uint16_t major{1};
    std::uint16_t minor{0};
};

/** @brief Architectural origin of a capability. */
enum class CapabilityOrigin : std::uint8_t {
    Protocol,
    Derived,
    Compatibility,
    Extension,
    Local,
};

/** @brief Quality level of a provider implementation for a capability. */
enum class CapabilityQuality : std::uint8_t {
    Native,
    Derived,
    Emulated,
    Partial,
    Experimental,
    Unavailable,
};

/** @brief Access mode supported by a capability implementation. */
enum class CapabilityAccess : std::uint8_t {
    Read,
    Write,
    Realtime,
    Historical,
};

/** @brief Provider-neutral declaration of one supported capability operation. */
struct CapabilityDescriptor final {
    CapabilityId id{};
    CapabilityVersion version{};
    CapabilityOrigin origin{CapabilityOrigin::Protocol};
    CapabilityQuality quality{CapabilityQuality::Unavailable};
    std::vector<CapabilityAccess> access{};
    bool isSelfHostable{false};
};

}
