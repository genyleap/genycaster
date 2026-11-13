module;

#include <algorithm>
#include <atomic>
#include <cctype>
#include <expected>
#include <functional>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

module genycaster.ui.runtime;

import farcaster.application.configuration;
import farcaster.application.read.async_gateway;
import farcaster.application.read.async_service;
import farcaster.application.session;
import farcaster.application.signer;
import farcaster.application.write;
import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.domain.identity;
import farcaster.execution.serial;
import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.read;
import farcaster.local.session;
import farcaster.local.signer;
import farcaster.local.write;
import farcaster.provider.model;
import farcaster.routing.policy;
import farcaster.routing.read.async_gateway;
import farcaster.routing.write.gateway;
import farcaster.storage.read;
import farcaster.storage.session;
import farcaster.storage.signer;
import farcaster.transport.qtnetwork;
import farcaster.transport.qtnetwork.async;

#if GENYCASTER_HAS_OPENPROOF_AUTH
import farcaster.provider.identity.openproof;
#endif

#if FARCASTER_HAS_FARCASTER_AUTH
import farcaster.provider.compatibility.auth;
#endif

#if FARCASTER_HAS_PROTOCOL_SIGNING
import farcaster.platform.signer_vault;
import farcaster.protocol.write;
import farcaster.storage.write;
#endif

#if FARCASTER_HAS_SNAPCHAIN_SIGNER
import farcaster.provider.snapchain.signer;
#endif

#if FARCASTER_HAS_SNAPCHAIN_WRITE
import farcaster.provider.snapchain.write;
#endif

#if FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER
import farcaster.provider.compatibility.browser_wallet.authorizer;
#endif

#if FARCASTER_HAS_SIGNER_ATTESTATION
import farcaster.provider.compatibility.signer_attestation;
#endif

#if FARCASTER_HAS_MEDIA_UPLOAD
import farcaster.application.media.upload;
import farcaster.application.media.lifecycle;
import farcaster.transport.qtnetwork.media;
#endif

#if FARCASTER_HAS_SNAPCHAIN_READ
import farcaster.provider.snapchain.read;
#endif

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.registration;
import farcaster.storage.continuation;
#endif

namespace genycaster::ui::runtime {

namespace application = farcaster::application;
namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace routing = farcaster::routing;

Runtime::~Runtime()
{
    cancelAllRequests();
    cancelAccountProfileRequest();
#if FARCASTER_HAS_MEDIA_UPLOAD
    {
        std::scoped_lock lock{mediaUploadMutex};
        if (activeMediaUpload) {
            activeMediaUpload->cancel();
        }
    }
    mediaExecutor.reset();
    mediaLifecycleService.reset();
    mediaUploadService.reset();
    mediaLifecycleGateway.reset();
    mediaGateway.reset();
#endif
    {
        std::scoped_lock lock{profileRelationshipMutex};
        for (const auto& [key, request] : followsMeRequests) {
            static_cast<void>(key);
            request.cancel();
        }
        followsMeRequests.clear();
        followsMeInFlight.clear();
    }
    cancelExplorerArtRequests();
    signerExecutor.reset();
    writeService.reset();
    signerService.reset();
    // Join the network reactor while every asynchronous observer's service,
    // gateway and cache executor still exists. Its shutdown discards pending
    // completions instead of dispatching into half-destroyed application state.
    // This ownership order applies to desktop, mobile and single-thread WASM.
    asyncTransport.reset();
    // Drain in-flight decode and cache jobs before their borrowed ReadService
    // and storage dependencies are released. A canceled job may still be
    // completing on another worker after its UI surface has disappeared.
    decodeExecutor.reset();
    cacheExecutor.reset();
    service.reset();
    routedAsyncGateway.reset();
    rawReadGateways.clear();
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    farcasterReadGateway.reset();
    asyncGateway.reset();
#endif
    executor.reset();
}

void Runtime::setCurrentSession(std::optional<domain::session::ClientSession> session)
{
    bool accountChanged{false};
    {
        std::scoped_lock lock{sessionMutex};
        const std::uint64_t previous = currentSession ? currentSession->fid.value : 0U;
        const std::uint64_t next = session ? session->fid.value : 0U;
        accountChanged = previous != next;
        currentSession = std::move(session);
    }
    if (accountChanged) {
        // Authentication-dependent callbacks and caches cannot cross viewers.
        // Invalidate outside sessionMutex to preserve a deterministic lock order.
        cancelAllRequests();
        std::scoped_lock lock{viewerOperationMutex};
        viewerReactionFreshUntil.clear();
        activeViewerFid = 0;
    }
}

std::optional<domain::session::ClientSession> Runtime::currentSessionSnapshot()
{
    std::scoped_lock lock{sessionMutex};
    return currentSession;
}

void Runtime::cancelPrimaryRequest()
{
    primaryRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    std::optional<application::read::async_service::ReadRequest> obsolete{};
    {
        std::scoped_lock lock{requestMutex};
        obsolete = std::move(activePrimaryRequest);
        activePrimaryRequest.reset();
    }
    if (obsolete) obsolete->cancel();
}

std::uint64_t Runtime::beginPrimaryRequestGeneration() noexcept
{
    return primaryRequestGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
}

bool Runtime::isCurrentPrimaryRequestGeneration(const std::uint64_t generation) const noexcept
{
    return primaryRequestGeneration.load(std::memory_order_acquire) == generation;
}

std::uint64_t Runtime::beginFeedRequestGeneration() noexcept
{
    return feedRequestGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
}

bool Runtime::isCurrentFeedRequestGeneration(const std::uint64_t generation) const noexcept
{
    return feedRequestGeneration.load(std::memory_order_acquire) == generation;
}

bool Runtime::selectFeedSurfaceIdentity(
    const std::string& identity, const bool preserveProfileRequest)
{
    {
        std::scoped_lock lock{feedSurfaceMutex};
        if (feedSurfaceIdentity == identity) {
            return false;
        }
    }
    if (preserveProfileRequest) {
        // Profile navigation begins its metadata read before it starts its
        // independently paged feed. Switching the latter must never cancel
        // the just-started metadata request or advance its primary generation.
        // Leaving the profile still uses cancelAllRequests() and retires both.
        feedSurfaceGeneration.fetch_add(1U, std::memory_order_acq_rel);
        cancelFeedRequest();
        cancelReferencedPreviewRequests();
        cancelViewerReactionRequest();
    } else {
        // A genuinely different navigation terminates the whole old surface.
        cancelAllRequests();
    }
    {
        std::scoped_lock lock{feedSurfaceMutex};
        feedSurfaceIdentity = identity;
    }
    return true;
}

bool Runtime::isCurrentFeedSurfaceGeneration(const std::uint64_t generation) const noexcept
{
    return feedSurfaceGeneration.load(std::memory_order_acquire) == generation;
}

std::uint64_t Runtime::beginExplorerArtRequestGeneration()
{
    std::vector<application::read::async_service::ReadRequest> requests{};
    {
        std::scoped_lock lock{explorerArtMutex};
        requests = std::move(explorerArtRequests);
        explorerArtRequests.clear();
    }
    for (auto& request : requests) {
        request.cancel();
    }
    return explorerArtRequestGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
}

bool Runtime::isCurrentExplorerArtRequestGeneration(const std::uint64_t generation) const noexcept
{
    return explorerArtRequestGeneration.load(std::memory_order_acquire) == generation;
}

void Runtime::keepExplorerArtRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{explorerArtMutex};
    explorerArtRequests.push_back(std::move(request));
}

void Runtime::keepFollowingStoriesRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    if (activeFollowingStoriesRequest) {
        activeFollowingStoriesRequest->cancel();
    }
    activeFollowingStoriesRequest = std::move(request);
}

void Runtime::keepFollowingActivityRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    if (activeFollowingActivityRequest) {
        activeFollowingActivityRequest->cancel();
    }
    activeFollowingActivityRequest = std::move(request);
}

void Runtime::cancelExplorerArtRequests()
{
    explorerArtRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    std::vector<application::read::async_service::ReadRequest> requests{};
    {
        std::scoped_lock lock{explorerArtMutex};
        requests = std::move(explorerArtRequests);
        explorerArtRequests.clear();
    }
    for (auto& request : requests) {
        request.cancel();
    }
}

void Runtime::cancelViewerReactionRequest()
{
    std::optional<application::read::async_gateway::Request> obsolete{};
    {
        std::scoped_lock lock{viewerOperationMutex};
        ++viewerOperationGeneration;
        ++viewerSurfaceGeneration;
        obsolete = std::move(activeViewerReactionRequest);
        activeViewerReactionRequest.reset();
        viewerReactionRegistering = false;
        activeViewerTargetIds.clear();
        deferredViewerTargets.clear();
        viewerReactionConsumerCounts.clear();
    }
    // Cancel outside the ownership lock: some transports can complete inline.
    if (obsolete) {
        obsolete->cancel();
    }
    // Navigation cancels obsolete interests; a same-viewer verified cache is
    // still usable, while a real account switch clears it explicitly.
}

void Runtime::cancelReferencedPreviewRequests()
{
    std::vector<application::read::async_service::ReadRequest> requests{};
    {
        std::scoped_lock lock{referencedPreviewMutex};
        requests.reserve(referencedPreviewRequests.size());
        for (auto& [castId, request] : referencedPreviewRequests) {
            static_cast<void>(castId);
            requests.push_back(std::move(request));
        }
        referencedPreviewRequests.clear();
        referencedPreviewInFlight.clear();
        referencedPreviewResolved.clear();
    }
    for (auto& request : requests) {
        request.cancel();
    }
}

void Runtime::cancelFeedRequest()
{
    feedRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    std::optional<application::read::async_service::ReadRequest> obsolete{};
    {
        std::scoped_lock lock{requestMutex};
        obsolete = std::move(activeFeedRequest);
        activeFeedRequest.reset();
    }
    if (obsolete) obsolete->cancel();
    // Page requests do not own referenced previews or viewer enrichment.
}

void Runtime::cancelSearchRequest()
{
    std::optional<application::read::async_service::ReadRequest> obsolete{};
    {
        std::scoped_lock lock{requestMutex};
        obsolete = std::move(activeSearchRequest);
        activeSearchRequest.reset();
    }
    if (obsolete) obsolete->cancel();
}

void Runtime::cancelAllRequests()
{
    {
        std::scoped_lock lock{feedSurfaceMutex};
        feedSurfaceIdentity.clear();
    }
    feedSurfaceGeneration.fetch_add(1U, std::memory_order_acq_rel);
    primaryRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    feedRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    std::optional<application::read::async_service::ReadRequest> primary{};
    std::optional<application::read::async_service::ReadRequest> feed{};
    std::optional<application::read::async_service::ReadRequest> search{};
    std::optional<application::read::async_service::ReadRequest> badge{};
    {
        std::scoped_lock lock{requestMutex};
        primary = std::move(activePrimaryRequest);
        feed = std::move(activeFeedRequest);
        search = std::move(activeSearchRequest);
        badge = std::move(activeNotificationBadgeRequest);
        activePrimaryRequest.reset();
        activeFeedRequest.reset();
        activeSearchRequest.reset();
        activeNotificationBadgeRequest.reset();
    }
    // Release ownership locks before cancelling transports: an inline
    // completion may reenter request handling or invalidate a generation.
    if (primary) primary->cancel();
    if (feed) feed->cancel();
    if (search) search->cancel();
    if (badge) badge->cancel();
    cancelReferencedPreviewRequests();
    cancelViewerReactionRequest();
}

bool Runtime::beginReferencedPreviewRequest(const domain::cast::CastId& castId)
{
    if (castId.value.empty()) {
        return false;
    }
    std::scoped_lock lock{referencedPreviewMutex};
    if (referencedPreviewResolved.contains(castId.value) || referencedPreviewInFlight.contains(castId.value)) {
        return false;
    }
    referencedPreviewInFlight.insert(castId.value);
    return true;
}

void Runtime::keepReferencedPreviewRequest(
    const domain::cast::CastId& castId,
    application::read::async_service::ReadRequest request)
{
    bool keep{false};
    {
        std::scoped_lock lock{referencedPreviewMutex};
        // A synchronous cached/fresh terminal callback may have completed
        // before the returned handle is registered.
        keep = referencedPreviewInFlight.contains(castId.value)
            && !referencedPreviewResolved.contains(castId.value);
        if (keep) {
            referencedPreviewInFlight.insert(castId.value);
            referencedPreviewRequests.insert_or_assign(castId.value, std::move(request));
        }
    }
    if (!keep) {
        request.cancel();
    }
}

void Runtime::finishReferencedPreviewRequest(const domain::cast::CastId& castId, const bool resolved)
{
    if (castId.value.empty()) {
        return;
    }
    std::scoped_lock lock{referencedPreviewMutex};
    referencedPreviewRequests.erase(castId.value);
    const bool wasActive = referencedPreviewInFlight.erase(castId.value) > 0U;
    if (resolved && wasActive) {
        referencedPreviewResolved.insert(castId.value);
    }
}

void Runtime::keepPrimaryRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    activePrimaryRequest = std::move(request);
}

void Runtime::keepFeedRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    activeFeedRequest = std::move(request);
}

void Runtime::keepSearchRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    if (activeSearchRequest) {
        activeSearchRequest->cancel();
    }
    activeSearchRequest = std::move(request);
}

void Runtime::keepNotificationBadgeRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    if (activeNotificationBadgeRequest) {
        activeNotificationBadgeRequest->cancel();
    }
    activeNotificationBadgeRequest = std::move(request);
}

void Runtime::cancelAccountProfileRequest()
{
    std::scoped_lock lock{requestMutex};
    if (activeAccountProfileRequest) {
        activeAccountProfileRequest->cancel();
    }
    activeAccountProfileRequest.reset();
}

void Runtime::keepAccountProfileRequest(application::read::async_service::ReadRequest request)
{
    std::scoped_lock lock{requestMutex};
    if (activeAccountProfileRequest) {
        activeAccountProfileRequest->cancel();
    }
    activeAccountProfileRequest = std::move(request);
}

foundation::result::Result<std::unique_ptr<Runtime>> makeRuntime(
    RuntimeConfiguration configuration)
{
    // Keep one validated provider factory. The named options are a lifetime
    // and readability boundary, not a second protocol/signing implementation.
    return makeRuntime(
        configuration.databasePath,
        configuration.networkProxyPolicy,
        configuration.providerBaseUrl,
        configuration.farcasterReadBaseUrl,
        configuration.farcasterReadApiKey,
        configuration.additionalReadProviders,
        configuration.readProviderApiKeys,
        configuration.preferredReadProviderId,
        configuration.allowCompatibleReadFallback,
        configuration.additionalWriteProviders,
        configuration.preferredWriteProviderId,
        configuration.authRelayUrl,
        configuration.optimismRpcUrl,
        configuration.snapchainBaseUrl,
        configuration.snapchainWriteBaseUrl,
        configuration.mediaGatewayBaseUrl,
        configuration.snapchainAuthorizationHeader,
        configuration.snapchainAuthorizationValue,
        configuration.snapchainWriteAuthorizationHeader,
        configuration.snapchainWriteAuthorizationValue,
        configuration.appFid,
        configuration.signerAttestationUrl,
        std::move(configuration.approvalLauncher));
}

foundation::result::Result<std::unique_ptr<Runtime>> makeRuntime(
    const std::filesystem::path& databasePath,
    const farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy,
    [[maybe_unused]] const std::optional<std::string>& providerBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& farcasterReadBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& farcasterReadApiKey,
    [[maybe_unused]] const std::vector<ReadProviderProfile>& additionalReadProviders,
    [[maybe_unused]] const std::unordered_map<std::string, std::string>& readProviderApiKeys,
    [[maybe_unused]] const std::optional<std::string>& preferredReadProviderId,
    [[maybe_unused]] const bool allowCompatibleReadFallback,
    [[maybe_unused]] const std::vector<WriteProviderProfile>& additionalWriteProviders,
    [[maybe_unused]] const std::optional<std::string>& preferredWriteProviderId,
    [[maybe_unused]] const std::optional<std::string>& authRelayUrl,
    [[maybe_unused]] const std::optional<std::string>& optimismRpcUrl,
    [[maybe_unused]] const std::optional<std::string>& snapchainBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& snapchainWriteBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& mediaGatewayBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& snapchainAuthorizationHeader,
    [[maybe_unused]] const std::optional<std::string>& snapchainAuthorizationValue,
    [[maybe_unused]] const std::optional<std::string>& snapchainWriteAuthorizationHeader,
    [[maybe_unused]] const std::optional<std::string>& snapchainWriteAuthorizationValue,
    [[maybe_unused]] const std::optional<std::uint64_t> appFid,
    [[maybe_unused]] const std::optional<std::string>& signerAttestationUrl,
    [[maybe_unused]] std::function<foundation::result::Status(const std::string&)> approvalLauncher)
{
    std::error_code directoryError{};
    std::filesystem::create_directories(databasePath.parent_path(), directoryError);
    if (directoryError) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::StorageFailure,
            "Failed to create application data directory: " + directoryError.message()));
    }

    auto runtime = std::make_unique<Runtime>();
    runtime->cacheExecutor = std::make_unique<farcaster::execution::serial::SerialExecutor>(256U);
    const auto hardwareThreads = std::max(2U, std::thread::hardware_concurrency());
    const auto decodeWorkers = std::clamp(hardwareThreads > 2U ? hardwareThreads - 2U : 2U, 2U, 4U);
    runtime->decodeExecutor =
        std::make_unique<farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor>(
            decodeWorkers, 48U, 64U * 1024U * 1024U);
    runtime->executor = std::make_unique<farcaster::execution::serial::SerialExecutor>(128U);
    auto store = farcaster::storage::read::openReadStore(databasePath);
    if (!store) {
        return std::unexpected(std::move(store.error()));
    }
    runtime->store = std::move(*store);
    auto sessionStore = farcaster::storage::session::openSessionStore(databasePath);
    if (!sessionStore) {
        return std::unexpected(std::move(sessionStore.error()));
    }
    runtime->sessionStore = std::move(*sessionStore);

    auto transport = farcaster::transport::qtnetwork::makeTransport(
        networkProxyPolicy,
        farcaster::transport::qtnetwork::ProtocolPolicy::Http1Only);
    if (!transport) {
        return std::unexpected(std::move(transport.error()));
    }
    runtime->transport = std::move(*transport);

    auto asyncTransport = farcaster::transport::qtnetwork::async::makeAsyncTransport(
        networkProxyPolicy,
        farcaster::transport::qtnetwork::ProtocolPolicy::Http1Only);
    if (!asyncTransport) {
        return std::unexpected(std::move(asyncTransport.error()));
    }
    runtime->asyncTransport = std::move(*asyncTransport);

    capability::auth::IAuthenticationProvider* primaryAuthentication = nullptr;
    capability::auth::IFarcasterIdentityResolver* farcasterIdentityResolver = nullptr;

#if FARCASTER_HAS_FARCASTER_AUTH
    farcaster::provider::compatibility::auth::Config farcasterAuthConfig{};
    if (authRelayUrl) {
        farcasterAuthConfig.relayBaseUrl = *authRelayUrl;
    }
    if (optimismRpcUrl) {
        farcasterAuthConfig.optimismRpcUrl = *optimismRpcUrl;
    }
    runtime->optimismRpcUrl = farcasterAuthConfig.optimismRpcUrl;
    auto farcasterAuthentication =
        farcaster::provider::compatibility::auth::makeAuthentication(
            *runtime->transport, std::move(farcasterAuthConfig));
    if (!farcasterAuthentication) {
        return std::unexpected(std::move(farcasterAuthentication.error()));
    }
    runtime->farcasterAuthentication = std::move(*farcasterAuthentication);
    primaryAuthentication = runtime->farcasterAuthentication.get();
    farcasterIdentityResolver = runtime->farcasterAuthentication.get();
#endif

#if GENYCASTER_HAS_OPENPROOF_AUTH
    farcaster::provider::identity::openproof::Config openProofConfig{};
    if (authRelayUrl) {
        openProofConfig.farcasterRelayBaseUrl = *authRelayUrl;
    }
    auto openProofAuthentication =
        farcaster::provider::identity::openproof::makeAuthentication(
            *runtime->transport, std::move(openProofConfig));
    if (!openProofAuthentication) {
        return std::unexpected(std::move(openProofAuthentication.error()));
    }
    runtime->openProofAuthentication = std::move(*openProofAuthentication);
    primaryAuthentication = runtime->openProofAuthentication.get();
#endif

    runtime->sessionService = std::make_unique<application::session::SessionService>(
        *runtime->executor,
        *runtime->sessionStore,
        primaryAuthentication,
        farcasterIdentityResolver,
#if FARCASTER_HAS_FARCASTER_AUTH
        runtime->farcasterAuthentication.get()
#else
        nullptr
#endif
        );

#if FARCASTER_HAS_PROTOCOL_SIGNING
    runtime->signerExecutor = std::make_unique<farcaster::execution::serial::SerialExecutor>();
    auto signerMetadataStore = farcaster::storage::signer::openSignerMetadataStore(databasePath);
    if (!signerMetadataStore) {
        runtime->signerSetupError = std::move(signerMetadataStore.error());
    } else {
        runtime->signerMetadataStore = std::move(*signerMetadataStore);
    }

    auto signerVault = farcaster::platform::signer_vault::open("org.genycaster.protocol-signer");
    if (!signerVault) {
        if (!runtime->signerSetupError) {
            runtime->signerSetupError = std::move(signerVault.error());
        }
    } else {
        runtime->signerVault = std::move(*signerVault);
    }

#if FARCASTER_HAS_SNAPCHAIN_SIGNER
    auto signerRegistryConfig = farcaster::provider::snapchain::signer::Config{};
    if (snapchainBaseUrl && !snapchainBaseUrl->empty()) {
        signerRegistryConfig.baseUrl = *snapchainBaseUrl;
    }
    // Signer state is read from the selected Snapchain node; KEY_ADD and
    // KEY_REMOVE must submit to its write-capable route (validator if needed).
    if (snapchainWriteBaseUrl && !snapchainWriteBaseUrl->empty()) {
        signerRegistryConfig.submitBaseUrl = *snapchainWriteBaseUrl;
    }
    if (snapchainAuthorizationHeader && snapchainAuthorizationValue) {
        signerRegistryConfig.authorizationHeaderName = *snapchainAuthorizationHeader;
        signerRegistryConfig.authorizationHeaderValue = *snapchainAuthorizationValue;
    }
    if (snapchainWriteAuthorizationHeader && snapchainWriteAuthorizationValue) {
        signerRegistryConfig.submitAuthorizationHeaderName = *snapchainWriteAuthorizationHeader;
        signerRegistryConfig.submitAuthorizationHeaderValue = *snapchainWriteAuthorizationValue;
    }
    runtime->signerRegistrySources.push_back(
        farcaster::provider::snapchain::signer::makeRegistry(
            *runtime->transport, std::move(signerRegistryConfig)));
    runtime->signerRegistry = std::make_unique<routing::signer::RoutedSignerRegistry>();
    runtime->signerRegistry->addSource(routing::signer::SourceBinding{
        .providerId = farcaster::provider::model::ProviderId{"primary-protocol-writer"},
        .registry = runtime->signerRegistrySources.back().get(),
    });
#if !FARCASTER_HAS_SNAPCHAIN_WRITE
    // Signer-only deployment profiles still expose key management without
    // falsely claiming the canonical Cast/Reaction/Link submit capability.
    const auto signerOnlyRegistration = runtime->registry.registerProvider({
        .provider = farcaster::provider::model::ProviderDescriptor{
            .id = farcaster::provider::model::ProviderId{"primary-protocol-writer"},
            .displayName = "Protocol signer node",
            .deployment = farcaster::provider::model::ProviderDeployment::PublicManaged,
            .privacy = farcaster::provider::model::ProviderPrivacy::Standard,
            .priority = 10,
        },
        .capabilities = {capability::model::CapabilityDescriptor{
            .id = capability::catalog::protocolSignerManage(),
            .version = {1, 0},
            .origin = capability::model::CapabilityOrigin::Protocol,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {capability::model::CapabilityAccess::Write},
            .isSelfHostable = true,
        }},
    });
    if (!signerOnlyRegistration) {
        return std::unexpected(std::move(signerOnlyRegistration.error()));
    }
#endif
#endif

#if FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER
    if (approvalLauncher) {
        auto authorizer = farcaster::provider::compatibility::browser_wallet::authorizer::makeAuthorizer(
            farcaster::provider::compatibility::browser_wallet::authorizer::Config{
                .launch = std::move(approvalLauncher),
            });
        if (!authorizer) {
            if (!runtime->signerSetupError) {
                runtime->signerSetupError = std::move(authorizer.error());
            }
        } else {
            runtime->custodyAuthorizer = std::move(*authorizer);
        }
    }
#endif

    const bool hasAppFid = appFid.has_value() && *appFid != 0U;
    const bool hasAttestationUrl = signerAttestationUrl.has_value() && !signerAttestationUrl->empty();
    // A custody wallet may authorize a *personal* signer using its own FID
    // as requestFid. Requiring App-FID attribution for all builds previously
    // prevented SignerService from being created at all, even though the
    // native authorizer already supports this verified user-owned flow.
    //
    // Never pretend a personal signer has Genycaster App-FID attribution:
    // only a configured App FID AND its trusted attestor enable that mode.
    const bool useExternalAppAttribution = hasAppFid && hasAttestationUrl;
    if (hasAttestationUrl && !hasAppFid && !runtime->signerSetupError) {
        runtime->signerSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "A signer-attestation service was configured without the matching App FID.");
    }
#if FARCASTER_HAS_SIGNER_ATTESTATION
    if (useExternalAppAttribution) {
        auto attestor = farcaster::provider::compatibility::signer_attestation::makeAttestor(
            *runtime->transport,
            farcaster::provider::compatibility::signer_attestation::Config{
                .endpointUrl = *signerAttestationUrl,
            });
        if (!attestor) {
            if (!runtime->signerSetupError) {
                runtime->signerSetupError = std::move(attestor.error());
            }
        } else {
            runtime->signerAttestor = std::move(*attestor);
        }
    }
#else
    if (useExternalAppAttribution && !runtime->signerSetupError) {
        runtime->signerSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "App-FID signer attestation is configured but this build excluded the attestation provider.");
    }
#endif

    if (runtime->signerExecutor && runtime->signerMetadataStore
        && runtime->signerVault && !runtime->signerSetupError) {
        application::signer::SignerPolicy signerPolicy{};
        // Without an external attestor the service retains its default
        // self-attributed policy. Its requestFid is the viewer's verified FID,
        // and BrowserWalletAuthorizer requires both EIP-712 wallet signatures.
        if (useExternalAppAttribution) {
            signerPolicy.requestFid = domain::identity::Fid{*appFid};
            signerPolicy.requireExternalAppAttribution = true;
        }
        runtime->signerService = std::make_unique<application::signer::SignerService>(
            *runtime->signerExecutor,
            *runtime->signerVault,
            *runtime->signerMetadataStore,
            runtime->signerRegistry.get(),
            runtime->custodyAuthorizer.get(),
            runtime->signerAttestor.get(),
            std::move(signerPolicy));
    }

    auto writeOutbox = farcaster::storage::write::openOutboxStore(databasePath);
    if (!writeOutbox) {
        runtime->writeSetupError = std::move(writeOutbox.error());
    } else {
        runtime->writeOutbox = std::move(*writeOutbox);
    }
    runtime->writeCodec = farcaster::protocol::write::makeCanonicalWriteCodec();

#if FARCASTER_HAS_SNAPCHAIN_WRITE
    auto writeSubmitterConfig = farcaster::provider::snapchain::write::Config{};
    // All Cast/Reaction/Link writes share the same selected validator route.
    // Reads for reconciliation remain on the same authoritative endpoint.
    if (snapchainWriteBaseUrl && !snapchainWriteBaseUrl->empty()) {
        writeSubmitterConfig.baseUrl = *snapchainWriteBaseUrl;
    } else if (snapchainBaseUrl && !snapchainBaseUrl->empty()) {
        writeSubmitterConfig.baseUrl = *snapchainBaseUrl;
    }
    if (snapchainBaseUrl && !snapchainBaseUrl->empty()) {
        writeSubmitterConfig.readBaseUrl = *snapchainBaseUrl;
    }
    if (snapchainAuthorizationHeader && snapchainAuthorizationValue) {
        writeSubmitterConfig.readAuthorizationHeaderName = *snapchainAuthorizationHeader;
        writeSubmitterConfig.readAuthorizationHeaderValue = *snapchainAuthorizationValue;
    }
    // Never forward read-only credentials to a different write destination.
    if (snapchainWriteAuthorizationHeader && snapchainWriteAuthorizationValue) {
        writeSubmitterConfig.authorizationHeaderName = *snapchainWriteAuthorizationHeader;
        writeSubmitterConfig.authorizationHeaderValue = *snapchainWriteAuthorizationValue;
    } else if (snapchainAuthorizationHeader && snapchainAuthorizationValue
               && snapchainWriteBaseUrl == snapchainBaseUrl) {
        writeSubmitterConfig.authorizationHeaderName = *snapchainAuthorizationHeader;
        writeSubmitterConfig.authorizationHeaderValue = *snapchainAuthorizationValue;
    }
    const farcaster::provider::model::ProviderDescriptor writerDescriptor{
        .id = farcaster::provider::model::ProviderId{"primary-protocol-writer"},
        .displayName = "Primary protocol writer",
        .deployment = farcaster::provider::model::ProviderDeployment::PublicManaged,
        .privacy = farcaster::provider::model::ProviderPrivacy::Standard,
        .priority = 10,
    };
    const auto writerRegistration = runtime->registry.registerProvider({
        .provider = writerDescriptor,
        .capabilities = {capability::model::CapabilityDescriptor{
            .id = capability::catalog::protocolMessageSubmit(),
            .version = {1, 0},
            .origin = capability::model::CapabilityOrigin::Protocol,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {capability::model::CapabilityAccess::Write},
            .isSelfHostable = true,
        },
#if FARCASTER_HAS_SNAPCHAIN_SIGNER
        capability::model::CapabilityDescriptor{
            .id = capability::catalog::protocolSignerManage(),
            .version = {1, 0},
            .origin = capability::model::CapabilityOrigin::Protocol,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {capability::model::CapabilityAccess::Write},
            .isSelfHostable = true,
        },
#endif
        },
    });
    if (!writerRegistration) return std::unexpected(std::move(writerRegistration.error()));
    const std::string primaryReadIdentity = writeSubmitterConfig.readBaseUrl.empty()
        ? writeSubmitterConfig.baseUrl : writeSubmitterConfig.readBaseUrl;
    runtime->writeSourceEndpointIdentities.push_back(
        "snapchain-v1|" + writeSubmitterConfig.baseUrl + "|" + primaryReadIdentity);
    runtime->writeSources.push_back(
        farcaster::provider::snapchain::write::makeSubmitter(
            *runtime->transport, std::move(writeSubmitterConfig)));
    runtime->writeSourceIds.push_back(writerDescriptor.id.value);

    for (const WriteProviderProfile& profile : additionalWriteProviders) {
        if (profile.adapterContract != "snapchain-v1") {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unsupported,
                "No installed write adapter implements contract '" + profile.adapterContract + "'."));
        }
        if (profile.id.empty() || profile.endpoint.empty()
            || runtime->registry.provider(farcaster::provider::model::ProviderId{profile.id})) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Write provider ID/endpoint is empty or collides with a registered provider."));
        }
        const farcaster::provider::model::ProviderDescriptor instance{
            .id = farcaster::provider::model::ProviderId{profile.id},
            .displayName = profile.displayName.empty() ? profile.id : profile.displayName,
            .deployment = profile.selfHosted
                ? farcaster::provider::model::ProviderDeployment::SelfHosted
                : farcaster::provider::model::ProviderDeployment::PublicManaged,
            .privacy = profile.selfHosted
                ? farcaster::provider::model::ProviderPrivacy::Private
                : farcaster::provider::model::ProviderPrivacy::Standard,
            .priority = 30,
        };
        auto registered = runtime->registry.registerProvider({
            .provider = instance,
            .capabilities = {capability::model::CapabilityDescriptor{
                .id = capability::catalog::protocolMessageSubmit(),
                .version = {1, 0},
                .origin = capability::model::CapabilityOrigin::Protocol,
                .quality = capability::model::CapabilityQuality::Native,
                .access = {capability::model::CapabilityAccess::Write},
                .isSelfHostable = true,
            },
#if FARCASTER_HAS_SNAPCHAIN_SIGNER
            capability::model::CapabilityDescriptor{
                .id = capability::catalog::protocolSignerManage(),
                .version = {1, 0},
                .origin = capability::model::CapabilityOrigin::Protocol,
                .quality = capability::model::CapabilityQuality::Native,
                .access = {capability::model::CapabilityAccess::Write},
                .isSelfHostable = true,
            },
#endif
            },
        });
        if (!registered) return std::unexpected(std::move(registered.error()));
        farcaster::provider::snapchain::write::Config config{};
        config.baseUrl = profile.endpoint;
        config.readBaseUrl = profile.readEndpoint.empty()
            ? profile.endpoint : profile.readEndpoint;
        runtime->writeSourceEndpointIdentities.push_back(
            "snapchain-v1|" + config.baseUrl + "|" + config.readBaseUrl);
        runtime->writeSources.push_back(
            farcaster::provider::snapchain::write::makeSubmitter(
                *runtime->transport, std::move(config)));
        runtime->writeSourceIds.push_back(profile.id);
#if FARCASTER_HAS_SNAPCHAIN_SIGNER
        farcaster::provider::snapchain::signer::Config signerConfig{};
        signerConfig.baseUrl = profile.readEndpoint.empty()
            ? profile.endpoint : profile.readEndpoint;
        signerConfig.submitBaseUrl = profile.endpoint;
        runtime->signerRegistrySources.push_back(
            farcaster::provider::snapchain::signer::makeRegistry(
                *runtime->transport, std::move(signerConfig)));
        if (runtime->signerRegistry) {
            runtime->signerRegistry->addSource(routing::signer::SourceBinding{
                .providerId = farcaster::provider::model::ProviderId{profile.id},
                .registry = runtime->signerRegistrySources.back().get(),
            });
        }
#endif
    }
#else
    if (!additionalWriteProviders.empty() || preferredWriteProviderId) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "No compatible protocol writer adapter is compiled into this build."));
    }
#endif
#endif

#if FARCASTER_HAS_MEDIA_UPLOAD
    runtime->mediaExecutor = std::make_unique<farcaster::execution::serial::SerialExecutor>();
    if (!runtime->signerService) {
        runtime->mediaSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::Unauthorized,
            "Media upload requires a verified Genycaster Farcaster signer.");
    } else if (!mediaGatewayBaseUrl || mediaGatewayBaseUrl->empty()) {
        runtime->mediaSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Genycaster media gateway is not configured.");
    } else {
        farcaster::transport::qtnetwork::media::Config mediaConfig{};
        mediaConfig.baseUrl = *mediaGatewayBaseUrl;
        auto mediaGateway = farcaster::transport::qtnetwork::media::makeGateway(
            *runtime->signerService, *runtime->asyncTransport, mediaConfig);
        auto mediaLifecycleGateway = farcaster::transport::qtnetwork::media::makeLifecycleGateway(
            *runtime->signerService, *runtime->asyncTransport, std::move(mediaConfig));
        if (!mediaGateway) {
            runtime->mediaSetupError = std::move(mediaGateway.error());
        } else if (!mediaLifecycleGateway) {
            runtime->mediaSetupError = std::move(mediaLifecycleGateway.error());
        } else {
            runtime->mediaGateway = std::move(*mediaGateway);
            runtime->mediaLifecycleGateway = std::move(*mediaLifecycleGateway);
            runtime->mediaUploadService = std::make_unique<application::media::upload::MediaUploadService>(
                *runtime->mediaExecutor, *runtime->mediaGateway);
            runtime->mediaLifecycleService = std::make_unique<application::media::lifecycle::MediaLifecycleService>(
                *runtime->mediaExecutor, *runtime->mediaLifecycleGateway);
        }
    }
#endif

#if FARCASTER_HAS_HYPERSNAP_PROVIDER || FARCASTER_HAS_SNAPCHAIN_READ
    auto continuationStore = farcaster::storage::continuation::openContinuationStore(databasePath);
    if (!continuationStore) {
        return std::unexpected(std::move(continuationStore.error()));
    }
    runtime->continuations.emplace(std::move(*continuationStore));
#endif

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    const farcaster::provider::model::ProviderDescriptor descriptor{
        .id = farcaster::provider::model::ProviderId{"hypersnap-v2"},
        .displayName = "Hypersnap v2",
        .deployment = farcaster::provider::model::ProviderDeployment::PublicManaged,
        .privacy = farcaster::provider::model::ProviderPrivacy::Standard,
        .priority = 10,
    };
    auto registration = runtime->registry.registerProvider(
        routing::registry::ProviderRegistration{
            .provider = descriptor,
            .capabilities = farcaster::provider::hypersnap::registration::capabilities(),
        });
    if (!registration) {
        return std::unexpected(std::move(registration.error()));
    }
    runtime->independentReadProviderId = descriptor.id.value;
    farcaster::provider::hypersnap::config::Config providerConfig{
        .provider = descriptor,
    };
    if (providerBaseUrl) {
        providerConfig.baseUrl = *providerBaseUrl;
    }
    runtime->asyncGateway = std::make_unique<farcaster::provider::hypersnap::async_gateway::AsyncGateway>(
        *runtime->asyncTransport,
        *runtime->decodeExecutor,
        *runtime->cacheExecutor,
        *runtime->continuations,
        providerConfig);

    // A separate configured Neynar-compatible hosted Farcaster read source.
    // Endpoint matching is required: plain Farcaster hub /v1 is NOT a
    // drop-in replacement for the v2 indexed JSON API and must not be bound.
    if (farcasterReadBaseUrl && !farcasterReadBaseUrl->empty()) {
        const farcaster::provider::model::ProviderDescriptor farcasterDescriptor{
            .id = farcaster::provider::model::ProviderId{"farcaster-snapchain"},
            .displayName = "Farcaster hosted read API",
            .deployment = farcaster::provider::model::ProviderDeployment::PublicManaged,
            .privacy = farcaster::provider::model::ProviderPrivacy::Standard,
            .priority = 20,
        };
        auto registered = runtime->registry.registerProvider(
            routing::registry::ProviderRegistration{
                .provider = farcasterDescriptor,
                .capabilities = farcaster::provider::hypersnap::registration::capabilities(),
            });
        if (!registered) {
            return std::unexpected(std::move(registered.error()));
        }
        runtime->hostedReadProviderId = farcasterDescriptor.id.value;
        farcaster::provider::hypersnap::config::Config farcasterConfig{
            .baseUrl = *farcasterReadBaseUrl,
            .apiKey = farcasterReadApiKey.value_or(std::string{}),
            .provider = farcasterDescriptor,
        };
        runtime->farcasterReadGateway =
            std::make_unique<farcaster::provider::hypersnap::async_gateway::AsyncGateway>(
                *runtime->asyncTransport,
                *runtime->decodeExecutor,
                *runtime->cacheExecutor,
                *runtime->continuations,
                std::move(farcasterConfig));
    }

    // Instance registration is contract-driven: any number of independent
    // endpoints can use a compatible adapter, with per-instance health/cursors.
    // Unknown contracts are NEVER bound to a superficially similar API.
    for (const ReadProviderProfile& profile : additionalReadProviders) {
        if (profile.adapterContract == "snapchain-v1") continue;
        if (profile.adapterContract != "hypersnap-v2") {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unsupported,
                "No installed read adapter implements contract '" + profile.adapterContract + "'."));
        }
        if (profile.id.empty() || profile.endpoint.empty()
            || runtime->registry.provider(farcaster::provider::model::ProviderId{profile.id})) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Read provider id/endpoint is empty or collides with an existing provider."));
        }
        const farcaster::provider::model::ProviderDescriptor instance{
            .id = farcaster::provider::model::ProviderId{profile.id},
            .displayName = profile.displayName.empty() ? profile.id : profile.displayName,
            .deployment = profile.selfHosted
                ? farcaster::provider::model::ProviderDeployment::SelfHosted
                : farcaster::provider::model::ProviderDeployment::PublicManaged,
            .privacy = profile.selfHosted
                ? farcaster::provider::model::ProviderPrivacy::Private
                : farcaster::provider::model::ProviderPrivacy::Standard,
            .priority = 30,
        };
        auto registeredInstance = runtime->registry.registerProvider({
            .provider = instance,
            .capabilities = farcaster::provider::hypersnap::registration::capabilities(),
        });
        if (!registeredInstance) return std::unexpected(std::move(registeredInstance.error()));
        // Per-instance credentials are injected only for the exact chosen
        // indexed provider and never copied from the built-in hosted service.
        // Plain HTTP must not transmit operator credentials.
        const auto credential = readProviderApiKeys.find(profile.id);
        const std::string endpointScheme = profile.endpoint.substr(0U, 8U);
        const bool secureEndpoint = endpointScheme.size() == 8U
            && std::equal(endpointScheme.begin(), endpointScheme.end(), "https://",
                [](const unsigned char input, const char expected) {
                    return std::tolower(input) == expected;
                });
        if (credential != readProviderApiKeys.end()
            && (!secureEndpoint || credential->second.empty())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Authenticated indexed read providers require an HTTPS endpoint and a valid credential."));
        }
        farcaster::provider::hypersnap::config::Config config{
            .baseUrl = profile.endpoint,
            .apiKey = credential == readProviderApiKeys.end()
                ? std::string{} : credential->second,
            .provider = instance,
        };
        runtime->additionalReadGateways.push_back(
            std::make_unique<farcaster::provider::hypersnap::async_gateway::AsyncGateway>(
                *runtime->asyncTransport, *runtime->decodeExecutor,
                *runtime->cacheExecutor, *runtime->continuations, std::move(config)));
        runtime->additionalReadProviderIds.push_back(profile.id);
    }
#endif

#if FARCASTER_HAS_SNAPCHAIN_READ
    for (const ReadProviderProfile& profile : additionalReadProviders) {
        if (profile.adapterContract != "snapchain-v1") continue;
        if (profile.id.empty() || profile.endpoint.empty()
            || runtime->registry.provider(farcaster::provider::model::ProviderId{profile.id})) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Raw-node provider ID/endpoint is empty or already registered."));
        }
        const farcaster::provider::model::ProviderDescriptor instance{
            .id = farcaster::provider::model::ProviderId{profile.id},
            .displayName = profile.displayName.empty() ? profile.id : profile.displayName,
            .deployment = profile.selfHosted
                ? farcaster::provider::model::ProviderDeployment::SelfHosted
                : farcaster::provider::model::ProviderDeployment::PublicManaged,
            .privacy = profile.selfHosted
                ? farcaster::provider::model::ProviderPrivacy::Private
                : farcaster::provider::model::ProviderPrivacy::Standard,
            .priority = 30,
        };
        auto registered = runtime->registry.registerProvider({
            .provider = instance,
            .capabilities = farcaster::provider::snapchain::read::capabilities(),
        });
        if (!registered) return std::unexpected(std::move(registered.error()));
        const auto rawCredential = readProviderApiKeys.find(profile.id);
        if (rawCredential != readProviderApiKeys.end()
            && (!profile.endpoint.starts_with("https://")
                || rawCredential->second.empty())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Authenticated raw read providers require HTTPS and a valid credential."));
        }
        runtime->rawReadGateways.push_back(
            farcaster::provider::snapchain::read::makeGateway(
                *runtime->asyncTransport,
                *runtime->decodeExecutor,
                *runtime->continuations,
                farcaster::provider::snapchain::read::Config{
                    .baseUrl = profile.endpoint,
                    .authorizationHeaderName = rawCredential == readProviderApiKeys.end()
                        ? std::string{} : std::string{"x-api-key"},
                    .authorizationHeaderValue = rawCredential == readProviderApiKeys.end()
                        ? std::string{} : rawCredential->second,
                    .providerId = instance.id,
                }));
        runtime->rawReadProviderIds.push_back(profile.id);
    }
#else
    for (const ReadProviderProfile& profile : additionalReadProviders) {
        if (profile.adapterContract == "snapchain-v1") {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unsupported,
                "Raw Snapchain reader adapter is not compiled into this build."));
        }
    }
#endif

    const bool strict = application::configuration::configuredDeploymentProfile()
        == application::configuration::DeploymentProfile::StrictSelfHosted;
#if !FARCASTER_HAS_HYPERSNAP_PROVIDER
    const bool missingIndexedAdapter = std::any_of(
        additionalReadProviders.begin(), additionalReadProviders.end(),
        [](const ReadProviderProfile& profile) {
            return profile.adapterContract != "snapchain-v1";
        });
    if (missingIndexedAdapter) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "Indexed read provider profiles require a compatible installed adapter."));
    }
#endif
    runtime->requestedReadProviderId = preferredReadProviderId.value_or(std::string{});
    runtime->allowCompatibleReadFallback = allowCompatibleReadFallback;
    runtime->router = std::make_unique<routing::router::CapabilityRouter>(
        runtime->registry,
        runtime->health,
        routing::policy::RoutingPolicy{
            .mode = strict ? routing::policy::RoutingMode::StrictSelfHosted
                           : routing::policy::RoutingMode::Standard,
            .allowManagedProviders = !strict,
            .minimumQuality = farcaster::capability::model::CapabilityQuality::Experimental,
        },
        // Only composition names concrete provider registrations. Routing
        // itself owns an operation preference but never a vendor identifier.
        routing::router::PreferredRouteProviders{
            .independent = farcaster::provider::model::ProviderId{"hypersnap-v2"},
            .hosted = farcaster::provider::model::ProviderId{"farcaster-snapchain"},
        });

#if FARCASTER_HAS_SNAPCHAIN_SIGNER
    if (runtime->signerRegistry) runtime->signerRegistry->bindRouter(*runtime->router);
#endif

    if (preferredWriteProviderId && !preferredWriteProviderId->empty()) {
        const farcaster::provider::model::ProviderId selected{*preferredWriteProviderId};
        if (!runtime->registry.provider(selected)) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Selected writer has no registered compatible adapter: " + selected.value));
        }
        runtime->router->setPreferredProviderForAccess(
            capability::model::CapabilityAccess::Write,
            routing::router::ProviderPreference{.provider = selected, .require = true});
    }

    if (!runtime->requestedReadProviderId.empty()) {
        const farcaster::provider::model::ProviderId selected{
            runtime->requestedReadProviderId};
        if (!runtime->registry.provider(selected)) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Preferred read provider has no registered compatible adapter: "
                    + selected.value));
        }
        runtime->router->setPreferredProviderForAccess(
            capability::model::CapabilityAccess::Read,
            routing::router::ProviderPreference{
                .provider = selected, .require = !allowCompatibleReadFallback});
    }

#if FARCASTER_HAS_PROTOCOL_SIGNING
    if (runtime->writeOutbox && runtime->writeCodec && runtime->signerService
        && runtime->signerExecutor) {
        // The binding table is in the same SQLite database as the canonical
        // outbox. Never allow network submission when durable pinning fails.
        auto bindings = farcaster::storage::write::openWriteRouteBindings(databasePath);
        if (!bindings) {
            runtime->writeSetupError = std::move(bindings.error());
        } else {
            runtime->writeRouteBindings = std::move(*bindings);
            std::vector<routing::write::SourceBinding> sources{};
            for (std::size_t index = 0; index < runtime->writeSources.size(); ++index) {
                sources.push_back(routing::write::SourceBinding{
                    .providerId = farcaster::provider::model::ProviderId{
                        runtime->writeSourceIds[index]},
                    .endpointIdentity = runtime->writeSourceEndpointIdentities[index],
                    .submitter = runtime->writeSources[index].get(),
                });
            }
            runtime->writeSubmitter = std::make_unique<routing::write::RoutedWriteSubmitter>(
                *runtime->router, *runtime->writeRouteBindings, std::move(sources));
            runtime->writeService = std::make_unique<application::write::WriteService>(
                *runtime->signerExecutor, *runtime->signerService,
                *runtime->writeCodec, *runtime->writeOutbox,
                runtime->writeSubmitter.get(), application::write::WritePolicy{});
        }
    } else if (!runtime->writeSetupError) {
        runtime->writeSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "Durable writes require the signer and persistent outbox.");
    }
#endif

    const auto identityRoute = runtime->router->resolve(routing::policy::CapabilityRequest{
        .capabilityId = capability::catalog::identityFidResolve(),
        .requiredAccess = capability::model::CapabilityAccess::Read,
        .minimumQuality = capability::model::CapabilityQuality::Experimental,
    });
    if (!identityRoute) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Live-read runtime composition is incomplete. " + identityRoute.error().message));
    }

#if FARCASTER_HAS_HYPERSNAP_PROVIDER || FARCASTER_HAS_SNAPCHAIN_READ
    std::vector<routing::read::async_gateway::SourceBinding> bindings{};
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    bindings.push_back({
        .providerId = descriptor.id,
        .gateway = runtime->asyncGateway.get(),
        .providesProfileCounts = true,
    });
    if (runtime->farcasterReadGateway) {
        bindings.push_back({
            .providerId = farcaster::provider::model::ProviderId{"farcaster-snapchain"},
            .gateway = runtime->farcasterReadGateway.get(),
            .providesProfileCounts = true,
        });
    }
    for (std::size_t index = 0; index < runtime->additionalReadGateways.size(); ++index) {
        bindings.push_back({
            .providerId = farcaster::provider::model::ProviderId{
                runtime->additionalReadProviderIds[index]},
            .gateway = runtime->additionalReadGateways[index].get(),
            .providesProfileCounts = true,
        });
    }
#endif
    for (std::size_t index = 0; index < runtime->rawReadGateways.size(); ++index) {
        bindings.push_back({
            .providerId = farcaster::provider::model::ProviderId{runtime->rawReadProviderIds[index]},
            .gateway = runtime->rawReadGateways[index].get(),
        });
    }
    runtime->routedAsyncGateway = std::make_unique<routing::read::async_gateway::RoutedReadGateway>(
        *runtime->router, runtime->health, std::move(bindings),
        allowCompatibleReadFallback);
    runtime->service = std::make_unique<application::read::async_service::ReadService>(
        *runtime->cacheExecutor, *runtime->store, *runtime->routedAsyncGateway);
#else
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "This build does not include an asynchronous live-read provider."));
#endif
    return runtime;
}

} // namespace genycaster::ui::runtime
