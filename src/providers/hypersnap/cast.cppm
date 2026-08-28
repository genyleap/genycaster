export module farcaster.provider.hypersnap.cast;

import farcaster.capability.cast;
import farcaster.domain.cast;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::cast {

/** @brief Blocking Hypersnap v2 cast adapter exposed only to composition code. */
class CastSource final : public capability::cast::ICastSource {
public:
    /** @brief Borrows the transport and owns provider configuration. */
    CastSource(transport::http::IHttpTransport& transport, config::Config config);

    /** @brief Reads one canonical cast by hash-backed canonical id. */
    [[nodiscard]] foundation::result::Result<domain::cast::Cast>
    get(const domain::cast::CastId& castId) override;

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

}
