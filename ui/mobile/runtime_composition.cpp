module;

#include <algorithm>
#include <atomic>
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
    service.reset();
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    routedAsyncGateway.reset();
    asyncGateway.reset();
#endif
    asyncTransport.reset();
    decodeExecutor.reset();
    cacheExecutor.reset();
    executor.reset();
}

void Runtime::setCurrentSession(std::optional<domain::session::ClientSession> session)
{
    std::scoped_lock lock{sessionMutex};
    currentSession = std::move(session);
}

std::optional<domain::session::ClientSession> Runtime::currentSessionSnapshot()
{
    std::scoped_lock lock{sessionMutex};
    return currentSession;
}

void Runtime::cancelPrimaryRequest()
{
    primaryRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    std::scoped_lock lock{requestMutex};
    if (activePrimaryRequest) {
        activePrimaryRequest->cancel();
    }
    activePrimaryRequest.reset();
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
    std::scoped_lock lock{viewerOperationMutex};
    if (activeViewerReactionRequest) {
        activeViewerReactionRequest->cancel();
    }
    activeViewerReactionRequest.reset();
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
    {
        std::scoped_lock lock{requestMutex};
        if (activeFeedRequest) {
            activeFeedRequest->cancel();
        }
        activeFeedRequest.reset();
    }
    cancelReferencedPreviewRequests();
    cancelViewerReactionRequest();
}

void Runtime::cancelSearchRequest()
{
    std::scoped_lock lock{requestMutex};
    if (activeSearchRequest) {
        activeSearchRequest->cancel();
    }
    activeSearchRequest.reset();
}

void Runtime::cancelAllRequests()
{
    primaryRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    feedRequestGeneration.fetch_add(1U, std::memory_order_acq_rel);
    {
        std::scoped_lock lock{requestMutex};
        if (activePrimaryRequest) {
            activePrimaryRequest->cancel();
        }
        if (activeFeedRequest) {
            activeFeedRequest->cancel();
        }
        if (activeSearchRequest) {
            activeSearchRequest->cancel();
        }
        if (activeNotificationBadgeRequest) {
            activeNotificationBadgeRequest->cancel();
        }
        activePrimaryRequest.reset();
        activeFeedRequest.reset();
        activeSearchRequest.reset();
        activeNotificationBadgeRequest.reset();
    }
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
        keep = !referencedPreviewResolved.contains(castId.value);
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

foundation::result::Result<std::unique_ptr<Runtime>> makeRuntime(
    const std::filesystem::path& databasePath,
    const farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy,
    [[maybe_unused]] const std::optional<std::string>& providerBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& authRelayUrl,
    [[maybe_unused]] const std::optional<std::string>& optimismRpcUrl,
    [[maybe_unused]] const std::optional<std::string>& snapchainBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& mediaGatewayBaseUrl,
    [[maybe_unused]] const std::optional<std::string>& snapchainAuthorizationHeader,
    [[maybe_unused]] const std::optional<std::string>& snapchainAuthorizationValue,
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
        farcasterIdentityResolver);

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
    if (snapchainAuthorizationHeader && snapchainAuthorizationValue) {
        signerRegistryConfig.authorizationHeaderName = *snapchainAuthorizationHeader;
        signerRegistryConfig.authorizationHeaderValue = *snapchainAuthorizationValue;
    }
    runtime->signerRegistry = farcaster::provider::snapchain::signer::makeRegistry(
        *runtime->transport, std::move(signerRegistryConfig));
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
    if (!hasAppFid && !runtime->signerSetupError) {
        runtime->signerSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Genycaster client attribution is not configured. Set the canonical App FID; "
            "protocol writes stay disabled rather than being attributed to the signed-in user.");
    } else if (!hasAttestationUrl && !runtime->signerSetupError) {
        runtime->signerSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Genycaster App-FID attribution requires the SignedKeyRequest attestation service.");
    }
#if FARCASTER_HAS_SIGNER_ATTESTATION
    if (hasAppFid && hasAttestationUrl) {
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
    if (hasAppFid && hasAttestationUrl && !runtime->signerSetupError) {
        runtime->signerSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "App-FID signer attestation is configured but this build excluded the attestation provider.");
    }
#endif

    if (hasAppFid && hasAttestationUrl && runtime->signerExecutor &&
        runtime->signerMetadataStore && runtime->signerVault && !runtime->signerSetupError) {
        application::signer::SignerPolicy signerPolicy{};
        signerPolicy.requestFid = domain::identity::Fid{*appFid};
        signerPolicy.requireExternalAppAttribution = true;
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
    if (snapchainBaseUrl && !snapchainBaseUrl->empty()) {
        writeSubmitterConfig.baseUrl = *snapchainBaseUrl;
    }
    if (snapchainAuthorizationHeader && snapchainAuthorizationValue) {
        writeSubmitterConfig.authorizationHeaderName = *snapchainAuthorizationHeader;
        writeSubmitterConfig.authorizationHeaderValue = *snapchainAuthorizationValue;
    }
    runtime->writeSubmitter = farcaster::provider::snapchain::write::makeSubmitter(
        *runtime->transport, std::move(writeSubmitterConfig));
#endif

    if (runtime->signerExecutor && runtime->signerService && runtime->writeOutbox && runtime->writeCodec) {
        runtime->writeService = std::make_unique<application::write::WriteService>(
            *runtime->signerExecutor,
            *runtime->signerService,
            *runtime->writeCodec,
            *runtime->writeOutbox,
            runtime->writeSubmitter.get(),
            application::write::WritePolicy{});
    } else if (!runtime->writeSetupError) {
        runtime->writeSetupError = foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "Durable PublishCast composition is unavailable because secure signer or outbox setup failed.");
    }
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
    auto continuationStore = farcaster::storage::continuation::openContinuationStore(databasePath);
    if (!continuationStore) {
        return std::unexpected(std::move(continuationStore.error()));
    }
    runtime->continuations.emplace(std::move(*continuationStore));
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
#endif

    const bool strict = application::configuration::configuredDeploymentProfile()
        == application::configuration::DeploymentProfile::StrictSelfHosted;
    runtime->router = std::make_unique<routing::router::CapabilityRouter>(
        runtime->registry,
        runtime->health,
        routing::policy::RoutingPolicy{
            .mode = strict ? routing::policy::RoutingMode::StrictSelfHosted
                           : routing::policy::RoutingMode::Standard,
            .allowManagedProviders = !strict,
            .minimumQuality = farcaster::capability::model::CapabilityQuality::Experimental,
        });

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

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    runtime->routedAsyncGateway = std::make_unique<routing::read::async_gateway::RoutedReadGateway>(
        *runtime->router,
        runtime->health,
        std::vector<routing::read::async_gateway::SourceBinding>{
            routing::read::async_gateway::SourceBinding{
                .providerId = descriptor.id,
                .gateway = runtime->asyncGateway.get(),
            },
        });
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
