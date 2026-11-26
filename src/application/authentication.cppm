/*!
 * @file        src/application/authentication.cppm
 * @brief       Provider-neutral authentication lifecycle policy.
 *
 * @details
 * Coordinates account-scoped signer and cloud revalidation, retry backoff
 * and cancellation of stale local session restoration without network I/O.
 *
 * @author      Kambiz Asadzadeh
 * @since       26 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
module;
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

export module farcaster.application.authentication;

import farcaster.application.session;
import farcaster.foundation.error;

export namespace farcaster::application::authentication {

enum class Trigger { Timer, Foreground, ConnectivityRestored, ManualSigner };
struct Snapshot final {
    std::string principal{};
    bool offline{};
    bool signerBusy{};
    bool writeBusy{};
    bool signerReady{};
    bool signerRetryable{};
    bool cloudReady{};
    bool cloudBusy{};
};
struct Actions final { bool revalidateSigner{}; bool restoreCloud{}; };

class AuthenticationCoordinator final {
public:
    using Clock=std::chrono::steady_clock;
    using Time=Clock::time_point;
    using Ticket=std::uint64_t;

    [[nodiscard]] Actions plan(Trigger trigger,const Snapshot& snapshot,Time now=Clock::now())
    {
        const std::scoped_lock lock{m_mutex};
        if(snapshot.principal!=m_principal) {
            m_principal=snapshot.principal;
            m_signerDue=now;
            m_cloudDue=now;
            m_signerDelay=std::chrono::seconds{30};
            m_cloudDelay=std::chrono::seconds{30};
        }
        if(m_principal.empty()||snapshot.offline)return {};
        if(snapshot.signerReady)m_signerDelay=std::chrono::seconds{30};
        if(snapshot.cloudReady)m_cloudDelay=std::chrono::seconds{30};
        const bool manual=trigger==Trigger::ManualSigner;
        const bool reconnect=trigger==Trigger::ConnectivityRestored;
        Actions result{};
        if(!snapshot.signerBusy&&!snapshot.writeBusy&&
           (manual||snapshot.signerReady||snapshot.signerRetryable)&&
           (manual||reconnect||now>=m_signerDue)) {
            result.revalidateSigner=true;
            m_signerDue=now+(snapshot.signerReady?std::chrono::seconds{300}:m_signerDelay);
            if(!snapshot.signerReady)m_signerDelay=backoff(m_signerDelay);
        }
        if(!manual&&!snapshot.cloudBusy&&(reconnect||now>=m_cloudDue)) {
            result.restoreCloud=true;
            m_cloudDue=now+(snapshot.cloudReady?std::chrono::seconds{300}:m_cloudDelay);
            if(!snapshot.cloudReady)m_cloudDelay=backoff(m_cloudDelay);
        }
        return result;
    }

    [[nodiscard]] std::optional<Ticket> beginRestore(Time now=Clock::now())
    {
        const std::scoped_lock lock{m_mutex};
        if(!m_restoreAllowed||m_restoreInFlight||now<m_restoreDue)return std::nullopt;
        m_restoreInFlight=true;
        return ++m_restoreTicket;
    }

    [[nodiscard]] bool completeRestore(Ticket ticket,const session::SessionState& state,
                                      Time now=Clock::now())
    {
        const std::scoped_lock lock{m_mutex};
        if(ticket!=m_restoreTicket||!m_restoreInFlight)return false;
        m_restoreInFlight=false;
        using Code=foundation::error::ErrorCode;
        m_restoreAllowed=state.phase==session::SessionPhase::Failed&&state.error&&
            (state.error->code==Code::Unavailable||state.error->code==Code::StorageFailure||
             state.error->code==Code::Unauthorized||state.error->code==Code::Timeout||
             state.error->code==Code::TransportFailure);
        m_restoreDue=now+m_restoreDelay;
        m_restoreDelay=backoff(m_restoreDelay);
        return true;
    }

    void abandonRestore(Ticket ticket,Time now=Clock::now())
    {
        const std::scoped_lock lock{m_mutex};
        if(ticket!=m_restoreTicket)return;
        m_restoreInFlight=false;
        m_restoreDue=now+m_restoreDelay;
        m_restoreDelay=backoff(m_restoreDelay);
    }

    void cancelRestore()
    {
        const std::scoped_lock lock{m_mutex};
        ++m_restoreTicket;
        m_restoreInFlight=false;
        m_restoreAllowed=false;
    }

private:
    static std::chrono::seconds backoff(std::chrono::seconds delay)
    {return std::min(delay*2,std::chrono::seconds{300});}
    std::mutex m_mutex{};
    std::string m_principal{};
    Time m_signerDue{},m_cloudDue{},m_restoreDue{};
    std::chrono::seconds m_signerDelay{30},m_cloudDelay{30},m_restoreDelay{30};
    Ticket m_restoreTicket{};
    bool m_restoreAllowed{true},m_restoreInFlight{};
};

} // namespace farcaster::application::authentication
