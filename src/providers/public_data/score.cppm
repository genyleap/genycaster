/*!
 * @file        src/providers/public_data/score.cppm
 * @brief       Derived profile score data adapter.
 *
 * @details
 * Adapts optional external profile-quality observations into normalized
 * non-canonical scores.
 *
 * @author      Kambiz Asadzadeh
 * @since       02 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

export module farcaster.provider.public_data.score;

import farcaster.capability.profile_quality;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.provider.public_data.config;
import farcaster.transport.http;

export namespace farcaster::provider::public_data::score {

/** @brief Neynar Base onchain score-reader adapter used as a profile-score fallback. */
class ProfileQualitySource final : public capability::profile_quality::IProfileQualitySource {
public:
    ProfileQualitySource(transport::http::IHttpTransport& transport, config::Config config);

    [[nodiscard]] foundation::result::Result<capability::profile_quality::ProfileQualityScore>
    get(domain::identity::Fid fid) override;

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

}
