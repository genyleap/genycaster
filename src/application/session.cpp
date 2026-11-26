module;

#include "genycaster_deployment.hpp"

#include <algorithm>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

module farcaster.application.session;

import farcaster.application.executor;
import farcaster.capability.auth;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.session;

namespace farcaster::application::session {
namespace {

[[nodiscard]] SessionState guestState()
{
    return SessionState{.phase = SessionPhase::Guest};
}

[[nodiscard]] SessionState failedState(
    foundation::error::Error error,
    const std::optional<domain::session::ClientSession>& current = std::nullopt)
{
    if (current) {
        return SessionState{
            .phase = SessionPhase::Authenticated,
            .session = current,
            .error = std::move(error),
        };
    }
    return SessionState{
        .phase = SessionPhase::Failed,
        .error = std::move(error),
    };
}

[[nodiscard]] foundation::error::Error unavailableAuthError()
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::Unsupported,
        "Authentication is not available in this build or deployment profile.");
}

}

struct SessionService::Impl final {
    executor::IExecutor& executor;
    local::session::IClientSessionStore& store;
    capability::auth::IAuthenticationProvider* authentication{};
    capability::auth::IAuthenticationProvider* directFarcasterAuthentication{};
    capability::auth::IFarcasterIdentityResolver* farcasterIdentityResolver{};
    std::unique_ptr<capability::auth::IAuthenticationAttempt> attempt{};
    std::optional<domain::session::ClientSession> current{};
    std::string approvalUrl{};
};

WriteAccess evaluateWriteAccess(const SessionState& state, const bool signerReady) noexcept
{
    // A new authentication attempt must not invalidate an already verified
    // client session. Session presence is the login gate; `phase` describes
    // the currently visible authentication flow and may be Starting or
    // AwaitingApproval during account switching/re-authentication.
    if (!state.session) {
        return WriteAccess::SignInRequired;
    }
    return signerReady ? WriteAccess::Ready : WriteAccess::SignerRequired;
}

SessionService::SessionService(
    executor::IExecutor& executor,
    local::session::IClientSessionStore& store,
    capability::auth::IAuthenticationProvider* const authentication,
    capability::auth::IFarcasterIdentityResolver* const farcasterIdentityResolver,
    capability::auth::IAuthenticationProvider* const directFarcasterAuthentication)
    : m_impl(std::make_unique<Impl>(Impl{
          .executor = executor,
          .store = store,
          .authentication = authentication,
          .directFarcasterAuthentication = directFarcasterAuthentication,
          .farcasterIdentityResolver = farcasterIdentityResolver,
      }))
{
}

SessionService::~SessionService() = default;

foundation::result::Status SessionService::restore(Observer observer)
{
    return m_impl->executor.post([this, observer = std::move(observer)]() mutable {
        auto loaded = m_impl->store.load();
        if (!loaded) {
            observer(failedState(std::move(loaded.error())));
            return;
        }
        if (!*loaded) {
            m_impl->current.reset();
            observer(guestState());
            return;
        }
        // A registry-verified custody account is a remembered local identity,
        // not a reusable custody proof. Its signer is restored and verified separately.
        if (!domain::session::isProtocolSessionEligible(**loaded, foundation::time::now())) {
            auto cleared = m_impl->store.clear();
            m_impl->current.reset();
            if (!cleared) {
                observer(failedState(std::move(cleared.error())));
                return;
            }
            observer(guestState());
            return;
        }
        m_impl->current = std::move(**loaded);
        if (!m_impl->current->custodyAddress && m_impl->farcasterIdentityResolver != nullptr) {
            auto custodyAddress =
                m_impl->farcasterIdentityResolver->resolveCustodyAddress(m_impl->current->fid);
            if (custodyAddress) {
                m_impl->current->custodyAddress = std::move(*custodyAddress);
                if (auto saved = m_impl->store.save(*m_impl->current); !saved) {
                    observer(failedState(std::move(saved.error()), m_impl->current));
                    return;
                }
            }
        }
        observer(SessionState{
            .phase = SessionPhase::Authenticated,
            .session = m_impl->current,
        });
    });
}

foundation::result::Status SessionService::beginSignIn(
    capability::auth::SignInRequest request,
    Observer observer)
{
    return m_impl->executor.post(
        [this, request = std::move(request), observer = std::move(observer)]() mutable {
            // The Farcaster button uses the protocol-native SIWF adapter.
            // Genyleap is a distinct OpenProof sign-in route; the selected read
            // network never selects the account authorization authority.
            auto* const auth = request.route == capability::auth::SignInRoute::Farcaster
                ? m_impl->directFarcasterAuthentication : m_impl->authentication;
            if (auth == nullptr) {
                observer(failedState(unavailableAuthError(), m_impl->current));
                return;
            }
            observer(SessionState{
                .phase = SessionPhase::Starting,
                .session = m_impl->current,
            });
            auto started = auth->begin(request);
            if (!started) {
                observer(failedState(std::move(started.error()), m_impl->current));
                return;
            }
            m_impl->approvalUrl = started->challenge.approvalUrl;
            m_impl->attempt = std::move(started->attempt);
            if (!m_impl->attempt || m_impl->approvalUrl.empty()) {
                m_impl->attempt.reset();
                observer(failedState(foundation::error::Error::make(
                    foundation::error::ErrorCode::ProtocolFailure,
                    "Authentication adapter returned an incomplete approval challenge."), m_impl->current));
                return;
            }
            observer(SessionState{
                .phase = SessionPhase::AwaitingApproval,
                .session = m_impl->current,
                .approvalUrl = m_impl->approvalUrl,
            });
        });
}

foundation::result::Status SessionService::pollSignIn(
    Observer observer, AccountGrantObserver grantObserver)
{
    return m_impl->executor.post(
        [this, observer = std::move(observer),
         grantObserver = std::move(grantObserver)]() mutable {
        if (!m_impl->attempt) {
            observer(m_impl->current
                ? SessionState{.phase = SessionPhase::Authenticated, .session = m_impl->current}
                : guestState());
            return;
        }
        auto polled = m_impl->attempt->poll();
        if (!polled) {
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(failedState(std::move(polled.error()), m_impl->current));
            return;
        }
        if (polled->state == capability::auth::PollState::Pending) {
            observer(SessionState{
                .phase = SessionPhase::AwaitingApproval,
                .session = m_impl->current,
                .approvalUrl = m_impl->approvalUrl,
            });
            return;
        }
        if (polled->state == capability::auth::PollState::FarcasterLinkRequired) {
            if (polled->approvalUrl && !polled->approvalUrl->empty()) {
                m_impl->approvalUrl = *polled->approvalUrl;
            }
            if (m_impl->approvalUrl.empty()) {
                m_impl->attempt.reset();
                observer(failedState(foundation::error::Error::make(
                    foundation::error::ErrorCode::ProtocolFailure,
                    "OpenProof authenticated the account but did not provide a Farcaster-link action."),
                    m_impl->current));
                return;
            }
            observer(SessionState{
                .phase = SessionPhase::LinkingFarcaster,
                .session = m_impl->current,
                .approvalUrl = m_impl->approvalUrl,
            });
            return;
        }
        if (!polled->session) {
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(failedState(foundation::error::Error::make(
                foundation::error::ErrorCode::ProtocolFailure,
                "Completed authentication did not contain a verified client session."), m_impl->current));
            return;
        }
        if (domain::session::isExpired(*polled->session, foundation::time::now())) {
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(failedState(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized,
                "The completed authentication challenge is already expired."), m_impl->current));
            return;
        }
        if (!polled->session->custodyAddress && m_impl->farcasterIdentityResolver != nullptr) {
            auto custodyAddress =
                m_impl->farcasterIdentityResolver->resolveCustodyAddress(polled->session->fid);
            if (custodyAddress) {
                polled->session->custodyAddress = std::move(*custodyAddress);
            }
        }
        if (auto saved = m_impl->store.save(*polled->session); !saved) {
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(failedState(std::move(saved.error()), m_impl->current));
            return;
        }
        m_impl->current = std::move(*polled->session);
        m_impl->attempt.reset();
        m_impl->approvalUrl.clear();
        observer(SessionState{
            .phase = SessionPhase::Authenticated,
            .session = m_impl->current,
        });
        // Deliver only after the verified session is committed and queued
        // to the presentation bridge. A restored local session has no grant.
        if (grantObserver && polled->accountGrant && m_impl->current
            && polled->accountGrant->fid == m_impl->current->fid.value) {
            grantObserver(std::move(*polled->accountGrant));
        }
    });
}

foundation::result::Status SessionService::acceptVerifiedRecovery(
    domain::session::ClientSession session,
    Observer observer)
{
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            const auto now = foundation::time::now();
            // This is an application commit boundary, not a second
            // authentication endpoint. Only accept the specifically
            // OpenProof-verified custody identity with a bounded lifetime.
            const auto validAddress = [](const std::optional<std::string>& address) {
                if (!address || address->size() != 42U
                    || !address->starts_with("0x")) {
                    return false;
                }
                return std::all_of(address->begin() + 2, address->end(),
                    [](const char ch) {
                        return (ch >= '0' && ch <= '9')
                            || (ch >= 'a' && ch <= 'f');
                    });
            };
            if (session.fid.value == 0U
                || session.method != domain::session::AuthenticationMethod::Custody
                || !session.identityIssuer
                || *session.identityIssuer != genycaster::deployment::openProofIssuer
                || !session.identitySubject || session.identitySubject->empty()
                || session.identitySubject->size() > 512U
                || !validAddress(session.custodyAddress)
                || session.authenticatedAt > now
                || domain::session::isExpired(session, now)
                || session.expiresAt - now > std::chrono::hours{1}) {
                observer(failedState(foundation::error::Error::make(
                    foundation::error::ErrorCode::Unauthorized,
                    "A complete, current OpenProof custody recovery proof is required."),
                    m_impl->current));
                return;
            }
            if (auto stored = m_impl->store.save(session); !stored) {
                observer(failedState(std::move(stored.error()), m_impl->current));
                return;
            }
            m_impl->current = std::move(session);
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(SessionState{
                .phase = SessionPhase::Authenticated,
                .session = m_impl->current,
            });
        });
}

foundation::result::Status SessionService::acceptVerifiedNativeCustody(
    domain::session::ClientSession session, Observer observer)
{
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            constexpr std::uint64_t FirstNativeFid = (1ULL << 32);
            constexpr std::string_view NativeIssuer = "hypersnap:native-registry";
            const auto now = foundation::time::now();
            const std::string expectedSubject =
                "native-fid:" + std::to_string(session.fid.value);
            const auto& address = session.custodyAddress;
            const auto validAddress = address && address->size() == 42
                && address->starts_with("0x")
                && std::all_of(address->begin() + 2, address->end(),
                    [](char c) {
                        return (c >= '0' && c <= '9')
                            || (c >= 'a' && c <= 'f');
                    });
            if (session.fid.value < FirstNativeFid
                || session.method != domain::session::AuthenticationMethod::Custody
                || session.identityIssuer != NativeIssuer
                || session.identitySubject != expectedSubject
                || !validAddress || session.authenticatedAt > now
                || domain::session::isExpired(session, now)
                || session.expiresAt - now > std::chrono::hours{1}) {
                observer(failedState(foundation::error::Error::make(
                    foundation::error::ErrorCode::Unauthorized,
                    "Verified HyperSnap native FID and custody proof required."),
                    m_impl->current));
                return;
            }
            if (auto saved = m_impl->store.save(session); !saved) {
                observer(failedState(std::move(saved.error()), m_impl->current));
                return;
            }
            m_impl->current = std::move(session);
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(SessionState{
                .phase = SessionPhase::Authenticated,
                .session = m_impl->current
            });
        });
}

foundation::result::Status SessionService::acceptVerifiedFarcasterCustody(
    domain::session::ClientSession session, Observer observer)
{
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            constexpr std::string_view FarcasterIssuer =
                "farcaster:optimism-id-registry";
            const auto now = foundation::time::now();
            const auto subject = "farcaster:" + std::to_string(session.fid.value);
            const auto& address = session.custodyAddress;
            const auto validAddress = address && address->size() == 42
                && address->starts_with("0x")
                && std::all_of(address->begin() + 2, address->end(), [](char c) {
                    return (c >= '0' && c <= '9')
                        || (c >= 'a' && c <= 'f');
                });
            if (session.fid.value == 0 || session.fid.value > 9'007'199'254'740'991ULL
                || session.method != domain::session::AuthenticationMethod::Custody
                || session.identityIssuer != FarcasterIssuer
                || session.identitySubject != subject
                || !validAddress || session.authenticatedAt > now
                || domain::session::isExpired(session, now)
                || session.expiresAt - now > std::chrono::hours{1}) {
                observer(failedState(foundation::error::Error::make(
                    foundation::error::ErrorCode::Unauthorized,
                    "A verified Optimism IdRegistry FID and custody signer are required."),
                    m_impl->current));
                return;
            }
            if (auto saved = m_impl->store.save(session); !saved) {
                observer(failedState(std::move(saved.error()), m_impl->current));
                return;
            }
            m_impl->current = std::move(session);
            m_impl->attempt.reset();
            m_impl->approvalUrl.clear();
            observer(SessionState{
                .phase = SessionPhase::Authenticated,
                .session = m_impl->current
            });
        });
}

foundation::result::Status SessionService::cancelSignIn(Observer observer)
{
    return m_impl->executor.post([this, observer = std::move(observer)]() mutable {
        m_impl->attempt.reset();
        m_impl->approvalUrl.clear();
        observer(m_impl->current
            ? SessionState{.phase = SessionPhase::Authenticated, .session = m_impl->current}
            : guestState());
    });
}

foundation::result::Status SessionService::signOut(Observer observer)
{
    return m_impl->executor.post([this, observer = std::move(observer)]() mutable {
        m_impl->attempt.reset();
        m_impl->approvalUrl.clear();
        if (auto cleared = m_impl->store.clear(); !cleared) {
            observer(failedState(std::move(cleared.error()), m_impl->current));
            return;
        }
        m_impl->current.reset();
        observer(guestState());
    });
}

}
