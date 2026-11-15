/*!
 * @file        ui/app/identity/hypersnap_registration_transport.hpp
 * @brief       HyperSnap native registration transport boundary for Qt clients.
 *
 * @details
 * Defines shard-zero read and single-use registration transport contracts.
 * Keeps registry verification independent of presentation decisions.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>

/**
 * Transport-only HyperSnap native registration adapter. No custody keys,
 * mnemonic or API credentials exist in this class or in its callbacks.
 *
 * A successful POST is *not* registration: it is "accepted for processing".
 * Confirm registration with an independent lookup on the same node.
 */
class HyperSnapRegistrationTransport final : public QObject {
    Q_OBJECT
public:
    enum class LookupState : std::uint8_t {
        Confirmed,
        NotFound,
        Unavailable,
        InvalidResponse
    };
    struct LookupResult final {
        LookupState state{LookupState::Unavailable};
        std::uint64_t fid{};
        std::uint64_t blockNumber{};
        // Zero when no HTTP response was received (e.g. DNS/TLS/timeout).
        int httpStatus{};
    };
    enum class SubmitState : std::uint8_t {
        AcceptedPending,
        Rejected,
        UnknownOutcome
    };
    struct SubmitResult final {
        SubmitState state{SubmitState::UnknownOutcome};
        // HTTP status only. Never expose untrusted node responses or secrets.
        // Zero means that there was no usable HTTP response.
        int httpStatus{};
    };
    using LookupCallback = std::function<void(LookupResult)>;
    using SubmitCallback = std::function<void(SubmitResult)>;

    explicit HyperSnapRegistrationTransport(QUrl base, QObject* parent = nullptr);

    /** Read-only authoritative lookup; never treats HTTP 200 alone as identity. */
    void lookupByCustody(const QString& address, LookupCallback onResult);

    /** Explicitly initiated binary native-onboard broadcast; no retry. */
    void submit(const QByteArray& protobufEnvelope, SubmitCallback onResult);

    [[nodiscard]] bool endpointIsSafe() const noexcept;
    [[nodiscard]] QUrl nodeEndpoint() const { return m_base; }

private:
    QNetworkAccessManager m_network{};
    QUrl m_base{};
};
