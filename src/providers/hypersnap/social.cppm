/*!
 * @file        src/providers/hypersnap/social.cppm
 * @brief       HyperSnap social graph source.
 *
 * @details
 * Reads follow relationships with provider-owned pagination and canonical
 * graph conversion.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

export module farcaster.provider.hypersnap.social;

import farcaster.capability.pagination;
import farcaster.capability.social;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.storage.continuation;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::social {

/** @brief Blocking Hypersnap v2 social-graph adapter with provider-private cursor isolation. */
class SocialGraphSource final : public capability::social::ISocialGraphSource {
public:
    /** @brief Borrows transport/continuation storage and owns provider configuration. */
    SocialGraphSource(
        transport::http::IHttpTransport& transport,
        storage::continuation::ContinuationStore& continuations,
        config::Config config);

    [[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage> followers(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage> following(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    /** @brief Returns whether source currently follows target using the canonical Link edge. */
    [[nodiscard]] foundation::result::Result<bool> isFollowing(
        domain::identity::Fid source,
        domain::identity::Fid target);

private:
    transport::http::IHttpTransport& m_transport;
    storage::continuation::ContinuationStore& m_continuations;
    config::Config m_config{};
};

}
