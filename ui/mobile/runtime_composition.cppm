module;

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module genycaster.ui.runtime;

import farcaster.application.read.async_gateway;
import farcaster.application.read.async_service;
import farcaster.application.session;
import farcaster.application.signer;
import farcaster.application.write;
import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.capability.write;
import farcaster.capability.write_submit;
import farcaster.domain.cast;
import farcaster.domain.session;
import farcaster.execution.serial;
import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.read;
import farcaster.local.session;
import farcaster.local.signer;
import farcaster.local.write;
import farcaster.routing.health;
import farcaster.routing.registry;
import farcaster.routing.router;
import farcaster.routing.read.async_gateway;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;

#if FARCASTER_HAS_FARCASTER_AUTH
import farcaster.capability.auth;
#endif

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.async_gateway;
import farcaster.storage.continuation;
#endif

#if FARCASTER_HAS_MEDIA_UPLOAD
import farcaster.application.media.upload;
import farcaster.application.media.lifecycle;
import farcaster.capability.media;
#endif

export namespace genycaster::ui::runtime {

/**
 * @brief Owns the outer runtime composition used by the Qt application shell.
 *
 * The type centralizes executor, storage, provider, routing, signer, write and
 * request-lifecycle ownership so `main.cpp` remains a composition root rather
 * than a second runtime subsystem. All asynchronous request handles are
 * cancelled before their borrowed providers and stores are destroyed.
 */
struct Runtime final {
    std::unique_ptr<farcaster::execution::serial::SerialExecutor> cacheExecutor{};
    std::unique_ptr<farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor> decodeExecutor{};
    std::unique_ptr<farcaster::execution::serial::SerialExecutor> executor{};
    std::unique_ptr<farcaster::local::read::IReadStore> store{};
    std::unique_ptr<farcaster::local::session::IClientSessionStore> sessionStore{};
    std::unique_ptr<farcaster::transport::http::IHttpTransport> transport{};
    std::unique_ptr<farcaster::transport::async_http::IAsyncHttpTransport> asyncTransport{};
#if FARCASTER_HAS_FARCASTER_AUTH
    std::unique_ptr<farcaster::capability::auth::ICompatibilityAuthentication> authentication{};
#endif
    std::unique_ptr<farcaster::application::session::SessionService> sessionService{};
    std::unique_ptr<farcaster::execution::serial::SerialExecutor> signerExecutor{};
    std::unique_ptr<farcaster::local::signer::ISignerMetadataStore> signerMetadataStore{};
    std::unique_ptr<farcaster::capability::signer::ISignerVault> signerVault{};
    std::unique_ptr<farcaster::capability::signer::ISignerRegistry> signerRegistry{};
    std::unique_ptr<farcaster::capability::signer::ICustodyAuthorizer> custodyAuthorizer{};
    std::unique_ptr<farcaster::capability::signer_attestation::ISignedKeyRequestAttestor> signerAttestor{};
    std::unique_ptr<farcaster::application::signer::SignerService> signerService{};
    std::unique_ptr<farcaster::local::write::IOutboxStore> writeOutbox{};
    std::unique_ptr<farcaster::capability::write::ICanonicalWriteCodec> writeCodec{};
    std::unique_ptr<farcaster::capability::write::IWriteSubmitter> writeSubmitter{};
    std::unique_ptr<farcaster::application::write::WriteService> writeService{};
#if FARCASTER_HAS_MEDIA_UPLOAD
    std::unique_ptr<farcaster::execution::serial::SerialExecutor> mediaExecutor{};
    std::unique_ptr<farcaster::capability::media::IMediaUploadGateway> mediaGateway{};
    std::unique_ptr<farcaster::capability::media::IMediaAssetLifecycleGateway> mediaLifecycleGateway{};
    std::unique_ptr<farcaster::application::media::upload::MediaUploadService> mediaUploadService{};
    std::unique_ptr<farcaster::application::media::lifecycle::MediaLifecycleService> mediaLifecycleService{};
    std::mutex mediaUploadMutex{};
    std::mutex mediaCleanupMutex{};
    std::unordered_set<std::string> mediaCleanupInFlight{};
    std::optional<farcaster::application::media::upload::UploadHandle> activeMediaUpload{};
    std::optional<farcaster::foundation::error::Error> mediaSetupError{};
#endif
    std::optional<farcaster::foundation::error::Error> signerSetupError{};
    std::optional<farcaster::foundation::error::Error> writeSetupError{};
    std::mutex sessionMutex{};
    std::optional<farcaster::domain::session::ClientSession> currentSession{};
    std::atomic_bool authenticationPollInFlight{false};
    std::atomic_bool viewerStateRefreshInFlight{false};
    std::atomic<std::int64_t> viewerStateLastRefreshMs{0};
    std::mutex viewerOperationMutex{};
    std::optional<farcaster::application::read::async_gateway::Request> activeViewerReactionRequest{};
    std::mutex profileRelationshipMutex{};
    std::unordered_map<std::string, bool> followsMeCache{};
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> followsMeObservedAt{};
    std::unordered_set<std::string> followsMeInFlight{};
    std::unordered_map<std::string, farcaster::application::read::async_gateway::Request> followsMeRequests{};
    farcaster::routing::registry::CapabilityRegistry registry{};
    farcaster::routing::health::HealthRegistry health{};
    std::unique_ptr<farcaster::routing::router::CapabilityRouter> router{};
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    std::optional<farcaster::storage::continuation::ContinuationStore> continuations{};
    std::unique_ptr<farcaster::provider::hypersnap::async_gateway::AsyncGateway> asyncGateway{};
    std::unique_ptr<farcaster::routing::read::async_gateway::RoutedReadGateway> routedAsyncGateway{};
#endif
    std::unique_ptr<farcaster::application::read::async_service::ReadService> service{};
    std::mutex requestMutex{};
    std::atomic<std::uint64_t> primaryRequestGeneration{0};
    std::atomic<std::uint64_t> feedRequestGeneration{0};
    std::atomic<std::uint64_t> explorerArtRequestGeneration{0};
    std::mutex explorerArtMutex{};
    std::vector<farcaster::application::read::async_service::ReadRequest> explorerArtRequests{};
    std::mutex referencedPreviewMutex{};
    std::unordered_set<std::string> referencedPreviewInFlight{};
    std::unordered_set<std::string> referencedPreviewResolved{};
    std::unordered_map<std::string, farcaster::application::read::async_service::ReadRequest> referencedPreviewRequests{};
    std::optional<farcaster::application::read::async_service::ReadRequest> activePrimaryRequest{};
    std::optional<farcaster::application::read::async_service::ReadRequest> activeFeedRequest{};
    std::optional<farcaster::application::read::async_service::ReadRequest> activeSearchRequest{};
    std::optional<farcaster::application::read::async_service::ReadRequest> activeNotificationBadgeRequest{};

    Runtime() = default;
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;
    ~Runtime();

    void setCurrentSession(std::optional<farcaster::domain::session::ClientSession> session);
    [[nodiscard]] std::optional<farcaster::domain::session::ClientSession> currentSessionSnapshot();
    void cancelPrimaryRequest();
    [[nodiscard]] std::uint64_t beginPrimaryRequestGeneration() noexcept;
    [[nodiscard]] bool isCurrentPrimaryRequestGeneration(std::uint64_t generation) const noexcept;
    [[nodiscard]] std::uint64_t beginFeedRequestGeneration() noexcept;
    [[nodiscard]] bool isCurrentFeedRequestGeneration(std::uint64_t generation) const noexcept;
    [[nodiscard]] std::uint64_t beginExplorerArtRequestGeneration();
    [[nodiscard]] bool isCurrentExplorerArtRequestGeneration(std::uint64_t generation) const noexcept;
    void keepExplorerArtRequest(farcaster::application::read::async_service::ReadRequest request);
    void cancelExplorerArtRequests();
    void cancelViewerReactionRequest();
    void cancelReferencedPreviewRequests();
    void cancelFeedRequest();
    void cancelSearchRequest();
    void cancelAllRequests();
    [[nodiscard]] bool beginReferencedPreviewRequest(const farcaster::domain::cast::CastId& castId);
    void keepReferencedPreviewRequest(
        const farcaster::domain::cast::CastId& castId,
        farcaster::application::read::async_service::ReadRequest request);
    void finishReferencedPreviewRequest(const farcaster::domain::cast::CastId& castId, bool resolved);
    void keepPrimaryRequest(farcaster::application::read::async_service::ReadRequest request);
    void keepFeedRequest(farcaster::application::read::async_service::ReadRequest request);
    void keepSearchRequest(farcaster::application::read::async_service::ReadRequest request);
    void keepNotificationBadgeRequest(farcaster::application::read::async_service::ReadRequest request);
};

/**
 * @brief Builds the complete outer runtime from provider-neutral stores, routing policy and enabled adapters.
 * @return A fully owned runtime, or a recoverable setup error before the GUI is exposed.
 */
[[nodiscard]] farcaster::foundation::result::Result<std::unique_ptr<Runtime>> makeRuntime(
    const std::filesystem::path& databasePath,
    farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy,
    const std::optional<std::string>& providerBaseUrl,
    const std::optional<std::string>& authRelayUrl,
    const std::optional<std::string>& optimismRpcUrl,
    const std::optional<std::string>& snapchainBaseUrl,
    const std::optional<std::string>& mediaGatewayBaseUrl,
    const std::optional<std::string>& snapchainAuthorizationHeader,
    const std::optional<std::string>& snapchainAuthorizationValue,
    const std::optional<std::uint64_t> appFid,
    const std::optional<std::string>& signerAttestationUrl,
    std::function<farcaster::foundation::result::Status(const std::string&)> approvalLauncher);

} // namespace genycaster::ui::runtime
