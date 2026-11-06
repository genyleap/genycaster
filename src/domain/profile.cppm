/*!
 * @file        src/domain/profile.cppm
 * @brief       Canonical Farcaster profile snapshot.
 *
 * @details
 * Represents normalized profile data and verified addresses without
 * introducing provider response types.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.domain.profile;

import farcaster.domain.identity;

export namespace farcaster::domain::profile {

/**
 * @brief Provider-neutral canonical Farcaster profile snapshot.
 */
struct Profile final {
    identity::Fid fid{};
    std::optional<identity::Username> username{};
    std::optional<identity::DisplayName> displayName{};
    std::string bio{};
    std::optional<std::string> avatarUrl{};
    std::optional<std::string> bannerUrl{};
    std::optional<std::string> location{};
    std::optional<std::string> profileUrl{};
    std::optional<std::string> custodyAddress{};

    /** @brief Farcaster-verified EVM addresses associated with this profile. */
    std::vector<std::string> verifiedEthAddresses{};

    /** @brief Farcaster-verified Solana addresses associated with this profile. */
    std::vector<std::string> verifiedSolAddresses{};
    std::uint64_t followerCount{};
    std::uint64_t followingCount{};

    /** @brief True only when social totals were supplied by an authoritative profile source.
     * A raw protocol UserData response does not include social totals. */
    bool socialCountsKnown{true};

    /** @brief Provider-derived account quality score normalized to [0, 1], when available. */
    std::optional<double> qualityScore{};
};

}
