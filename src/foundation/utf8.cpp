module;

#include <cstddef>
#include <string_view>

module farcaster.foundation.utf8;

namespace farcaster::foundation::utf8 {
namespace {

[[nodiscard]] constexpr bool continuation(const unsigned char value) noexcept
{
    return (value & 0xC0U) == 0x80U;
}

[[nodiscard]] bool consumeScalar(std::string_view value, std::size_t& index) noexcept
{
    const auto first = static_cast<unsigned char>(value[index]);
    if (first <= 0x7FU) {
        ++index;
        return true;
    }
    if (first >= 0xC2U && first <= 0xDFU) {
        if (index + 1U >= value.size() || !continuation(static_cast<unsigned char>(value[index + 1U]))) return false;
        index += 2U;
        return true;
    }
    if (first == 0xE0U) {
        if (index + 2U >= value.size()) return false;
        const auto second = static_cast<unsigned char>(value[index + 1U]);
        const auto third = static_cast<unsigned char>(value[index + 2U]);
        if (second < 0xA0U || second > 0xBFU || !continuation(third)) return false;
        index += 3U;
        return true;
    }
    if ((first >= 0xE1U && first <= 0xECU) || (first >= 0xEEU && first <= 0xEFU)) {
        if (index + 2U >= value.size() ||
            !continuation(static_cast<unsigned char>(value[index + 1U])) ||
            !continuation(static_cast<unsigned char>(value[index + 2U]))) return false;
        index += 3U;
        return true;
    }
    if (first == 0xEDU) {
        if (index + 2U >= value.size()) return false;
        const auto second = static_cast<unsigned char>(value[index + 1U]);
        const auto third = static_cast<unsigned char>(value[index + 2U]);
        if (second < 0x80U || second > 0x9FU || !continuation(third)) return false;
        index += 3U;
        return true;
    }
    if (first == 0xF0U) {
        if (index + 3U >= value.size()) return false;
        const auto second = static_cast<unsigned char>(value[index + 1U]);
        if (second < 0x90U || second > 0xBFU ||
            !continuation(static_cast<unsigned char>(value[index + 2U])) ||
            !continuation(static_cast<unsigned char>(value[index + 3U]))) return false;
        index += 4U;
        return true;
    }
    if (first >= 0xF1U && first <= 0xF3U) {
        if (index + 3U >= value.size() ||
            !continuation(static_cast<unsigned char>(value[index + 1U])) ||
            !continuation(static_cast<unsigned char>(value[index + 2U])) ||
            !continuation(static_cast<unsigned char>(value[index + 3U]))) return false;
        index += 4U;
        return true;
    }
    if (first == 0xF4U) {
        if (index + 3U >= value.size()) return false;
        const auto second = static_cast<unsigned char>(value[index + 1U]);
        if (second < 0x80U || second > 0x8FU ||
            !continuation(static_cast<unsigned char>(value[index + 2U])) ||
            !continuation(static_cast<unsigned char>(value[index + 3U]))) return false;
        index += 4U;
        return true;
    }
    return false;
}

} // namespace

bool isValid(const std::string_view value) noexcept
{
    std::size_t index{};
    while (index < value.size()) {
        if (!consumeScalar(value, index)) return false;
    }
    return true;
}

std::size_t scalarCount(const std::string_view value) noexcept
{
    std::size_t index{};
    std::size_t count{};
    while (index < value.size()) {
        if (!consumeScalar(value, index)) return 0U;
        ++count;
    }
    return count;
}

} // namespace farcaster::foundation::utf8
