module;

#include <cstddef>
#include <string_view>

export module farcaster.foundation.utf8;

export namespace farcaster::foundation::utf8 {

/** @brief Validates canonical Unicode scalar encoding in UTF-8. */
[[nodiscard]] bool isValid(std::string_view value) noexcept;

/** @brief Counts Unicode scalar values in valid UTF-8, returning zero for invalid input. */
[[nodiscard]] std::size_t scalarCount(std::string_view value) noexcept;

} // namespace farcaster::foundation::utf8
