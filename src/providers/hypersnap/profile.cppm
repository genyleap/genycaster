export module farcaster.provider.hypersnap.profile;

import farcaster.capability.profile;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::profile {

/** @brief Blocking Hypersnap v2 profile adapter exposed only to composition code. */
class ProfileSource final : public capability::profile::IProfileSource {
public:
    /** @brief Borrows the transport and owns provider configuration. */
    ProfileSource(transport::http::IHttpTransport& transport, config::Config config);

    /** @brief Reads one canonical profile. */
    [[nodiscard]] foundation::result::Result<domain::profile::Profile>
    get(domain::identity::Fid fid) override;

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

}
