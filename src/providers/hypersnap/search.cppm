module;

#include <cstddef>
#include <string_view>

export module farcaster.provider.hypersnap.search;

import farcaster.capability.search;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::search {

/** @brief Blocking Hypersnap v2 derived-search adapter exposed only to composition code. */
class SearchSource final : public capability::search::ISearchSource {
public:
    /** @brief Borrows the transport and owns provider configuration. */
    SearchSource(transport::http::IHttpTransport& transport, config::Config config);

    /** @brief Searches the self-hostable Hypersnap user/channel indexes. */
    [[nodiscard]] foundation::result::Result<capability::search::SearchPage>
    search(std::string_view query, std::size_t limit) override;

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

}
