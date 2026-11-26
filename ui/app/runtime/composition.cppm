/*!
 * @file        ui/app/runtime/composition.cppm
 * @brief       GenyCaster cross-platform runtime composition.
 *
 * @details
 * Connects the shared application core, provider routes, local state and
 * UI-facing services under the selected build policy.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
import farcaster.capability.auth;
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
import farcaster.routing.signer.gateway;
import farcaster.routing.read.async_gateway;
import farcaster.storage.continuation;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.async_gateway;
#endif

#if FARCASTER_HAS_MEDIA_UPLOAD
import farcaster.application.media.upload;
import farcaster.application.media.lifecycle;
import farcaster.capability.media;
#endif

export namespace genycaster::ui::runtime {

/**
 * @brief User-configured provider instance for a declared adapter contract.
 *
 * Metadata contains only public configuration; secrets never enter the profile.
 * A registered adapter is still required for the protocol contract. Unknown
 * contracts are rejected rather than guessed from hostname or URL shape.
 */
struct WriteProviderProfile final {
    std::string id{};
    std::string displayName{};
    std::string endpoint{};
    std::string readEndpoint{};
    std::string adapterContract{};
    bool selfHosted{false};
};

struct ReadProviderProfile final {
    std::string id{};
    std::string displayName{};
    std::string endpoint{};
    std::string adapterContract{};
    bool selfHosted{false};
};

/**
 * @brief Owned, provider-neutral startup options for a single runtime.
 *
 * The configuration makes Read, Write, signer attestation and authorization
 * policy explicit rather than relying on the positional order of unrelated
 * optional strings. It contains ephemeral API keys only in process memory;
 * it must never be persisted or sent to a QML context property.
 *
 * The selected Read provider does not imply a Write provider. An attestor
 * is configured independently from both Read and Write endpoints.
 */
struct RuntimeConfiguration final {
    std::filesystem::path databasePath{};
    farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy{};
    std::optional<std::string> providerBaseUrl{};

    /** HyperSnap shard-0 identity/write node used by native onboarding.
     * Independent of the indexed reader and Farcaster write provider. */
    std::optional<std::string> hyperSnapNativeNodeBaseUrl{};
    std::optional<std::string> farcasterReadBaseUrl{};
    std::optional<std::string> farcasterReadApiKey{};
    std::vector<ReadProviderProfile> additionalReadProviders{};
    std::unordered_map<std::string, std::string> readProviderApiKeys{};
    std::optional<std::string> preferredReadProviderId{};
    bool allowCompatibleReadFallback{false};
    bool publicReadPool{false};
    std::vector<std::string> publicReadPeerIds{};
    std::vector<WriteProviderProfile> additionalWriteProviders{};
    std::optional<std::string> preferredWriteProviderId{};
    std::optional<std::string> authRelayUrl{};
    std::optional<std::string> optimismRpcUrl{};
    std::optional<std::string> snapchainBaseUrl{};
    std::optional<std::string> snapchainWriteBaseUrl{};
    std::optional<std::string> mediaGatewayBaseUrl{};
    std::optional<std::string> snapchainAuthorizationHeader{};
    std::optional<std::string> snapchainAuthorizationValue{};
    std::optional<std::string> snapchainWriteAuthorizationHeader{};
    std::optional<std::string> snapchainWriteAuthorizationValue{};
    std::optional<std::uint64_t> appFid{};
    std::optional<std::string> signerAttestationUrl{};
    std::function<farcaster::foundation::result::Status(const std::string&)>
        approvalLauncher{};
};

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
    std::string optimismRpcUrl{};
    std::unique_ptr<farcaster::transport::async_http::IAsyncHttpTransport> asyncTransport{};
#if GENYCASTER_HAS_OPENPROOF_AUTH
    std::unique_ptr<farcaster::capability::auth::IAuthenticationProvider> openProofAuthentication{};
#endif
#if FARCASTER_HAS_FARCASTER_AUTH
    std::unique_ptr<farcaster::capability::auth::ICompatibilityAuthentication> farcasterAuthentication{};
#endif
    std::unique_ptr<farcaster::application::session::SessionService> sessionService{};
    std::unique_ptr<farcaster::execution::serial::SerialExecutor> signerExecutor{};
    std::unique_ptr<farcaster::local::signer::ISignerMetadataStore> signerMetadataStore{};
    std::unique_ptr<farcaster::capability::signer::ISignerVault> signerVault{};
    std::mutex cloudDeviceVaultMutex{};
    std::unique_ptr<farcaster::capability::signer::ISignerVault> farcasterCloudVault{};
    std::unique_ptr<farcaster::capability::signer::ISignerVault> hypersnapCloudVault{};
    std::unique_ptr<farcaster::routing::signer::RoutedSignerRegistry> signerRegistry{};
    std::vector<std::unique_ptr<farcaster::capability::signer::ISignerRegistry>> signerRegistrySources{};
    std::unique_ptr<farcaster::capability::signer::ICustodyAuthorizer> custodyAuthorizer{};
    std::unique_ptr<farcaster::capability::signer_attestation::ISignedKeyRequestAttestor> signerAttestor{};
    std::unique_ptr<farcaster::application::signer::SignerService> signerService{};
    std::unique_ptr<farcaster::local::write::IOutboxStore> writeOutbox{};
    std::unique_ptr<farcaster::capability::write::ICanonicalWriteCodec> writeCodec{};
    std::unique_ptr<farcaster::capability::write::IWriteSubmitter> writeSubmitter{};
    std::unique_ptr<farcaster::local::write::IWriteRouteBindings> writeRouteBindings{};
    std::vector<std::unique_ptr<farcaster::capability::write::IWriteSubmitter>> writeSources{};
    std::vector<std::string> writeSourceIds{};
    std::vector<std::string> writeSourceEndpointIdentities{};
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
    std::uint64_t viewerOperationGeneration{0};
    // Changes only when the owning surface/account becomes invalid, not on each batch.
    std::uint64_t viewerSurfaceGeneration{0};
    std::uint64_t activeViewerFid{0};
    // A separate version for each confirmed field prevents out-of-order GUI deliveries
    // from undoing newer Like/Recast evidence for the same canonical cast.
    std::unordered_map<std::string, std::uint64_t> viewerReactionDeliveredGeneration{};
    bool viewerReactionRegistering{false};
    std::vector<std::string> activeViewerTargetIds{};
    // Demand arriving during a shared account acquisition is queued, not
    // allowed to cancel a provider operation serving other visible casts.
    std::vector<farcaster::domain::cast::CastReference> deferredViewerTargets{};
    // Active QML consumers share one canonical cast demand, independent of
    // the viewport row object, and release it on pooling/destruction.
    std::unordered_map<std::string, std::size_t> viewerReactionConsumerCounts{};
    // Per-viewer/cast freshness; only complete positive/negative evidence is
    // cached. Protected by viewerOperationMutex.
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        viewerReactionFreshUntil{};
    std::optional<farcaster::application::read::async_gateway::Request> activeViewerReactionRequest{};
    std::mutex profileRelationshipMutex{};
    std::unordered_map<std::string, bool> followsMeCache{};
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> followsMeObservedAt{};
    std::unordered_set<std::string> followsMeInFlight{};
    std::unordered_map<std::string, farcaster::application::read::async_gateway::Request> followsMeRequests{};
    farcaster::routing::registry::CapabilityRegistry registry{};
    farcaster::routing::health::HealthRegistry health{};
    std::unique_ptr<farcaster::routing::router::CapabilityRouter> router{};
    // Provider instance identifiers are supplied by adapter registration.
    // Presentation may select them without knowing adapter/vendor names.
    std::string independentReadProviderId{};
    std::string hostedReadProviderId{};
    std::string requestedReadProviderId{};

    /** @brief Explicit consent for cross-provider Read fallback, never Write. */
    bool allowCompatibleReadFallback{false};
    std::optional<farcaster::storage::continuation::ContinuationStore> continuations{};
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    std::unique_ptr<farcaster::provider::hypersnap::async_gateway::AsyncGateway> asyncGateway{};
    std::unique_ptr<farcaster::provider::hypersnap::async_gateway::AsyncGateway> farcasterReadGateway{};
    std::vector<std::unique_ptr<farcaster::provider::hypersnap::async_gateway::AsyncGateway>>
        additionalReadGateways{};
    std::vector<std::string> additionalReadProviderIds{};
#endif
    std::vector<std::unique_ptr<farcaster::application::read::async_gateway::IAsyncReadGateway>> rawReadGateways{};
    std::vector<std::string> rawReadProviderIds{};
    std::unique_ptr<farcaster::routing::read::async_gateway::RoutedReadGateway> routedAsyncGateway{};
    std::unique_ptr<farcaster::application::read::async_service::ReadService> service{};
    std::mutex requestMutex{};
    std::atomic<std::uint64_t> primaryRequestGeneration{0};
    std::atomic<std::uint64_t> feedRequestGeneration{0};
    // Page cancellation and navigation are separate lifetimes.
    std::atomic<std::uint64_t> feedSurfaceGeneration{0};
    std::mutex feedSurfaceMutex{};
    std::string feedSurfaceIdentity{};
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
    std::optional<farcaster::application::read::async_service::ReadRequest> activeAccountProfileRequest{};
    std::optional<farcaster::application::read::async_gateway::Request> activeAccountIndexedRequest{};
    std::atomic<std::uint64_t> accountProfileRequestGeneration{0};
    std::optional<farcaster::application::read::async_service::ReadRequest> activeFollowingStoriesRequest{};
    std::optional<farcaster::application::read::async_service::ReadRequest> activeFollowingActivityRequest{};

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

    /** @brief Switches feed identity, optionally retaining independent profile-header work.
     * @param preserveProfileRequest True for tabs within the same profile navigation.
     *        The owning profile request remains live; feed/preview/reaction work is replaced.
     * @return True when this operation changed the active feed identity.
     */
    [[nodiscard]] bool selectFeedSurfaceIdentity(
        const std::string& identity, bool preserveProfileRequest = false);
    [[nodiscard]] bool isCurrentFeedSurfaceGeneration(std::uint64_t generation) const noexcept;
    [[nodiscard]] std::uint64_t beginExplorerArtRequestGeneration();
    [[nodiscard]] bool isCurrentExplorerArtRequestGeneration(std::uint64_t generation) const noexcept;
    void keepExplorerArtRequest(farcaster::application::read::async_service::ReadRequest request);
    void keepFollowingStoriesRequest(farcaster::application::read::async_service::ReadRequest request);
    void keepFollowingActivityRequest(farcaster::application::read::async_service::ReadRequest request);
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
    void cancelAccountProfileRequest();
    void keepAccountProfileRequest(farcaster::application::read::async_service::ReadRequest request);
    void keepAccountIndexedRequest(farcaster::application::read::async_gateway::Request request);
};

/**
 * @brief Builds the complete outer runtime from provider-neutral stores, routing policy and enabled adapters.
 * @return A fully owned runtime, or a recoverable setup error before the GUI is exposed.
 */
[[nodiscard]] farcaster::foundation::result::Result<std::unique_ptr<Runtime>> makeRuntime(
    const std::filesystem::path& databasePath,
    farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy,
    const std::optional<std::string>& providerBaseUrl,
    const std::optional<std::string>& hyperSnapNativeNodeBaseUrl,
    const std::optional<std::string>& farcasterReadBaseUrl,
    const std::optional<std::string>& farcasterReadApiKey,
    const std::vector<ReadProviderProfile>& additionalReadProviders,
    const std::unordered_map<std::string, std::string>& readProviderApiKeys,
    const std::optional<std::string>& preferredReadProviderId,
    bool allowCompatibleReadFallback,
    const std::vector<WriteProviderProfile>& additionalWriteProviders,
    const std::optional<std::string>& preferredWriteProviderId,
    const std::optional<std::string>& authRelayUrl,
    const std::optional<std::string>& optimismRpcUrl,
    const std::optional<std::string>& snapchainBaseUrl,
    const std::optional<std::string>& snapchainWriteBaseUrl,
    const std::optional<std::string>& mediaGatewayBaseUrl,
    const std::optional<std::string>& snapchainAuthorizationHeader,
    const std::optional<std::string>& snapchainAuthorizationValue,
    const std::optional<std::string>& snapchainWriteAuthorizationHeader,
    const std::optional<std::string>& snapchainWriteAuthorizationValue,
    const std::optional<std::uint64_t> appFid,
    const std::optional<std::string>& signerAttestationUrl,
    std::function<farcaster::foundation::result::Status(const std::string&)> approvalLauncher);

/**
 * @brief Build the runtime from a named, owned set of startup options.
 *
 * @param configuration Single-use policy snapshot assembled before the worker
 *        begins, including ephemeral Read/Write credentials.
 * @return The same result and signer security guarantees as the underlying
 *         provider-neutral runtime factory.
 *
 * This overload forwards to the existing validated factory without changing
 * protocol routing, signer approval or runtime ownership.
 */
[[nodiscard]] farcaster::foundation::result::Result<std::unique_ptr<Runtime>> makeRuntime(
    RuntimeConfiguration configuration);

} // namespace genycaster::ui::runtime
