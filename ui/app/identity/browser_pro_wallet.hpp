/*!
 * @file        ui/app/identity/browser_pro_wallet.hpp
 * @brief       Ephemeral, click-initiated EIP-1193 checkout in WebAssembly.
 *
 * @details
 * The browser wallet only returns transaction hashes. The account server
 * independently verifies receipts and decides membership; no token or seed
 * material is persisted or passed through this adapter. Never use a Mini App
 * origin grant for a first-party subscription.
 *
 * @author      Kambiz Asadzadeh
 * @since       10 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#if defined(Q_OS_WASM)

#include "pro_checkout.hpp"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <emscripten.h>

#include <array>
#include <utility>

namespace genycaster::ui::identity {

// A bounded, asynchronous event queue bridges EIP-1193 Promises to Qt's
// single-threaded event loop. No browser-native private keys cross this queue.
class BrowserProWallet final : public QObject {
public:
    explicit BrowserProWallet(ProCheckoutController& checkout)
        : QObject{&checkout}
    {
        m_timer.setParent(this);
        m_timer.setInterval(65);
        connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
        checkout.setWalletTransport(
            [] {
                return EM_ASM_INT({
                    return !!(window.ethereum && typeof window.ethereum.request === 'function');
                }) != 0;
            },
            [this](QString method, QJsonArray parameters,
                   ProCheckoutController::RpcComplete complete) {
                request(std::move(method), std::move(parameters), std::move(complete));
            },
            [this] {
                // A wallet approval dialog may outlive the app page; ignore
                // callbacks from abandoned sessions and never auto-retry.
                m_pending.clear();
                m_timer.stop();
            });
    }
private:
    void request(QString method, QJsonArray parameters,
                 ProCheckoutController::RpcComplete complete)
    {
        static const QStringList methods{
            QStringLiteral("eth_requestAccounts"),
            QStringLiteral("eth_chainId"),
            QStringLiteral("wallet_switchEthereumChain"),
            QStringLiteral("eth_sendTransaction")
        };
        if (!methods.contains(method) || m_pending.size() >= 4) {
            complete({}, QStringLiteral("wallet_method_unavailable"));
            return;
        }
        const int id = ++m_requestId;
        m_pending.insert(id, std::move(complete));
        const QByteArray name = method.toUtf8();
        const QByteArray encoded = QJsonDocument{parameters}.toJson(QJsonDocument::Compact);
        // This code executes on an explicit Qt button event. Do not delay the
        // first provider.request() behind a JS microtask or popup may be denied.
        EM_ASM({
            const id = $0;
            const method = UTF8ToString($1);
            const json = UTF8ToString($2);
            // C preprocessor macro arguments must not contain bare commas in
            // unparenthesized JS object literals inside EM_ASM.
            const state = window.__genycasterProWalletRuntime ||=
                ({responses: [], pending: new Set()});
            state.pending.add(id);
            function finish(value, failure) {
                if (!state.pending.delete(id)) return;
                if (state.responses.length >= 16) state.responses.shift();
                state.responses.push({id, value, error:failure || ""});
            }
            if (!['eth_requestAccounts','eth_chainId',
                  'wallet_switchEthereumChain','eth_sendTransaction'].includes(method)) {
                finish(null,'wallet_method_unavailable');
                return;
            }
            const provider = window.ethereum;
            if (!provider || typeof provider.request !== 'function') {
                finish(null,'wallet_unavailable');
                return;
            }
            let params;
            try { params = JSON.parse(json); }
            catch (_) { finish(null,'wallet_invalid_request'); return; }
            let promise;
            try { promise = provider.request({method,params}); }
            catch (error) {
                finish(null, error && error.code === 4001
                    ? 'wallet_rejected' : 'wallet_failed');
                return;
            }
            Promise.resolve(promise).then(
                value => finish(value,null),
                error => finish(null,error && error.code === 4001
                    ? 'wallet_rejected' : 'wallet_failed'));
        }, id, name.constData(), encoded.constData());
        m_timer.start();
    }
    void poll()
    {
        for (int count = 0; count < 4; ++count) {
            std::array<char, 4096> buffer{};
            const int received = EM_ASM_INT({
                const state = window.__genycasterProWalletRuntime;
                if (!state || !state.responses.length) return 0;
                const response = state.responses.shift();
                let document;
                try { document = JSON.stringify(response); }
                catch (_) { return -1; }
                if (lengthBytesUTF8(document) + 1 > $1) return -1;
                stringToUTF8(document, $0, $1);
                return 1;
            }, buffer.data(), buffer.size());
            if (received == 0) break;
            if (received < 0) continue;
            const auto document = QJsonDocument::fromJson(buffer.data());
            if (!document.isObject()) continue;
            const auto object = document.object();
            const int id = object.value(QStringLiteral("id")).toInt();
            auto it = m_pending.find(id);
            if (it == m_pending.end()) continue;
            auto completion = std::move(it.value());
            m_pending.erase(it);
            completion(object.value(QStringLiteral("value")),
                       object.value(QStringLiteral("error")).toString());
        }
        if (m_pending.isEmpty()) m_timer.stop();
    }

    int m_requestId{};
    QHash<int, ProCheckoutController::RpcComplete> m_pending{};
    QTimer m_timer{};
};

} // namespace genycaster::ui::identity

#endif // Q_OS_WASM
