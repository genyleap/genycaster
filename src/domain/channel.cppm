/*!
 * @file        src/domain/channel.cppm
 * @brief       Canonical channel identity and references.
 *
 * @details
 * Defines stable channel identifiers for protocol-neutral navigation and
 * cast presentation.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <compare>
#include <string>

export module farcaster.domain.channel;

export namespace farcaster::domain::channel {

/** @brief Stable channel identifier. */
struct ChannelId final {
    std::string value{};
};

/** @brief Minimal provider-neutral channel reference. */
struct ChannelRef final {
    ChannelId id{};
    std::string name{};
};

}
