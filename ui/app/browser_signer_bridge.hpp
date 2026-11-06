/*!
 * @file        ui/app/browser_signer_bridge.hpp
 * @brief       WebAssembly protocol signer event bridge.
 *
 * @details
 * Bridges bounded asynchronous browser messages to Qt and separates
 * sign-in state from explicit authorization to sign a protocol message.
 *
 * @author      Kambiz Asadzadeh
 * @since       17 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

// Qt/Emscripten asynchronous bridge. It never blocks on a browser Promise,
// exports a secret, or treats login as signer permission. The pre-js module
// produces bounded JSON events which are polled on the Qt GUI event loop.
#if defined(__EMSCRIPTEN__)

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QtTranslation>

#include <array>
#include <functional>
#include <utility>
#include <emscripten.h>
#include <QUuid>

#include "presentation_bridge.hpp"
#include "rich_composer_controller.hpp"

namespace genycaster::ui::browser_signing {

class Bridge final : public QObject {
public:
    explicit Bridge(PresentationBridge& presentation, RichComposerController& composer,
                    QString snapchainOrigin, QString recoveryGatewayOrigin)
        : QObject{&presentation},
          m_presentation{&presentation},
          m_composer{&composer},
          m_snapchainOrigin{std::move(snapchainOrigin)},
          m_recoveryGatewayOrigin{std::move(recoveryGatewayOrigin)}
    {
        m_timer.setInterval(100);
        connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
        m_timer.start();
    }

    using RecoveryObserver = std::function<void(
        QString, QString, bool, bool, QString, QString, QByteArray, int)>;
    using AccountSessionObserver = std::function<void(qulonglong, QByteArray)>;
    void setAccountSessionObserver(AccountSessionObserver observer)
    {
        m_accountSessionObserver=std::move(observer);
    }
    void setRecoveryObserver(RecoveryObserver observer)
    {
        m_recoveryObserver = std::move(observer);
    }
    void restoreRememberedAccount()
    {
        m_pendingResumeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        start(QStringLiteral("resume-account"), {},
              QJsonObject{{QStringLiteral("requestId"), m_pendingResumeId}});
    }
    void forgetAccount()
    {
        m_pendingResumeId.clear();
        m_pendingRecoveryId.clear();
        start(QStringLiteral("forget-account"), {});
    }
    void startRecovery()
    {
        // One-shot nonce prevents delayed responses from an older setup
        // from establishing a fresh identity after cancellation.
        m_pendingRecoveryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        start(QStringLiteral("recover-account"), {},
              QJsonObject{{QStringLiteral("requestId"), m_pendingRecoveryId},
                          {QStringLiteral("openProofGatewayOrigin"), m_recoveryGatewayOrigin}});
    }
    void cancelRecovery()
    {
        m_pendingRecoveryId.clear();
        m_pendingResumeId.clear();
        start(QStringLiteral("signed-out"), {});
    }

    void start(const QString& operation, const QString& fid,
               const QJsonObject& payload = {})
    {
        const QByteArray action = operation.toUtf8();
        const QByteArray identity = fid.toUtf8();
        const QByteArray base = m_snapchainOrigin.toUtf8();
        const QByteArray data = QJsonDocument{payload}.toJson(QJsonDocument::Compact);
        EM_ASM({
            const controller = window.__genycasterBrowserSigning;
            if (controller && typeof controller.start === 'function') {
                controller.start(UTF8ToString($0, $1),
                    UTF8ToString($2, $3), UTF8ToString($4, $5),
                    UTF8ToString($6, $7));
            } else {
                console.error('Genycaster browser signer module is missing');
            }
        }, action.constData(), action.size(), identity.constData(), identity.size(),
           base.constData(), base.size(), data.constData(), data.size());
    }

private:
    void poll()
    {
        // A fixed-size buffer prevents untrusted JS response data from
        // allocating unbounded C++ memory or blocking the UI event loop.
        for (int processed=0;processed<5;++processed) {
            std::array<char, 8192> buffer{};
            const int received = EM_ASM_INT({
                const bridge = window.__genycasterBrowserSigning;
                if (!bridge || !bridge.events.length) return 0;
                const payload = JSON.stringify(bridge.events.shift());
                if (lengthBytesUTF8(payload) + 1 > $1) return -1;
                stringToUTF8(payload, $0, $1);
                return 1;
            }, buffer.data(), buffer.size());
            if (received == 0) return;
            if (received < 0) continue;

            const auto document = QJsonDocument::fromJson(buffer.data());
            if (!document.isObject()) continue;
            const auto event = document.object();
            PresentationBridge* const presentation = m_presentation.data();
            if (!presentation) return;
            const QString phase = event.value(QStringLiteral("phase")).toString();
            const QString kind = event.value(QStringLiteral("kind")).toString();
            const QString fid = event.value(QStringLiteral("fid")).toString();
            if (kind == QStringLiteral("setup-account")) {
                const QString requestId = event.value(QStringLiteral("requestId")).toString();
                const bool restoredDevice = !m_pendingResumeId.isEmpty()
                    && requestId == m_pendingResumeId;
                const bool enteredRecovery = !m_pendingRecoveryId.isEmpty()
                    && requestId == m_pendingRecoveryId;
                if (!restoredDevice && !enteredRecovery) continue;
                if (restoredDevice != event.value(QStringLiteral("localDevice")).toBool())
                    continue;
                const QString outcome = event.value(QStringLiteral("phase")).toString();
                if (outcome != QStringLiteral("ready")
                    && outcome != QStringLiteral("failed")
                    && outcome != QStringLiteral("cancelled"))
                    continue;
                bool valid = false;
                const qulonglong recoveredFid = fid.toULongLong(&valid);
                const bool verified = outcome == QStringLiteral("ready")
                    && valid && recoveredFid != 0U && !presentation->isAuthenticated();
                const QString error = outcome == QStringLiteral("cancelled")
                    ? QString{} : verified ? QString{}
                    : event.value(QStringLiteral("error")).toString();
                if (restoredDevice) m_pendingResumeId.clear();
                else m_pendingRecoveryId.clear();
                if (m_recoveryObserver) {
                    m_recoveryObserver(verified ? fid : QString{},
                        error.isEmpty() && outcome != QStringLiteral("cancelled") && !verified
                            ? QStringLiteral("Farcaster recovery was not verified.") : error,
                        verified, restoredDevice,
                        enteredRecovery && verified
                            ? event.value(QStringLiteral("identitySubject")).toString() : QString{},
                        enteredRecovery && verified
                            ? event.value(QStringLiteral("identityIssuer")).toString() : QString{},
                        enteredRecovery && verified
                            ? event.value(QStringLiteral("accountTicket")).toString().toUtf8() : QByteArray{},
                        enteredRecovery && verified
                            ? event.value(QStringLiteral("accountExpiresSeconds")).toInt() : 0);
                }
                if (restoredDevice) {
                    // SessionObserver queued its verified state on this same
                    // GUI receiver. Resolve the splash *after* that commit so
                    // a provisional Guest never flashes Setup on returning users.
                    QMetaObject::invokeMethod(presentation,
                        [presentation] { presentation->finishBrowserAccountRestore(); },
                        Qt::QueuedConnection);
                }
                continue;
            }
            if (!presentation->isAuthenticated()
                || fid != QString::number(presentation->accountFid())) continue;

            const QString error = event.value(QStringLiteral("error")).toString();
            if(kind==QStringLiteral("account-session")) {
                if(phase==QStringLiteral("ready") && m_accountSessionObserver) {
                    const QByteArray csrf=event.value(QStringLiteral("csrfToken"))
                                               .toString().toUtf8();
                    if(csrf.size()==43)
                        m_accountSessionObserver(presentation->accountFid(),csrf);
                }
                continue;
            }
            if (kind == QStringLiteral("signer")) {
                QString status{};
                if (phase == QStringLiteral("ready")) {
                    status = qtTrId("genycaster.runtime.interaction_signing_is_ready");
                } else if (phase == QStringLiteral("checking")) {
                    status = qtTrId("genycaster.runtime.checking_the_protocol_signer_against_the_network");
                } else if (phase == QStringLiteral("provisioning")) {
                    status = qtTrId("genycaster.runtime.waiting_for_custody_approval_and_network_confirmation");
                } else if (phase == QStringLiteral("authorization-required")
                    || phase == QStringLiteral("missing")) {
                    status = qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet");
                } else if (phase == QStringLiteral("revoked")) {
                    status = qtTrId("genycaster.runtime.the_protocol_signer_has_been_revoked");
                } else {
                    status = qtTrId("genycaster.runtime.interaction_signing_is_temporarily_unavailable_reading_is_un");
                }
                const bool busy = event.value(QStringLiteral("busy")).toBool();
                const bool ready = !busy && phase == QStringLiteral("ready")
                    && event.value(QStringLiteral("ready")).toBool();
                presentation->applySignerState(phase, busy, ready, status, error);
            } else if (kind == QStringLiteral("write")) {
                if (phase == QStringLiteral("submitting")) {
                    presentation->applyWriteState(
                        QStringLiteral("submitting"), true, {}, {});
                } else if (phase == QStringLiteral("confirmed")) {
                    const auto hash = event.value(QStringLiteral("hash")).toString();
                    const auto operation = event.value(QStringLiteral("operation")).toString();
                    if (operation == QStringLiteral("profile")) {
                        // UserDataAdd is not a CastAdd. Profile reads may lag
                        // indexing; refresh from the provider after confirmation.
                        presentation->applyWriteState(
                            QStringLiteral("confirmed"), false, {}, {});
                        emit presentation->protocolProfileWriteCompleted(true, {});
                        presentation->openMyProfile();
                    } else if (operation == QStringLiteral("follow")
                               || operation == QStringLiteral("block")) {
                        bool valid = false;
                        const qulonglong targetFid =
                            event.value(QStringLiteral("targetFid")).toString().toULongLong(&valid);
                        if (!valid || targetFid == 0 || targetFid == presentation->accountFid()) {
                            presentation->applyWriteState(
                                QStringLiteral("failed"), false, {},
                                QStringLiteral("Confirmed social link has no valid target."));
                        } else {
                            const bool active = event.value(QStringLiteral("active")).toBool();
                            presentation->applyWriteState(
                                QStringLiteral("confirmed"), false, {}, {},
                                QVariantMap{
                                    {QStringLiteral("operationKind"),
                                     operation == QStringLiteral("block")
                                         ? (active ? QStringLiteral("block") : QStringLiteral("unblock"))
                                         : (active ? QStringLiteral("follow") : QStringLiteral("unfollow"))},
                                    {QStringLiteral("targetFid"), targetFid},
                                });
                        }
                    } else if (operation == QStringLiteral("remove")) {
                        const QString castId = event.value(QStringLiteral("targetCastId")).toString();
                        // A confirmed CastRemove has its *own* message hash.
                        // Use the original target hash for the removal mutation,
                        // never the CastRemove message hash.
                        if (castId.size() == 42 && castId.startsWith(QStringLiteral("0x"))) {
                            presentation->applyWriteState(
                                QStringLiteral("confirmed"), false, {}, {},
                                QVariantMap{
                                    {QStringLiteral("operationKind"), QStringLiteral("remove-cast")},
                                    {QStringLiteral("targetCastId"), castId},
                                });
                            if (m_composer) {
                                // Only after protocol confirmation should
                                // managed-media retention be released.
                                (void)m_composer->releaseManagedAssetsForCast(castId);
                            }
                        } else {
                            presentation->applyWriteState(
                                QStringLiteral("failed"), false, {},
                                QStringLiteral("Confirmed deletion lacked a valid cast reference"));
                        }
                    } else {
                        presentation->applyVerifiedBrowserWrite(
                            hash, operation == QStringLiteral("cast")
                                || operation == QStringLiteral("thread"));
                    }
                    presentation->retryAutomatically();
                } else {
                    presentation->applyWriteState(
                        QStringLiteral("failed"), false, {}, error);
                    if (event.value(QStringLiteral("operation")).toString()
                        == QStringLiteral("profile"))
                        emit presentation->protocolProfileWriteCompleted(false, error);
                }
            }
        }
    }

    RecoveryObserver m_recoveryObserver{};
    AccountSessionObserver m_accountSessionObserver{};
    QString m_pendingRecoveryId{};
    QString m_pendingResumeId{};
    QPointer<PresentationBridge> m_presentation{};
    QPointer<RichComposerController> m_composer{};
    QString m_snapchainOrigin{};
    QString m_recoveryGatewayOrigin{};
    QTimer m_timer{};
};

} // namespace genycaster::ui::browser_signing

#endif
