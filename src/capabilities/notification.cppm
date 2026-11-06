/*!
 * @file        src/capabilities/notification.cppm
 * @brief       Canonical notification read capability.
 *
 * @details
 * Defines notification kinds, pages and provider-neutral event snapshots
 * for the client's notification surface.
 *
 * @author      Kambiz Asadzadeh
 * @since       03 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.capability.notification;

import farcaster.capability.pagination;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.foundation.time;

export namespace farcaster::capability::notification {

/** @brief Provider-neutral in-app Farcaster notification category. */
enum class NotificationKind : std::uint8_t {
    Mention,
    Reply,
    Reaction,
    Recast,
    Quote,
    Follow,
    Unknown,
};

/** @brief Canonical notification row materialized from the Farcaster read API. */
struct NotificationItem final {
    std::string stableId{};
    NotificationKind kind{NotificationKind::Unknown};
    foundation::time::Timestamp observedAt{};
    std::optional<domain::cast::Cast> cast{};

    /** @brief Cast this notification answers (the parent of a reply/mention), when the provider supplied it. */
    std::optional<domain::cast::Cast> parentCast{};

    /** @brief Author profile of the parent cast, when the provider supplied it. */
    std::optional<domain::profile::Profile> parentAuthor{};
    std::vector<domain::profile::Profile> actors{};
    std::size_t aggregateCount{1};
};

/** @brief Canonical paginated notification snapshot. */
struct NotificationPage final {
    std::vector<NotificationItem> items{};
    std::optional<pagination::ContinuationToken> next{};
};

/** @brief Blocking provider-neutral in-app notifications read port. */
class INotificationSource {
public:
    virtual ~INotificationSource() = default;

    /** @brief Reads one notification page for the specified viewer. */
    [[nodiscard]] virtual foundation::result::Result<NotificationPage> notifications(
        domain::identity::Fid viewer,
        const pagination::PageRequest& pageRequest) = 0;
};

}
