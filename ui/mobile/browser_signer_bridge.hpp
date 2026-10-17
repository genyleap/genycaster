#pragma once

// Qt/Emscripten asynchronous bridge. It never blocks on a browser Promise,
// exports a secret, or treats login as signer permission. The pre-js module
// produces bounded JSON events which are polled on the Qt GUI event loop.
#if defined(__EMSCRIPTEN__)

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QtTranslation>

#include <array>
#include <emscripten.h>

#include "presentation_bridge.hpp"

namespace genycaster::ui::browser_signing {

class Bridge final : public QObject {
public:
    explicit Bridge(PresentationBridge& presentation, QString snapchainOrigin)
        : QObject{&presentation},
          m_presentation{&presentation},
          m_snapchainOrigin{std::move(snapchainOrigin)}
    {
        m_timer.setInterval(100);
        connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
        m_timer.start();
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
            if (!presentation->isAuthenticated()
                || fid != QString::number(presentation->accountFid())) continue;

            const QString error = event.value(QStringLiteral("error")).toString();
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
                    presentation->applyVerifiedBrowserWrite(
                        hash, operation == QStringLiteral("cast")
                            || operation == QStringLiteral("thread"));
                    presentation->retryAutomatically();
                } else {
                    presentation->applyWriteState(
                        QStringLiteral("failed"), false, {}, error);
                }
            }
        }
    }

    QPointer<PresentationBridge> m_presentation{};
    QString m_snapchainOrigin{};
    QTimer m_timer{};
};

} // namespace genycaster::ui::browser_signing

#endif
