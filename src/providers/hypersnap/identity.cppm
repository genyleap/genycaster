module;

#include <string_view>

export module farcaster.provider.hypersnap.identity;

import farcaster.capability.identity;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::identity {

/** @brief Blocking Hypersnap v2 username-to-FID adapter. */
class IdentityResolver final : public capability::identity::IIdentityResolver {
public:
    /** @brief Borrows the transport and owns provider configuration. */
    IdentityResolver(transport::http::IHttpTransport& transport, config::Config config);

    /** @brief Resolves a Farcaster username to its canonical FID. */
    [[nodiscard]] foundation::result::Result<domain::identity::Fid>
    resolveUsername(std::string_view username) override;

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

}
