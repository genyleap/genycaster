/*!
 * @file        src/routing/read_gateway.cppm
 * @brief       Capability-routed blocking reads.
 *
 * @details
 * Binds eligible providers to canonical read operations under the active
 * routing and health policy.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <string_view>
#include <vector>

export module farcaster.routing.read.gateway;

import farcaster.application.read.gateway;
import farcaster.application.read.model;
import farcaster.capability.cast;
import farcaster.capability.feed;
import farcaster.capability.identity;
import farcaster.capability.profile;
import farcaster.capability.search;
import farcaster.capability.notification;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.router;

export namespace farcaster::routing::read::gateway {

/**
 * @brief Borrowed provider-neutral capability sources for one registered provider.
 *
 * Every non-null source must outlive the routed gateway and all in-flight jobs.
 */
struct SourceBinding final {
    provider::model::ProviderId providerId{};
    capability::identity::IIdentityResolver* identity{};
    capability::profile::IProfileSource* profile{};
    capability::cast::ICastSource* cast{};
    capability::feed::IFeedSource* feed{};
    capability::search::ISearchSource* search{};
    capability::notification::INotificationSource* notifications{};
    capability::social::ISocialGraphSource* social{};
};

/** @brief Routes every read operation independently, then invokes its bound source. */
class RoutedReadGateway final : public application::read::gateway::IReadGateway {
public:
    /**
     * @brief Creates a gateway over a borrowed router and source bindings.
     * @param router Router that must outlive this gateway.
     * @param health Mutable per-capability health registry that must outlive this gateway.
     * @param bindings Borrowed source bindings copied by value.
     */
    RoutedReadGateway(
        const router::CapabilityRouter& router,
        health::HealthRegistry& health,
        std::vector<SourceBinding> bindings) noexcept;

    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<domain::identity::Fid>
    resolveIdentity(
        std::string_view username,
        application::read::model::ReadOptions options = {}) override;
    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<domain::profile::Profile>
    profile(
        domain::identity::Fid fid,
        application::read::model::ReadOptions options = {}) override;
    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<domain::cast::Cast>
    cast(
        const domain::cast::CastId& castId,
        application::read::model::ReadOptions options = {}) override;
    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage>
    feed(
        const application::read::model::FeedQuery& query,
        application::read::model::ReadOptions options = {}) override;
    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<capability::search::SearchPage>
    search(
        const application::read::model::SearchQuery& query,
        application::read::model::ReadOptions options = {}) override;
    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<capability::notification::NotificationPage>
    notifications(
        const application::read::model::NotificationQuery& query,
        application::read::model::ReadOptions options = {}) override;
    /** @inheritdoc */
    [[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage>
    social(
        const application::read::model::SocialQuery& query,
        application::read::model::ReadOptions options = {}) override;

private:
    [[nodiscard]] foundation::result::Result<const SourceBinding*>
    bindingFor(const provider::model::ProviderId& providerId) const;

    const router::CapabilityRouter& m_router;
    health::HealthRegistry& m_health;
    std::vector<SourceBinding> m_bindings{};
};

}
