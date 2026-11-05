module;

#include "miniapp_catalog_controller.hpp"
#include "miniapp_host_controller.hpp"
#include "miniapp_browser.hpp"
#include "miniapp_task_manager_controller.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QThreadPool>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QtTranslation>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

module farcaster.runtime.miniapp;

import farcaster.capability.wallet;
#if GENYCASTER_MINIAPP_SIWF
import farcaster.provider.compatibility.auth;
import farcaster.transport.qtnetwork;
import farcaster.domain.identity;
#endif

namespace farcaster::runtime::miniapp {
namespace {

constexpr qsizetype manifestByteLimit = 64 * 1024;
constexpr qsizetype universalPageByteLimit = 256 * 1024;
constexpr qsizetype rpcByteLimit = 32 * 1024;
constexpr double largestExactInteger = 9007199254740991.0;
constexpr int manifestTimeoutMs = 10'000;
constexpr int readyTimeoutMs = 20'000;
constexpr qint64 manifestCacheTtlMs = 5 * 60 * 1000;

[[nodiscard]] QJsonArray capabilities(const capability::wallet::IEthereumWalletBridge* wallet,
                                      const bool supportsSignIn = false)
{
    QJsonArray result{
        QStringLiteral("actions.ready"),
        QStringLiteral("actions.close"),
        QStringLiteral("actions.viewProfile"),
        QStringLiteral("actions.viewCast"),
        QStringLiteral("actions.openMiniApp"),
        QStringLiteral("actions.composeCast"),
        QStringLiteral("back"),
    };
    if (wallet != nullptr && wallet->available() && !wallet->supportedChains().empty()) {
        result.push_back(QStringLiteral("wallet.getEthereumProvider"));
    }
    if (supportsSignIn && wallet != nullptr && wallet->available())
        result.push_back(QStringLiteral("actions.signIn"));
    return result;
}

[[nodiscard]] QJsonArray chains(const capability::wallet::IEthereumWalletBridge* wallet)
{
    QJsonArray result{};
    if (wallet == nullptr || !wallet->available()) {
        return result;
    }
    for (const auto& chain : wallet->supportedChains()) {
        result.push_back(QString::fromStdString(chain));
    }
    return result;
}

[[nodiscard]] QJsonValue parseJsonValue(const std::string& json)
{
    const QByteArray wrapped = QByteArrayLiteral("[") + QByteArray::fromStdString(json) + QByteArrayLiteral("]");
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(wrapped, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray() || document.array().size() != 1) {
        return QJsonValue{};
    }
    return document.array().first();
}

[[nodiscard]] QString ethereumEventName(const capability::wallet::EthereumWalletEventKind kind)
{
    using Kind = capability::wallet::EthereumWalletEventKind;
    switch (kind) {
    case Kind::AccountsChanged: return QStringLiteral("accountsChanged");
    case Kind::ChainChanged: return QStringLiteral("chainChanged");
    case Kind::Connect: return QStringLiteral("connect");
    case Kind::Disconnect: return QStringLiteral("disconnect");
    case Kind::Message: return QStringLiteral("message");
    }
    return QStringLiteral("message");
}

[[nodiscard]] QString unsupportedRequirementsText(const QStringList& missingCapabilities,
                                                  const QStringList& missingChains)
{
    QString text = qtTrId("genycaster.miniapp.unsupported_requirements");
    if (!missingCapabilities.isEmpty()) {
        text += QStringLiteral("\nrequiredCapabilities: ") + missingCapabilities.join(QStringLiteral(", "));
    }
    if (!missingChains.isEmpty()) {
        text += QStringLiteral("\nrequiredChains: ") + missingChains.join(QStringLiteral(", "));
    }
    return text;
}

[[nodiscard]] bool publicHttps(const QUrl& url)
{
    const QString host = url.host().toLower();
    QHostAddress literal;
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.userInfo().isEmpty() && url.port(443) == 443
        && host.contains(QLatin1Char('.')) && !host.endsWith(QLatin1Char('.'))
        && !host.endsWith(QStringLiteral(".localhost"))
        && !host.endsWith(QStringLiteral(".local"))
        && !host.endsWith(QStringLiteral(".internal"))
        && !host.endsWith(QStringLiteral(".test")) && !literal.setAddress(host);
}

[[nodiscard]] bool farcasterUniversalLink(const QUrl& url)
{
    return publicHttps(url)
        && url.host().compare(QStringLiteral("farcaster.xyz"), Qt::CaseInsensitive) == 0
        && url.path().startsWith(QStringLiteral("/miniapps/"));
}

[[nodiscard]] QJsonObject rawResponse(const QJsonValue& id,
                                      const QJsonValue& value = QJsonValue::Undefined)
{
    QJsonObject result{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("RAW")}};
    if (!value.isUndefined()) {
        result.insert(QStringLiteral("value"), value);
    }
    return result;
}

[[nodiscard]] QJsonObject errorResponse(const QJsonValue& id, const QString& text)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("HANDLER")},
            {QStringLiteral("name"), QStringLiteral("throw")},
            {QStringLiteral("value"), QJsonObject{
                {QStringLiteral("isError"), true},
                {QStringLiteral("value"), QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("NotSupportedError")},
                    {QStringLiteral("message"), text}}}}}};
}

[[nodiscard]] QUrl manifestUrlForHost(const QUrl& appUrl)
{
    QUrl manifest = appUrl;
    manifest.setPath(QStringLiteral("/.well-known/farcaster.json"));
    manifest.setQuery(QString{});
    manifest.setFragment(QString{});
    return manifest;
}

[[nodiscard]] QString htmlAttribute(const QString& tag, const QString& attribute)
{
    const QRegularExpression expression{
        QStringLiteral(R"((?:^|\s)%1\s*=\s*(["'])(.*?)\1)")
            .arg(QRegularExpression::escape(attribute)),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    const auto match = expression.match(tag);
    return match.hasMatch() ? match.captured(2).trimmed() : QString{};
}

[[nodiscard]] QString universalDomain(const QByteArray& bytes)
{
    const QString html = QString::fromUtf8(bytes);
    const QRegularExpression metaExpression{
        QStringLiteral(R"(<meta\b[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    auto matches = metaExpression.globalMatch(html);
    while (matches.hasNext()) {
        const QString tag = matches.next().captured(0);
        const QString name = htmlAttribute(tag, QStringLiteral("name"));
        if (name.compare(QStringLiteral("fc:miniapp:domain"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        QString domain = htmlAttribute(tag, QStringLiteral("content")).toLower();
        if (domain.startsWith(QStringLiteral("https://"))) {
            domain = QUrl{domain}.host().toLower();
        }
        if (domain.contains(QLatin1Char('/')) || domain.contains(QLatin1Char(':'))) {
            return {};
        }
        const QUrl origin{QStringLiteral("https://") + domain};
        return publicHttps(origin) ? domain : QString{};
    }
    return {};
}

[[nodiscard]] platform::miniapp::PresentationMode presentationMode(const QString& mode)
{
    using platform::miniapp::PresentationMode;
    if (mode == QStringLiteral("bottom")) {
        return PresentationMode::Bottom;
    }
    if (mode == QStringLiteral("left")) {
        return PresentationMode::Left;
    }
    if (mode == QStringLiteral("right")) {
        return PresentationMode::Right;
    }
    return PresentationMode::Window;
}

[[nodiscard]] QString normalizedUrl(const QUrl& url)
{
    QUrl normalized = url;
    normalized.setFragment(QString{});
    return normalized.toString(QUrl::FullyEncoded);
}

[[nodiscard]] QJsonObject decodedBase64UrlObject(const QJsonValue& encodedValue)
{
    if (!encodedValue.isString()) {
        return {};
    }
    const QByteArray encoded = encodedValue.toString().toLatin1();
    if (encoded.isEmpty() || encoded.size() > 16 * 1024) {
        return {};
    }
    const QByteArray decoded = QByteArray::fromBase64(
        encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(decoded, &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject{};
}

[[nodiscard]] bool validAccountAssociationEnvelope(const QJsonObject& manifest,
                                                    const QString& manifestHost)
{
    const QJsonValue associationValue = manifest.value(QStringLiteral("accountAssociation"));
    if (!associationValue.isObject()) {
        return false;
    }
    const QJsonObject association = associationValue.toObject();
    const QString signature = association.value(QStringLiteral("signature")).toString().trimmed();
    if (signature.isEmpty() || signature.size() > 16 * 1024) {
        return false;
    }

    const QJsonObject header = decodedBase64UrlObject(association.value(QStringLiteral("header")));
    const QJsonObject payload = decodedBase64UrlObject(association.value(QStringLiteral("payload")));
    const QString associationType = header.value(QStringLiteral("type")).toString();
    const double fid = header.value(QStringLiteral("fid")).toDouble();
    const QString signedDomain = payload.value(QStringLiteral("domain")).toString().trimmed();
    return (associationType == QStringLiteral("custody") || associationType == QStringLiteral("auth"))
        && std::isfinite(fid) && fid >= 1.0 && fid <= largestExactInteger && std::floor(fid) == fid
        && !signedDomain.isEmpty()
        && signedDomain.compare(manifestHost, Qt::CaseInsensitive) == 0;
}

[[nodiscard]] QVariantMap catalogueRowFromManifest(const QJsonObject& app, const QUrl& launchUrl)
{
    const QString name = app.value(QStringLiteral("name")).toString().trimmed();
    const QString description = app.value(QStringLiteral("description")).toString().trimmed();
    const QString iconUrl = app.value(QStringLiteral("iconUrl")).toString().trimmed();
    QByteArray encodedIcon{};
    const QUrl icon{iconUrl, QUrl::StrictMode};
    QString iconSource{};
    if (publicHttps(icon)) {
        encodedIcon = icon.toString(QUrl::FullyEncoded).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        iconSource = QStringLiteral("image://remote/") + QString::fromLatin1(encodedIcon);
    }
    return {
        {QStringLiteral("name"), name.left(96)},
        {QStringLiteral("description"), description.left(420)},
        {QStringLiteral("domain"), launchUrl.host().toLower()},
        {QStringLiteral("homeUrl"), normalizedUrl(launchUrl)},
        {QStringLiteral("iconSource"), iconSource},
        {QStringLiteral("requiredChains"), app.value(QStringLiteral("requiredChains")).toArray().toVariantList()},
        {QStringLiteral("requiredCapabilities"), app.value(QStringLiteral("requiredCapabilities")).toArray().toVariantList()},
        {QStringLiteral("source"), QStringLiteral("Farcaster manifest")},
        {QStringLiteral("saved"), false},
    };
}

} // namespace

class MiniAppRuntime::Impl final : public QObject {
public:
    Impl(MiniAppHostController& controller,
         MiniAppCatalogController& catalog,
         MiniAppTaskManagerController& taskManager,
         const bool useDirectProxy,
         const qulonglong clientFid,
         capability::wallet::IEthereumWalletBridge* wallet,
         std::string optimismRpcUrl)
        : m_controller(controller),
          m_catalog(catalog),
          m_taskManager(taskManager),
          m_clientFid(clientFid),
          m_wallet(wallet),
          m_optimismRpcUrl(std::move(optimismRpcUrl)),
          m_useDirectProxy(useDirectProxy),
          m_callbackGate(std::make_shared<CallbackGate>())
    {
        m_callbackGate->runtime = this;
        if (useDirectProxy) {
            m_network.setProxy(QNetworkProxy::NoProxy);
        }

        if (m_wallet != nullptr) {
            const std::weak_ptr<CallbackGate> weakGate = m_callbackGate;
            m_wallet->setEventSink([weakGate](capability::wallet::EthereumWalletEvent event) mutable {
                const auto gate = weakGate.lock();
                if (!gate) {
                    return;
                }
                std::scoped_lock lock{gate->mutex};
                if (gate->runtime == nullptr) {
                    return;
                }
                Impl* const runtime = gate->runtime;
                QMetaObject::invokeMethod(runtime,
                    [runtime, event = std::move(event)] { runtime->forwardEthereumEvent(event); },
                    Qt::QueuedConnection);
            });
        }

        connect(&m_controller, &MiniAppHostController::launchRequested, this,
            [this](const QString& url, const QString& title, const QString& iconSource) {
                launch(url, title, iconSource);
            });
        connect(&m_controller, &MiniAppHostController::closeRequested, this,
            [this] { terminateForeground(); });
        connect(&m_controller, &MiniAppHostController::closeAllRequested, this,
            [this] { terminateAll(); });
        connect(&m_controller, &MiniAppHostController::walletBrowserCancelRequested,
                this, [this] { endWalletSession(); });
        // A native WKWebView drawer is an NSView above the Qt scene graph.
        // QML popup z-order cannot cover it. Reveal the Qt wallet chooser
        // while preserving the live Mini App document and its pending RPC.
        connect(&m_controller, &MiniAppHostController::walletBrowserPickerChanged,
                this, [this] {
                    if (m_controller.walletBrowserPickerActive()) {
                        Session* current = foreground();
                        if (m_walletChooserHiddenTaskId.isEmpty() && current
                            && current->browser && current->taskId == m_walletTaskId) {
                            m_walletChooserHiddenTaskId = current->taskId;
                            current->browser->setVisible(false);
                        }
                        return;
                    }
                    if (m_walletChooserHiddenTaskId.isEmpty()) return;
                    const QString hiddenTask = std::exchange(m_walletChooserHiddenTaskId, QString{});
                    // Let Qt dismiss the modal before restoring the native
                    // surface, preventing an NSView/QML stacking race.
                    QTimer::singleShot(0, this, [this, hiddenTask] {
                        Session* current = foreground();
                        if (current && current->browser && current->taskId == hiddenTask
                            && !m_controller.walletBrowserPickerActive())
                            current->browser->setVisible(true);
                    });
                });
        connect(&m_controller, &MiniAppHostController::composeCastCompleted, this,
            [this](const QString& requestId, const QVariantMap& result) {
                completeComposeCast(requestId, result);
            });
        connect(&m_controller, &MiniAppHostController::composeCastCancelled, this,
            [this](const QString& requestId) { cancelComposeCast(requestId); });
        connect(&m_taskManager, &MiniAppTaskManagerController::focusRequested, this,
            [this](const QString& taskId) {
                // Task Manager is a Qt modal while Mini Apps are native WebKit surfaces.
                // Defer activation by one event turn so closing the modal cannot share
                // the same pointer/focus transaction with the native drawer restore.
                QTimer::singleShot(0, this, [this, taskId] { focus(taskId); });
            });
        connect(&m_taskManager, &MiniAppTaskManagerController::minimizeRequested, this,
            [this](const QString& taskId) { minimize(taskId); });
        connect(&m_taskManager, &MiniAppTaskManagerController::terminateRequested, this,
            [this](const QString& taskId) { terminate(taskId); });
    }

    void setSessionLimit(const int limit)
    {
        const auto normalized = static_cast<std::size_t>(std::clamp(limit,1,4));
        if (m_allowedSessions == normalized) return;
        m_allowedSessions = normalized;
        // A downgrade must close excess isolated documents immediately;
        // remove background sessions first without leaving wallet authority
        // behind. Existing focus/teardown semantics handle active sessions.
        while (m_sessions.size() > m_allowedSessions) {
            auto previous = std::ranges::find_if(m_sessions, [](const auto& session) {
                return !session->foreground;
            });
            const QString target = previous != m_sessions.end()
                ? (*previous)->taskId : m_sessions.back()->taskId;
            terminate(target);
        }
    }

    ~Impl() override
    {
        if (m_wallet != nullptr) {
            m_wallet->setEventSink({});
            if (!m_walletTaskId.isEmpty()) {
                m_wallet->endSession();
            }
        }
        {
            std::scoped_lock lock{m_callbackGate->mutex};
            m_callbackGate->runtime = nullptr;
        }
        for (auto& session : m_sessions) {
            stopReply(*session);
        }
        m_sessions.clear();
    }

private:
    struct PendingCompose final {
        QJsonValue outerId{};
        QString document{};
        QString parentHash{};
        bool closeAfter{};
    };

    struct Session final {
        QString taskId{};
        QString title{};
        QString iconSource{};
        QUrl requestedUrl{};
        QUrl launchUrl{};
        QJsonObject manifestApp{};
        QVariantMap catalogRow{};
        QJsonObject context{};
        std::unique_ptr<platform::miniapp::Browser> browser{};
        QPointer<QNetworkReply> reply{};
        QByteArray responseBytes{};
        QString documentToken{};
        QString providerUuid{};
        QElapsedTimer rateWindow{};
        int messageCount{};
        quint64 documentGeneration{};
        quint64 readyGeneration{};
        QString handoffSourceTaskId{};
        QJsonValue handoffOuterId{};
        QString handoffDocument{};
        quint64 handoffSourceGeneration{};
        QString referrerDomain{};
        bool foreground{};
        bool minimized{};
        bool ready{};
        bool failed{};
        bool pendingSignIn{};
        quint64 signInSequence{};
        QHash<QString, PendingCompose> pendingCompose{};
    };

    struct ManifestCacheEntry final {
        QJsonObject app{};
        QVariantMap catalogRow{};
        QString title{};
        QString iconSource{};
        qint64 expiresAtMs{};
    };

    struct CallbackGate final {
        std::mutex mutex{};
        Impl* runtime{};
    };

    [[nodiscard]] Session* find(const QString& taskId)
    {
        const auto iterator = std::ranges::find_if(m_sessions, [&taskId](const auto& session) {
            return session->taskId == taskId;
        });
        return iterator == m_sessions.end() ? nullptr : iterator->get();
    }

    [[nodiscard]] Session* foreground()
    {
        return m_foregroundTaskId.isEmpty() ? nullptr : find(m_foregroundTaskId);
    }

    [[nodiscard]] bool supportsParallelWindowVisibility() const
    {
        return presentationMode(m_controller.presentationMode())
            == platform::miniapp::PresentationMode::Window;
    }

    [[nodiscard]] QString taskState(const Session& session) const
    {
        if (session.failed) {
            return QStringLiteral("error");
        }
        if (!session.ready) {
            return QStringLiteral("loading");
        }
        if (session.foreground) {
            return QStringLiteral("foreground");
        }
        return session.minimized ? QStringLiteral("minimized")
                                 : QStringLiteral("background");
    }

    [[nodiscard]] Session* findByRequestedUrl(const QUrl& url)
    {
        const QString key = normalizedUrl(url);
        const auto iterator = std::ranges::find_if(m_sessions, [&key](const auto& session) {
            return normalizedUrl(session->requestedUrl) == key || normalizedUrl(session->launchUrl) == key;
        });
        return iterator == m_sessions.end() ? nullptr : iterator->get();
    }

    void updateHostState(const QString& error = {})
    {
        m_controller.setState(foreground() != nullptr, error);
    }

    void updateTask(Session& session, const QString& state)
    {
        const qlonglong pid = session.browser ? session.browser->processIdentifier() : 0;
        m_taskManager.upsertTask(
            session.taskId,
            session.title,
            session.requestedUrl.toString(QUrl::FullyEncoded),
            session.launchUrl.isEmpty() ? session.requestedUrl.toString(QUrl::FullyEncoded)
                                        : session.launchUrl.toString(QUrl::FullyEncoded),
            session.iconSource,
            state,
            session.foreground,
            pid);
    }

    void stopReply(Session& session)
    {
        if (!session.reply) {
            return;
        }
        disconnect(session.reply, nullptr, this, nullptr);
        session.reply->abort();
        session.reply->deleteLater();
        session.reply = nullptr;
        session.responseBytes.clear();
    }

    void endWalletSession()
    {
        if (m_wallet != nullptr && !m_walletTaskId.isEmpty()) {
            m_wallet->endSession();
        }
        m_walletTaskId.clear();
        m_controller.walletBrowserSessionEnded();
    }

    [[nodiscard]] bool useCachedManifest(Session& session, const QUrl& requestedLaunch)
    {
        const QString hostKey = requestedLaunch.host().toLower();
        const auto iterator = m_manifestCache.find(hostKey);
        if (hostKey.isEmpty() || iterator == m_manifestCache.end()) {
            return false;
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const ManifestCacheEntry& cached = iterator.value();
        if (cached.expiresAtMs <= now) {
            m_manifestCache.erase(iterator);
            return false;
        }
        session.manifestApp = cached.app;
        session.launchUrl = requestedLaunch;
        session.catalogRow = cached.catalogRow;
        session.title = cached.title.isEmpty() ? session.title : cached.title;
        if (!cached.iconSource.isEmpty()) {
            session.iconSource = cached.iconSource;
        }
        createBrowser(session);
        return true;
    }

    void rejectOpenMiniAppHandoff(const QString& sourceTaskId,
                                  const quint64 sourceGeneration,
                                  const QJsonValue& outerId,
                                  const QString& document,
                                  const QString& reason)
    {
        Session* source = find(sourceTaskId);
        if (source == nullptr || !source->browser || source->documentGeneration != sourceGeneration) {
            return;
        }
        source->browser->reply(errorResponse(outerId, reason), document);
        focus(sourceTaskId);
    }

    void completeOpenMiniAppHandoff(Session& target)
    {
        if (target.handoffSourceTaskId.isEmpty()) {
            return;
        }

        const QString sourceTaskId = std::exchange(target.handoffSourceTaskId, QString{});
        const QJsonValue outerId = std::exchange(target.handoffOuterId, QJsonValue{});
        const QString document = std::exchange(target.handoffDocument, QString{});
        const quint64 sourceGeneration = std::exchange(target.handoffSourceGeneration, quint64{});

        Session* source = find(sourceTaskId);
        if (source == nullptr || !source->browser || source->documentGeneration != sourceGeneration) {
            return;
        }
        source->browser->reply(rawResponse(outerId), document);
        terminate(sourceTaskId);
    }

    void launch(const QString& rawUrl,
                const QString& title,
                const QString& iconSource = {},
                const QString& handoffSourceTaskId = {},
                const QJsonValue& handoffOuterId = {},
                const QString& handoffDocument = {},
                const quint64 handoffSourceGeneration = 0,
                const QString& referrerDomain = {})
    {
        const QUrl requested{rawUrl, QUrl::StrictMode};
        if (rawUrl.size() > 4096 || !publicHttps(requested)) {
            if (!handoffSourceTaskId.isEmpty()) {
                rejectOpenMiniAppHandoff(handoffSourceTaskId, handoffSourceGeneration,
                                         handoffOuterId, handoffDocument,
                                         QStringLiteral("Invalid Mini App URL"));
            } else {
                updateHostState(qtTrId("genycaster.miniapp.invalid_url"));
            }
            return;
        }
        if (m_clientFid == 0 || m_clientFid > 9007199254740991ULL) {
            if (!handoffSourceTaskId.isEmpty()) {
                rejectOpenMiniAppHandoff(handoffSourceTaskId, handoffSourceGeneration,
                                         handoffOuterId, handoffDocument,
                                         QStringLiteral("Mini App host unavailable"));
            } else {
                updateHostState(qtTrId("genycaster.miniapp.unavailable"));
            }
            return;
        }

        if (Session* existing = findByRequestedUrl(requested)) {
            if (!existing->failed || existing->browser) {
                if (!handoffSourceTaskId.isEmpty()) {
                    Session* source = find(handoffSourceTaskId);
                    if (source != nullptr && source->taskId == existing->taskId) {
                        source->browser->reply(rawResponse(handoffOuterId), handoffDocument);
                        return;
                    }
                    m_focusIntentTaskId = existing->taskId;
                    if (existing->browser) {
                        existing->context[QStringLiteral("location")] = QJsonObject{
                            {QStringLiteral("type"), QStringLiteral("open_miniapp")},
                            {QStringLiteral("referrerDomain"), referrerDomain},
                        };
                        focus(existing->taskId);
                        if (source != nullptr && source->browser
                            && source->documentGeneration == handoffSourceGeneration) {
                            source->browser->reply(rawResponse(handoffOuterId), handoffDocument);
                            terminate(handoffSourceTaskId);
                        }
                    } else {
                        existing->handoffSourceTaskId = handoffSourceTaskId;
                        existing->handoffOuterId = handoffOuterId;
                        existing->handoffDocument = handoffDocument;
                        existing->handoffSourceGeneration = handoffSourceGeneration;
                        existing->referrerDomain = referrerDomain.left(255);
                        updateTask(*existing, QStringLiteral("resolving"));
                    }
                    return;
                }
                m_focusIntentTaskId = existing->taskId;
                if (existing->browser) {
                    focus(existing->taskId);
                } else {
                    updateTask(*existing, QStringLiteral("resolving"));
                }
                return;
            }
            terminate(existing->taskId);
        }
        // A Farcaster SDK openMiniApp handoff is a *replacement* of its
        // source document. Permit one temporary resolving target even on
        // Basic, then terminate the source at ready; never admit a second
        // ordinary launch or a third pending session.
        const bool replacingOnlySession =
            m_allowedSessions == 1U && m_sessions.size() == 1U
            && !handoffSourceTaskId.isEmpty() && find(handoffSourceTaskId);
        if (m_sessions.size() >= m_allowedSessions && !replacingOnlySession) {
            // A failed document may remain in Task Manager for retry/error
            // inspection, but it must not permanently reserve a Pro slot.
            // Reclaim ONLY failed, non-publishing sessions when the user
            // explicitly launches another app. Never evict live sessions.
            while (m_sessions.size() >= m_allowedSessions) {
                const auto failed = std::ranges::find_if(m_sessions, [](const auto& item) {
                    return item->failed && item->pendingCompose.isEmpty();
                });
                if (failed == m_sessions.end()) break;
                const QString failedTaskId = (*failed)->taskId;
                terminate(failedTaskId);
            }
        }
        if (m_sessions.size() >= m_allowedSessions && !replacingOnlySession) {
            if (!handoffSourceTaskId.isEmpty()) {
                rejectOpenMiniAppHandoff(handoffSourceTaskId, handoffSourceGeneration,
                                         handoffOuterId, handoffDocument,
                                         QStringLiteral("Mini App session limit reached"));
            } else {
                updateHostState(qtTrId("genycaster.miniapp.session_limit"));
            }
            return;
        }

        auto session = std::make_unique<Session>();
        session->taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session->requestedUrl = requested;
        session->launchUrl = requested;
        session->title = title.trimmed().left(80);
        if (session->title.isEmpty()) {
            session->title = requested.host();
        }
        session->iconSource = iconSource.trimmed().left(4096);
        session->handoffSourceTaskId = handoffSourceTaskId;
        session->handoffOuterId = handoffOuterId;
        session->handoffDocument = handoffDocument;
        session->handoffSourceGeneration = handoffSourceGeneration;
        session->referrerDomain = referrerDomain.left(255);
        session->foreground = false;
        const QString taskId = session->taskId;
        m_focusIntentTaskId = taskId;
        m_sessions.push_back(std::move(session));
        Session* created = find(taskId);
        if (created == nullptr) {
            return;
        }
        updateTask(*created, QStringLiteral("resolving"));

        if (farcasterUniversalLink(requested)) {
            resolveUniversalLink(*created, requested, 0);
        } else if (!useCachedManifest(*created, requested)) {
            resolveManifest(*created, manifestUrlForHost(requested), 0, requested);
        }
    }

    void fail(Session& session, const QString& error)
    {
        if (!session.handoffSourceTaskId.isEmpty()) {
            const QString targetTaskId = session.taskId;
            const QString sourceTaskId = session.handoffSourceTaskId;
            const quint64 sourceGeneration = session.handoffSourceGeneration;
            const QJsonValue outerId = session.handoffOuterId;
            const QString document = session.handoffDocument;
            terminate(targetTaskId);
            rejectOpenMiniAppHandoff(sourceTaskId, sourceGeneration, outerId, document, error);
            return;
        }
        stopReply(session);
        session.failed = true;
        if (session.browser) {
            session.browser->status(error, true);
        }
        updateTask(session, QStringLiteral("error"));
        if (session.foreground || m_focusIntentTaskId == session.taskId) {
            updateHostState(error);
        }
    }

    void resolveUniversalLink(Session& session, const QUrl& url, const int redirects)
    {
        if (!farcasterUniversalLink(url) || redirects > 3) {
            fail(session, qtTrId("genycaster.miniapp.invalid_manifest"));
            return;
        }
        stopReply(session);
        session.responseBytes.clear();
        QNetworkRequest request{url};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        request.setTransferTimeout(manifestTimeoutMs);
        request.setRawHeader("Accept", "text/html,application/xhtml+xml");
        session.reply = m_network.get(request);
        session.reply->setReadBufferSize(universalPageByteLimit + 1);
        QNetworkReply* const reply = session.reply;
        const QString taskId = session.taskId;
        connect(reply, &QIODevice::readyRead, this, [this, taskId, reply] {
            Session* active = find(taskId);
            if (active == nullptr || active->reply != reply) {
                return;
            }
            active->responseBytes += reply->read(universalPageByteLimit + 1 - active->responseBytes.size());
            if (active->responseBytes.size() > universalPageByteLimit) {
                fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
            }
        });
        connect(reply, &QNetworkReply::finished, this, [this, taskId, reply, url, redirects] {
            Session* active = find(taskId);
            if (active == nullptr || active->reply != reply) {
                reply->deleteLater();
                return;
            }
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            const auto error = reply->error();
            active->responseBytes += reply->read(universalPageByteLimit + 1 - active->responseBytes.size());
            active->reply = nullptr;
            reply->deleteLater();
            if (status >= 300 && status < 400 && !redirect.isEmpty()) {
                resolveUniversalLink(*active, url.resolved(redirect), redirects + 1);
                return;
            }
            if (error != QNetworkReply::NoError || status != 200
                || active->responseBytes.size() > universalPageByteLimit) {
                fail(*active, qtTrId("genycaster.miniapp.load_failed"));
                return;
            }
            const QString domain = universalDomain(active->responseBytes);
            active->responseBytes.clear();
            if (domain.isEmpty()) {
                fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                return;
            }
            const QUrl origin{QStringLiteral("https://") + domain};
            resolveManifest(*active, manifestUrlForHost(origin), 0, origin);
        });
    }

    void resolveManifest(Session& session,
                         const QUrl& url,
                         const int redirects,
                         const QUrl& requestedLaunch)
    {
        if (!publicHttps(url) || redirects > 3) {
            fail(session, qtTrId("genycaster.miniapp.invalid_manifest"));
            return;
        }
        stopReply(session);
        session.responseBytes.clear();
        QNetworkRequest request{url};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
        request.setTransferTimeout(manifestTimeoutMs);
        request.setRawHeader("Accept", "application/json");
        session.reply = m_network.get(request);
        session.reply->setReadBufferSize(manifestByteLimit + 1);
        QNetworkReply* const reply = session.reply;
        const QString taskId = session.taskId;
        connect(reply, &QIODevice::readyRead, this, [this, taskId, reply] {
            Session* active = find(taskId);
            if (active == nullptr || active->reply != reply) {
                return;
            }
            active->responseBytes += reply->read(manifestByteLimit + 1 - active->responseBytes.size());
            if (active->responseBytes.size() > manifestByteLimit) {
                fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
            }
        });
        connect(reply, &QNetworkReply::finished, this,
            [this, taskId, reply, url, redirects, requestedLaunch] {
                Session* active = find(taskId);
                if (active == nullptr || active->reply != reply) {
                    reply->deleteLater();
                    return;
                }
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
                const auto error = reply->error();
                active->responseBytes += reply->read(manifestByteLimit + 1 - active->responseBytes.size());
                active->reply = nullptr;
                reply->deleteLater();
                if (status >= 300 && status < 400 && !redirect.isEmpty()) {
                    resolveManifest(*active, url.resolved(redirect), redirects + 1, requestedLaunch);
                    return;
                }
                if (error != QNetworkReply::NoError || status != 200
                    || active->responseBytes.size() > manifestByteLimit) {
                    fail(*active, qtTrId("genycaster.miniapp.load_failed"));
                    return;
                }
                const QJsonDocument document = QJsonDocument::fromJson(active->responseBytes);
                active->responseBytes.clear();
                if (!document.isObject()) {
                    fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                    return;
                }
                const QJsonObject manifest = document.object();
                if (!validAccountAssociationEnvelope(manifest, url.host())) {
                    fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                    return;
                }
                const QJsonObject app = manifest.value(QStringLiteral("miniapp")).isObject()
                    ? manifest.value(QStringLiteral("miniapp")).toObject()
                    : manifest.value(QStringLiteral("frame")).toObject();
                if (app.value(QStringLiteral("version")).toString() != QStringLiteral("1")
                    || app.value(QStringLiteral("name")).toString().trimmed().isEmpty()) {
                    fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                    return;
                }
                const QJsonValue required = app.value(QStringLiteral("requiredCapabilities"));
                const QJsonValue requiredChains = app.value(QStringLiteral("requiredChains"));
                if ((!required.isUndefined() && !required.isArray())
                    || (!requiredChains.isUndefined() && !requiredChains.isArray())) {
                    fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                    return;
                }

                QStringList missingChains{};
                const QJsonArray supportedChains = chains(m_wallet);
                for (const QJsonValue chain : requiredChains.toArray()) {
                    if (!chain.isString() || chain.toString().trimmed().isEmpty()) {
                        fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                        return;
                    }
                    const QString requiredChain = chain.toString().trimmed();
                    if (!supportedChains.contains(requiredChain)) {
                        missingChains.push_back(requiredChain);
                    }
                }
                QStringList missingCapabilities{};
                const QJsonArray supportedCapabilities = capabilities(m_wallet, signInAvailable());
                for (const QJsonValue capability : required.toArray()) {
                    if (!capability.isString() || capability.toString().trimmed().isEmpty()) {
                        fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                        return;
                    }
                    const QString requiredCapability = capability.toString().trimmed();
                    if (!supportedCapabilities.contains(requiredCapability)) {
                        missingCapabilities.push_back(requiredCapability);
                    }
                }
                if (!missingChains.isEmpty() || !missingCapabilities.isEmpty()) {
                    fail(*active, unsupportedRequirementsText(missingCapabilities, missingChains));
                    return;
                }

                QUrl launch = requestedLaunch;
                const QUrl manifestHome{app.value(QStringLiteral("homeUrl")).toString().trimmed(), QUrl::StrictMode};
                if (farcasterUniversalLink(active->requestedUrl) && publicHttps(manifestHome)) {
                    launch = manifestHome;
                }
                if (!publicHttps(launch)) {
                    fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                    return;
                }
                if (launch.host().compare(url.host(), Qt::CaseInsensitive) != 0
                    && (!publicHttps(manifestHome)
                        || manifestHome.host().compare(launch.host(), Qt::CaseInsensitive) != 0)) {
                    fail(*active, qtTrId("genycaster.miniapp.invalid_manifest"));
                    return;
                }

                active->manifestApp = app;
                active->launchUrl = launch;
                active->title = app.value(QStringLiteral("name")).toString().trimmed().left(80);
                active->catalogRow = catalogueRowFromManifest(app, launch);
                const QString resolvedIcon = active->catalogRow.value(QStringLiteral("iconSource")).toString();
                if (!resolvedIcon.isEmpty()) {
                    active->iconSource = resolvedIcon;
                }
                m_manifestCache.insert(launch.host().toLower(), ManifestCacheEntry{
                    .app = active->manifestApp,
                    .catalogRow = active->catalogRow,
                    .title = active->title,
                    .iconSource = active->iconSource,
                    .expiresAtMs = QDateTime::currentMSecsSinceEpoch() + manifestCacheTtlMs,
                });
                createBrowser(*active);
            });
    }

    [[nodiscard]] bool isSavedInCatalog(const Session& session) const
    {
        const QString requested = normalizedUrl(session.requestedUrl);
        const QString launch = normalizedUrl(session.launchUrl);
        const QVariantList savedApps = m_catalog.myApps();
        return std::ranges::any_of(savedApps, [&requested, &launch](const QVariant& value) {
            const QVariantMap row = value.toMap();
            const QUrl url{row.value(QStringLiteral("homeUrl")).toString(), QUrl::StrictMode};
            if (!publicHttps(url)) {
                return false;
            }
            const QString saved = normalizedUrl(url);
            return saved == requested || saved == launch;
        });
    }

    void createBrowser(Session& session)
    {
        const QString taskId = session.taskId;
        platform::miniapp::BrowserCallbacks callbacks{};
        callbacks.message = [this, taskId](const QJsonObject& message, const QString& document) {
            if (Session* active = find(taskId)) {
                receive(*active, message, document);
            }
        };
        callbacks.closed = [this, taskId] {
            QTimer::singleShot(0, this, [this, taskId] { terminate(taskId); });
        };
        callbacks.retry = [this, taskId] {
            QTimer::singleShot(0, this, [this, taskId] {
                Session* active = find(taskId);
                if (active == nullptr) {
                    return;
                }
                const QString url = active->requestedUrl.toString(QUrl::FullyEncoded);
                const QString title = active->title;
                const QString iconSource = active->iconSource;
                terminate(taskId);
                launch(url, title, iconSource);
            });
        };
        callbacks.minimize = [this, taskId] {
            QTimer::singleShot(0, this, [this, taskId] { minimize(taskId); });
        };
        callbacks.activated = [this, taskId] {
            QTimer::singleShot(0, this, [this, taskId] {
                Session* target = find(taskId);
                if (target == nullptr || target->minimized || !supportsParallelWindowVisibility()) {
                    return;
                }
                activate(*target, false);
            });
        };
        callbacks.share = [this, taskId] {
            QTimer::singleShot(0, this, [this, taskId] {
                Session* active = find(taskId);
                if (active != nullptr) {
                    const QUrl shareUrl = farcasterUniversalLink(active->requestedUrl)
                        ? active->requestedUrl : active->launchUrl;
                    minimize(taskId);
                    m_controller.requestSessionShare(
                        shareUrl.toString(QUrl::FullyEncoded), active->title, taskId);
                }
            });
        };
        callbacks.navigationStarted = [this, taskId] {
            if (m_walletTaskId == taskId) endWalletSession();
            Session* active = find(taskId);
            if (active == nullptr) {
                return;
            }
            ++active->documentGeneration;
            ++active->readyGeneration;
            active->documentToken.clear();
            active->providerUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            active->rateWindow.restart();
            active->messageCount = 0;
            active->pendingSignIn = false;
            active->ready = false;
            const quint64 readyGeneration = active->readyGeneration;
            QTimer::singleShot(readyTimeoutMs, this, [this, taskId, readyGeneration] {
                Session* candidate = find(taskId);
                if (candidate != nullptr && !candidate->ready
                    && candidate->readyGeneration == readyGeneration) {
                    fail(*candidate, qtTrId("genycaster.miniapp.ready_timeout"));
                }
            });
        };
        callbacks.failed = [this, taskId](const QString& error) {
            if (Session* active = find(taskId)) {
                fail(*active, error);
            }
        };

        const bool shouldFocus = m_focusIntentTaskId == taskId;
        session.browser = platform::miniapp::createBrowser(
            session.title,
            session.launchUrl,
            std::move(callbacks),
            presentationMode(m_controller.presentationMode()),
            m_controller.reduceMotion(),
            shouldFocus,
            platform::miniapp::BrowserRole::MiniApp);
        if (!session.browser) {
            fail(session, qtTrId("genycaster.miniapp.unavailable"));
            return;
        }

        const QJsonObject insets{{QStringLiteral("top"), 0}, {QStringLiteral("bottom"), 0},
                                 {QStringLiteral("left"), 0}, {QStringLiteral("right"), 0}};
        const QJsonObject location = session.referrerDomain.isEmpty()
            ? QJsonObject{{QStringLiteral("type"), QStringLiteral("launcher")}}
            : QJsonObject{
                {QStringLiteral("type"), QStringLiteral("open_miniapp")},
                {QStringLiteral("referrerDomain"), session.referrerDomain},
            };
        session.context = {
            {QStringLiteral("user"), QJsonObject::fromVariantMap(m_controller.user())},
            {QStringLiteral("location"), location},
            {QStringLiteral("client"), QJsonObject{
                {QStringLiteral("platformType"), QStringLiteral("web")},
                {QStringLiteral("clientFid"), static_cast<double>(m_clientFid)},
                {QStringLiteral("added"), isSavedInCatalog(session)},
                {QStringLiteral("safeAreaInsets"), insets}}},
            {QStringLiteral("features"), QJsonObject{{QStringLiteral("haptics"), false}}},
        };
        session.browser->status(qtTrId("genycaster.miniapp.loading"), false);
        session.browser->load(session.launchUrl);
        QTimer::singleShot(350, this, [this, taskId] {
            Session* active = find(taskId);
            if (active != nullptr && active->browser) {
                m_taskManager.setProcessId(taskId, active->browser->processIdentifier());
            }
        });
        if (shouldFocus) {
            focus(taskId);
        } else {
            session.foreground = false;
            session.minimized = true;
            session.browser->setVisible(false);
            updateTask(session, QStringLiteral("loading"));
        }
    }

    void activate(Session& target, const bool hidePrevious)
    {
        Session* const current = foreground();
        if (m_foregroundTaskId != target.taskId) {
            endWalletSession();
        }

        target.foreground = true;
        target.minimized = false;
        m_foregroundTaskId = target.taskId;

        if (current != nullptr && current->taskId != target.taskId) {
            current->foreground = false;
            current->minimized = hidePrevious;
            if (hidePrevious && current->browser) {
                current->browser->setVisible(false);
            }
            updateTask(*current, taskState(*current));
        }
        updateTask(target, taskState(target));
        updateHostState();
    }

    void focus(const QString& taskId)
    {
        Session* target = find(taskId);
        if (target == nullptr) {
            return;
        }
        m_focusIntentTaskId = taskId;
        if (!target->browser) {
            updateTask(*target, QStringLiteral("resolving"));
            return;
        }

        const bool parallelWindow = supportsParallelWindowVisibility();
        if (parallelWindow) {
            // Native windows are independent documents: focusing one changes
            // active authority but must never hide its already-open peers.
            activate(*target, false);
            target->browser->setVisible(true);
            return;
        }

        // Drawer modes share one parent surface. Present the target before
        // hiding the previous drawer to avoid a blank-frame handoff flash.
        target->browser->setVisible(true);
        activate(*target, true);
    }

    void minimize(const QString& taskId)
    {
        Session* target = find(taskId);
        if (target == nullptr || !target->browser) {
            return;
        }
        target->foreground = false;
        target->minimized = true;
        target->browser->setVisible(false);
        if (m_foregroundTaskId == taskId) {
            m_foregroundTaskId.clear();
            endWalletSession();
        }
        if (m_focusIntentTaskId == taskId) {
            m_focusIntentTaskId.clear();
        }
        updateTask(*target, QStringLiteral("minimized"));
        updateHostState();
    }

    void terminateForeground()
    {
        if (!m_foregroundTaskId.isEmpty()) {
            terminate(m_foregroundTaskId);
        }
    }

    void terminate(const QString& taskId)
    {
        auto iterator = std::ranges::find_if(m_sessions, [&taskId](const auto& session) {
            return session->taskId == taskId;
        });
        if (iterator == m_sessions.end()) {
            return;
        }
        Session& session = **iterator;
        stopReply(session);
        if (m_walletTaskId == taskId) {
            endWalletSession();
        }
        if (m_foregroundTaskId == taskId) {
            m_foregroundTaskId.clear();
        }
        if (m_focusIntentTaskId == taskId) {
            m_focusIntentTaskId.clear();
        }
        for (auto compose = session.pendingCompose.cbegin(); compose != session.pendingCompose.cend(); ++compose) {
            m_composeRequestToTask.remove(compose.key());
        }
        m_taskManager.removeTask(taskId);
        m_sessions.erase(iterator);
        updateHostState();
    }

    void terminateAll()
    {
        endWalletSession();
        for (auto& session : m_sessions) {
            stopReply(*session);
            m_taskManager.removeTask(session->taskId);
        }
        m_composeRequestToTask.clear();
        m_foregroundTaskId.clear();
        m_focusIntentTaskId.clear();
        m_sessions.clear();
        updateHostState();
    }

    void receive(Session& session, const QJsonObject& message, const QString& document)
    {
        if (!session.browser || document.isEmpty() || document.size() > 128) {
            return;
        }
        session.documentToken = document;
        m_taskManager.recordRpc(session.taskId);
        m_taskManager.setProcessId(session.taskId, session.browser->processIdentifier());
        if (!session.rateWindow.isValid() || session.rateWindow.elapsed() >= 1000) {
            session.rateWindow.restart();
            session.messageCount = 0;
        }
        if (++session.messageCount > 120
            || QJsonDocument(message).toJson(QJsonDocument::Compact).size() > rpcByteLimit) {
            fail(session, qtTrId("genycaster.miniapp.rpc_limit"));
            return;
        }

        const QJsonValue id = message.value(QStringLiteral("id"));
        if (!id.isString() || id.toString().size() > 128) {
            return;
        }
        const auto reject = [&session, &id, &document](const QString& reason) {
            session.browser->reply(errorResponse(id, reason), document);
        };
        const QJsonArray path = message.value(QStringLiteral("path")).toArray();
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("GET") && path.size() == 1
            && path.first().toString() == QStringLiteral("context")) {
            // The public PFP can arrive after the initial Farcaster account
            // restore. A launch-time JSON snapshot would permanently omit it
            // from this Mini App even after the profile finished hydrating.
            // Refresh only the user for the SAME FID; never expose another
            // account's identity to an already-open third-party document.
            const QJsonObject currentUser = QJsonObject::fromVariantMap(m_controller.user());
            const double currentFid = currentUser.value(QStringLiteral("fid")).toDouble();
            const double sessionFid = session.context.value(QStringLiteral("user"))
                                          .toObject().value(QStringLiteral("fid")).toDouble();
            if (currentFid <= 0.0 || currentFid != sessionFid) {
                reject(QStringLiteral("Mini App account context is no longer valid"));
                return;
            }
            session.context.insert(QStringLiteral("user"), currentUser);
            session.browser->reply(rawResponse(id, session.context), document);
            return;
        }
        if (type == QStringLiteral("RELEASE")) {
            session.browser->reply(rawResponse(id), document);
            return;
        }
        if (type != QStringLiteral("APPLY") || path.size() != 1 || !path.first().isString()) {
            reject(QStringLiteral("Unsupported host operation"));
            return;
        }
        const QString method = path.first().toString();
        const bool requiresForegroundAttention =
            method == QStringLiteral("signIn")
            || method == QStringLiteral("viewProfile")
            || method == QStringLiteral("viewCast")
            || method == QStringLiteral("openMiniApp")
            || method == QStringLiteral("composeCast");
        if (requiresForegroundAttention && !session.foreground) {
            reject(QStringLiteral("Background Mini Apps cannot request host UI actions"));
            return;
        }
        const QJsonValue arguments = message.value(QStringLiteral("argumentList"));
        if (!arguments.isArray() || arguments.toArray().size() > 1) {
            reject(QStringLiteral("Invalid host arguments"));
            return;
        }
        const QJsonArray args = arguments.toArray();
        if (!args.isEmpty() && args.first().toObject().value(QStringLiteral("type")).toString()
            != QStringLiteral("RAW")) {
            reject(QStringLiteral("Transferred functions and ports are not supported"));
            return;
        }
        const QJsonValue argument = args.isEmpty() ? QJsonValue{}
            : args.first().toObject().value(QStringLiteral("value"));

        if (method == QStringLiteral("getCapabilities")) {
            session.browser->reply(rawResponse(id, capabilities(m_wallet, signInAvailable())), document);
        } else if (method == QStringLiteral("getChains")) {
            session.browser->reply(rawResponse(id, chains(m_wallet)), document);
        } else if (method == QStringLiteral("ethProviderRequestV2")) {
            receiveEthereumRequest(session, id, argument, document);
        } else if (method == QStringLiteral("eip6963RequestProvider")) {
            announceEthereumProvider(session, id, document);
        } else if (method == QStringLiteral("signIn")) {
#if GENYCASTER_MINIAPP_SIWF
            beginSignIn(session, id, argument, document);
#else
            reject(QStringLiteral("Mini App sign in is unavailable on this platform."));
#endif
        } else if (method == QStringLiteral("ready")) {
            bool disableNativeGestures = false;
            if (!argument.isUndefined() && !argument.isNull()) {
                if (!argument.isObject()) {
                    reject(QStringLiteral("Invalid ready options"));
                    return;
                }
                const QJsonValue gestures = argument.toObject().value(QStringLiteral("disableNativeGestures"));
                if (!gestures.isUndefined() && !gestures.isBool()) {
                    reject(QStringLiteral("Invalid ready options"));
                    return;
                }
                disableNativeGestures = gestures.toBool(false);
            }
            session.ready = true;
            session.failed = false;
            session.browser->ready(disableNativeGestures);
            session.browser->reply(rawResponse(id), document);
            updateTask(session, taskState(session));
            completeOpenMiniAppHandoff(session);
        } else if (method == QStringLiteral("updateBackState")) {
            if (!argument.toObject().value(QStringLiteral("visible")).isBool()) {
                reject(QStringLiteral("Invalid back state"));
                return;
            }
            session.browser->setBackVisible(argument.toObject().value(QStringLiteral("visible")).toBool());
            session.browser->reply(rawResponse(id), document);
        } else if (method == QStringLiteral("close")) {
            session.browser->reply(rawResponse(id), document);
            const QString taskId = session.taskId;
            QTimer::singleShot(0, this, [this, taskId] { terminate(taskId); });
        } else if (method == QStringLiteral("viewProfile")) {
            const double fid = argument.toObject().value(QStringLiteral("fid")).toDouble();
            if (!std::isfinite(fid) || fid < 1 || fid > largestExactInteger || std::floor(fid) != fid) {
                reject(QStringLiteral("Invalid profile FID"));
                return;
            }
            session.browser->reply(rawResponse(id), document);
            QTimer::singleShot(0, this, [this, fid] {
                emit m_controller.profileRequested(static_cast<qulonglong>(fid));
            });
        } else if (method == QStringLiteral("viewCast")) {
            const QString hash = argument.toObject().value(QStringLiteral("hash")).toString();
            static const QRegularExpression castHash{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
            if (!castHash.match(hash).hasMatch()) {
                reject(QStringLiteral("Invalid cast hash"));
                return;
            }
            session.browser->reply(rawResponse(id), document);
            QTimer::singleShot(0, this, [this, hash] { emit m_controller.castRequested(hash); });
        } else if (method == QStringLiteral("openMiniApp")) {
            const QString url = argument.toObject().value(QStringLiteral("url")).toString().trimmed();
            const QUrl nested{url, QUrl::StrictMode};
            if (url.size() > 4096 || !publicHttps(nested)) {
                reject(QStringLiteral("Invalid Mini App URL"));
                return;
            }
            const QString sourceTaskId = session.taskId;
            const quint64 sourceGeneration = session.documentGeneration;
            const QString referrerDomain = session.launchUrl.host().toLower();
            QTimer::singleShot(0, this,
                [this, url, sourceTaskId, id, document, sourceGeneration, referrerDomain] {
                    launch(url, QString{}, QString{}, sourceTaskId, id, document,
                           sourceGeneration, referrerDomain);
                });
        } else if (method == QStringLiteral("addMiniApp") || method == QStringLiteral("addFrame")) {
            reject(QStringLiteral(
                "Genycaster does not advertise addMiniApp until Farcaster-signed server events and notification delivery are available"));
        } else if (method == QStringLiteral("composeCast")) {
            composeCast(session, id, argument, document);
        } else {
            reject(QStringLiteral("Genycaster does not support this SDK method: ") + method.left(80));
        }
    }

    void composeCast(Session& session,
                     const QJsonValue& id,
                     const QJsonValue& argument,
                     const QString& document)
    {
        if (!argument.isObject()) {
            session.browser->reply(errorResponse(id, QStringLiteral("Invalid composeCast options")), document);
            return;
        }
        if (!m_composeRequestToTask.isEmpty()) {
            session.browser->reply(errorResponse(id, QStringLiteral("The Genycaster composer is already in use")), document);
            return;
        }
        const QJsonObject options = argument.toObject();
        const QString text = options.value(QStringLiteral("text")).toString();
        const QJsonValue embedsValue = options.value(QStringLiteral("embeds"));
        const QJsonValue closeValue = options.value(QStringLiteral("close"));
        const QJsonValue parentValue = options.value(QStringLiteral("parent"));
        const QString channelKey = options.value(QStringLiteral("channelKey")).toString().trimmed();
        if (text.size() > 4096 || (!embedsValue.isUndefined() && !embedsValue.isArray())
            || (!closeValue.isUndefined() && !closeValue.isBool())
            || (!parentValue.isUndefined() && !parentValue.isObject())
            || (!options.value(QStringLiteral("channelKey")).isUndefined()
                && !options.value(QStringLiteral("channelKey")).isString())) {
            session.browser->reply(errorResponse(id, QStringLiteral("Invalid composeCast options")), document);
            return;
        }
        if (!channelKey.isEmpty()) {
            session.browser->reply(errorResponse(id, QStringLiteral("Channel-targeted composeCast is not supported yet")), document);
            return;
        }
        QString parentHash{};
        if (parentValue.isObject()) {
            const QJsonObject parent = parentValue.toObject();
            parentHash = parent.value(QStringLiteral("hash")).toString().trimmed();
            static const QRegularExpression castHash{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
            if (parent.value(QStringLiteral("type")).toString() != QStringLiteral("cast")
                || !castHash.match(parentHash).hasMatch()) {
                session.browser->reply(errorResponse(id, QStringLiteral("Invalid composeCast parent")), document);
                return;
            }
        }
        QJsonArray embeds = embedsValue.toArray();
        if (embeds.size() > 2) {
            session.browser->reply(errorResponse(id, QStringLiteral("composeCast supports at most two embeds")), document);
            return;
        }
        for (const QJsonValue& embed : embeds) {
            if (!embed.isString() || !publicHttps(QUrl{embed.toString(), QUrl::StrictMode})) {
                session.browser->reply(errorResponse(id, QStringLiteral("Invalid composeCast embed")), document);
                return;
            }
        }
        const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session.pendingCompose.insert(requestId, PendingCompose{
            .outerId = id,
            .document = document,
            .parentHash = parentHash,
            .closeAfter = closeValue.toBool(false),
        });
        m_composeRequestToTask.insert(requestId, session.taskId);
        minimize(session.taskId);
        m_controller.requestComposeCast(requestId, QVariantMap{
            {QStringLiteral("text"), text},
            {QStringLiteral("embeds"), embeds.toVariantList()},
            {QStringLiteral("close"), closeValue.toBool(false)},
            {QStringLiteral("parentHash"), parentHash},
            {QStringLiteral("taskId"), session.taskId},
            {QStringLiteral("appName"), session.title},
        });
    }

    void completeComposeCast(const QString& requestId, const QVariantMap& result)
    {
        const QString taskId = m_composeRequestToTask.take(requestId);
        Session* session = find(taskId);
        if (session == nullptr || !session->browser) {
            return;
        }
        const auto iterator = session->pendingCompose.find(requestId);
        if (iterator == session->pendingCompose.end()) {
            return;
        }
        const PendingCompose pending = iterator.value();
        session->pendingCompose.erase(iterator);

        const QString castId = result.value(QStringLiteral("castId")).toString();
        QJsonValue value{};
        if (castId.isEmpty()) {
            value = QJsonObject{{QStringLiteral("cast"), QJsonValue::Null}};
        } else {
            QJsonObject cast{
                {QStringLiteral("hash"), castId},
                {QStringLiteral("text"), result.value(QStringLiteral("text")).toString()},
                {QStringLiteral("embeds"), QJsonArray::fromVariantList(result.value(QStringLiteral("embeds")).toList())},
            };
            if (!pending.parentHash.isEmpty()) {
                cast.insert(QStringLiteral("parent"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("cast")},
                    {QStringLiteral("hash"), pending.parentHash},
                });
            }
            value = QJsonObject{{QStringLiteral("cast"), cast}};
        }
        if (pending.closeAfter) {
            session->browser->reply(rawResponse(pending.outerId), pending.document);
        } else {
            session->browser->reply(rawResponse(pending.outerId, value), pending.document);
        }
        if (pending.closeAfter) {
            QTimer::singleShot(0, this, [this, taskId] { terminate(taskId); });
        } else {
            QTimer::singleShot(0, this, [this, taskId] { focus(taskId); });
        }
    }

    void cancelComposeCast(const QString& requestId)
    {
        const QString taskId = m_composeRequestToTask.take(requestId);
        Session* session = find(taskId);
        if (session == nullptr || !session->browser) {
            return;
        }
        const auto iterator = session->pendingCompose.find(requestId);
        if (iterator == session->pendingCompose.end()) {
            return;
        }
        const PendingCompose pending = iterator.value();
        session->pendingCompose.erase(iterator);
        if (pending.closeAfter) {
            session->browser->reply(rawResponse(pending.outerId), pending.document);
            QTimer::singleShot(0, this, [this, taskId] { terminate(taskId); });
        } else {
            session->browser->reply(
                rawResponse(pending.outerId, QJsonObject{{QStringLiteral("cast"), QJsonValue::Null}}),
                pending.document);
            QTimer::singleShot(0, this, [this, taskId] { focus(taskId); });
        }
    }

    [[nodiscard]] bool ensureWalletSession(Session& session)
    {
        if (m_wallet == nullptr || !m_wallet->available() || !session.foreground
            || m_foregroundTaskId != session.taskId) {
            return false;
        }
        if (m_walletTaskId == session.taskId) {
            return true;
        }
        endWalletSession();
        QUrl origin = session.launchUrl;
        origin.setPath(QString{});
        origin.setQuery(QString{});
        origin.setFragment(QString{});
        const auto started = m_wallet->beginSession(
            session.title.toStdString(), origin.toString(QUrl::FullyEncoded).toStdString());
        if (!started) {
            return false;
        }
        m_walletTaskId = session.taskId;
        m_controller.setWalletBrowserContext(
            session.title, origin.toString(QUrl::FullyEncoded));
        return true;
    }

    [[nodiscard]] bool signInAvailable() const
    {
#if GENYCASTER_MINIAPP_SIWF
        return m_wallet != nullptr && m_wallet->available()
            && m_controller.user().value(QStringLiteral("fid")).toULongLong() > 0U
            && m_optimismRpcUrl.starts_with("https://");
#else
        return false;
#endif
    }

#if GENYCASTER_MINIAPP_SIWF
    // The same already user-approved EIP-1193 wallet transport used by Mini
    // Apps performs each sensitive operation. Never read native signer seeds.
    void requestSignInWallet(
        const QString& taskId,
        capability::wallet::EthereumRpcRequest request,
        std::function<void(Impl&, capability::wallet::EthereumRpcResponse)> completion)
    {
        const std::weak_ptr<CallbackGate> weakGate = m_callbackGate;
        m_wallet->request(std::move(request),
            [weakGate, taskId, completion = std::move(completion)](
                capability::wallet::EthereumRpcResponse response) mutable {
                auto gate = weakGate.lock();
                if (!gate) return;
                std::scoped_lock lock{gate->mutex};
                if (!gate->runtime) return;
                Impl* const runtime = gate->runtime;
                QMetaObject::invokeMethod(runtime,
                    [runtime, taskId, completion = std::move(completion),
                     response = std::move(response)]() mutable {
                        if (runtime->m_walletTaskId == taskId)
                            runtime->m_controller.walletBrowserRequestFinished(!response.error);
                        completion(*runtime, std::move(response));
                    }, Qt::QueuedConnection);
            });
    }

    void finishSignIn(const QString& taskId, const quint64 generation,
                      const quint64 sequence, const QString& document,
                      const QJsonValue& id,
                      const QString& message = {}, const QString& signature = {},
                      const bool authAddress = false, const bool userRejected = false)
    {
        Session* const session = find(taskId);
        if (!session || !session->pendingSignIn
            || session->signInSequence != sequence
            || session->documentGeneration != generation
            || session->documentToken != document || !session->browser) return;
        session->pendingSignIn = false;
        if (!session->foreground || m_foregroundTaskId != taskId
            || m_controller.user().value(QStringLiteral("fid")).toULongLong()
                != session->context.value(QStringLiteral("user")).toObject()
                   .value(QStringLiteral("fid")).toDouble()) {
            session->browser->reply(errorResponse(
                id, QStringLiteral("Mini App authentication was cancelled.")), document);
            return;
        }
        if (message.isEmpty() || signature.isEmpty()) {
            if (userRejected) {
                // Exact official Mini App wire result for wallet rejection.
                session->browser->reply(rawResponse(id, QJsonObject{
                    {QStringLiteral("error"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("rejected_by_user")}}}
                }), document);
            } else {
                session->browser->reply(errorResponse(id, QStringLiteral(
                    "Sign in failed: the wallet proof could not be verified for this FID. "
                    "Use a current Farcaster custody/auth address wallet and retry.")), document);
            }
            return;
        }
        session->browser->reply(rawResponse(id, QJsonObject{
            {QStringLiteral("result"), QJsonObject{
                {QStringLiteral("message"), message},
                {QStringLiteral("signature"), signature},
                {QStringLiteral("authMethod"), authAddress
                    ? QStringLiteral("authAddress") : QStringLiteral("custody")}
            }}
        }), document);
    }

    void beginSignIn(Session& session, const QJsonValue& id,
                     const QJsonValue& value, const QString& document)
    {
        const QJsonObject currentUser = QJsonObject::fromVariantMap(m_controller.user());
        const QJsonObject sessionUser = session.context.value(QStringLiteral("user")).toObject();
        const QUrl appUrl = session.launchUrl;
        const QString taskId = session.taskId;
        const quint64 generation = session.documentGeneration;
        const qulonglong fid = static_cast<qulonglong>(currentUser.value(
            QStringLiteral("fid")).toDouble());
        if (!signInAvailable() || !session.foreground
            || m_foregroundTaskId != taskId || !publicHttps(appUrl)
            || fid == 0U || fid != static_cast<qulonglong>(
                sessionUser.value(QStringLiteral("fid")).toDouble())
            || session.documentToken != document || session.pendingSignIn
            || !value.isObject()) {
            session.browser->reply(errorResponse(id, QStringLiteral(
                "Sign in requires the foreground Mini App and a verified account.")), document);
            return;
        }
        const QJsonObject options = value.toObject();
        const QString nonce = options.value(QStringLiteral("nonce")).toString();
        static const QRegularExpression validNonce{QStringLiteral("^[A-Za-z0-9]{8,128}$")};
        if (!validNonce.match(nonce).hasMatch()
            || (options.contains(QStringLiteral("acceptAuthAddress"))
                && !options.value(QStringLiteral("acceptAuthAddress")).isBool())) {
            session.browser->reply(errorResponse(id, QStringLiteral(
                "Sign in requires a valid relying-party nonce and options.")), document);
            return;
        }
        const bool acceptAuthAddress = options.value(QStringLiteral("acceptAuthAddress"))
                                           .toBool(true);
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const auto readTime = [&options](const QString& field) -> QDateTime {
            if (!options.contains(field)) return {};
            const QString raw = options.value(field).toString();
            if (raw.size() < 20 || raw.size() > 40) return {};
            const QDateTime result = QDateTime::fromString(raw, Qt::ISODateWithMs);
            return result.isValid() ? result.toUTC()
                                   : QDateTime::fromString(raw, Qt::ISODate).toUTC();
        };
        QDateTime expires = now.addSecs(300);
        QDateTime notBefore{};
        if (options.contains(QStringLiteral("expirationTime"))) {
            expires = readTime(QStringLiteral("expirationTime"));
            if (!expires.isValid() || expires <= now
                || expires > now.addSecs(600)) {
                session.browser->reply(errorResponse(id, QStringLiteral(
                    "Sign in expiration must be within ten minutes.")), document);
                return;
            }
        }
        if (options.contains(QStringLiteral("notBefore"))) {
            notBefore = readTime(QStringLiteral("notBefore"));
            if (!notBefore.isValid() || notBefore > now.addSecs(120)
                || notBefore >= expires) {
                session.browser->reply(errorResponse(id, QStringLiteral(
                    "Sign in validity time is outside the permitted window.")), document);
                return;
            }
        }
        if (!ensureWalletSession(session)) {
            session.browser->reply(errorResponse(id, QStringLiteral(
                "No approved Mini App wallet session is available.")), document);
            return;
        }
        session.pendingSignIn = true;
        const quint64 sequence = ++session.signInSequence;
        QTimer::singleShot(180'000, this,
            [this, taskId, generation, sequence, document, id] {
                finishSignIn(taskId, generation, sequence, document, id);
            });
        requestSignInWallet(taskId, capability::wallet::EthereumRpcRequest{
            .id = ++m_nextSignInRpcId, .method = "eth_requestAccounts",
            .paramsJson = "[]",
        }, [taskId, generation, sequence, document, id, nonce, fid, appUrl,
            expires, notBefore, now, acceptAuthAddress](
                Impl& runtime, capability::wallet::EthereumRpcResponse response) {
            Session* const active = runtime.find(taskId);
            if (!active || !active->pendingSignIn || !active->foreground
                || active->signInSequence != sequence
                || active->documentGeneration != generation
                || active->documentToken != document || response.error) {
                runtime.finishSignIn(taskId, generation, sequence, document, id,
                    {}, {}, false, response.error && response.error->code == 4001);
                return;
            }
            const QJsonValue accounts = parseJsonValue(response.resultJson);
            const QString account = accounts.isArray()
                && !accounts.toArray().isEmpty()
                ? accounts.toArray().first().toString() : QString{};
            static const QRegularExpression validAddress{
                QStringLiteral("^0x[0-9a-fA-F]{40}$")};
            if (!validAddress.match(account).hasMatch()) {
                runtime.finishSignIn(taskId, generation, sequence, document, id);
                return;
            }
            const QString domain = appUrl.host().toLower();
            const QString uri = normalizedUrl(appUrl);
            const QString stamp = now.toString(Qt::ISODate);
            QString siwe = domain + QStringLiteral(
                " wants you to sign in with your Ethereum account:\n")
                + account + QStringLiteral(
                "\n\nFarcaster Auth\n\nURI: ") + uri + QStringLiteral(
                "\nVersion: 1\nChain ID: 10\nNonce: ") + nonce
                + QStringLiteral("\nIssued At: ") + stamp
                + QStringLiteral("\nExpiration Time: ")
                + expires.toString(Qt::ISODate);
            if (notBefore.isValid())
                siwe += QStringLiteral("\nNot Before: ") + notBefore.toString(Qt::ISODate);
            siwe += QStringLiteral("\nResources:\n- farcaster://fid/")
                    + QString::number(fid);
            const QString encoded = QStringLiteral("0x")
                + QString::fromLatin1(siwe.toUtf8().toHex());
            const QJsonArray params{encoded, account};
            runtime.requestSignInWallet(taskId, capability::wallet::EthereumRpcRequest{
                .id = ++runtime.m_nextSignInRpcId,
                .method = "personal_sign",
                .paramsJson = QJsonDocument(params)
                    .toJson(QJsonDocument::Compact).toStdString(),
            }, [taskId, generation, sequence, document, id, nonce, fid, domain, uri,
                siwe, acceptAuthAddress](Impl& owner,
                     capability::wallet::EthereumRpcResponse signedResponse) {
                Session* const current = owner.find(taskId);
                if (!current || !current->pendingSignIn
                    || current->signInSequence != sequence
                    || current->documentGeneration != generation
                    || current->documentToken != document || signedResponse.error) {
                    owner.finishSignIn(taskId, generation, sequence, document, id,
                        {}, {}, false, signedResponse.error
                            && signedResponse.error->code == 4001);
                    return;
                }
                const QString signature = parseJsonValue(signedResponse.resultJson).toString();
                if (!signature.startsWith(QStringLiteral("0x"))
                    || signature.size() < 132 || signature.size() > 2048) {
                    owner.finishSignIn(taskId, generation, sequence, document, id);
                    return;
                }
                // Verification is blocking RPC/crypto work: perform it away
                // from the GUI. Document generation + FID are rechecked
                // before returning the signed credential to third-party JS.
                const std::weak_ptr<CallbackGate> weakGate = owner.m_callbackGate;
                const bool direct = owner.m_useDirectProxy;
                const std::string rpc = owner.m_optimismRpcUrl;
                QThreadPool::globalInstance()->start(
                    [weakGate, direct, rpc, taskId, generation, sequence, document, id,
                     nonce, fid, domain, uri, siwe, signature, acceptAuthAddress] {
                    auto transport = farcaster::transport::qtnetwork::makeTransport(
                        direct ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
                               : farcaster::transport::qtnetwork::ProxyPolicy::System);
                    bool verified = false;
                    bool authAddress = false;
                    if (transport) {
                        farcaster::provider::compatibility::auth::Config config{};
                        config.optimismRpcUrl = rpc;
                        const auto proof = farcaster::provider::compatibility::auth::verifyMiniAppSignIn(
                            **transport, siwe.toStdString(), signature.toStdString(),
                            domain.toStdString(), uri.toStdString(), nonce.toStdString(),
                            farcaster::domain::identity::Fid{fid}, acceptAuthAddress,
                            std::move(config));
                        verified = proof.has_value();
                        authAddress = verified && proof->authAddress;
                    }
                    const auto gate = weakGate.lock();
                    if (!gate) return;
                    std::scoped_lock lock{gate->mutex};
                    if (!gate->runtime) return;
                    Impl* const runtime = gate->runtime;
                    QMetaObject::invokeMethod(runtime,
                        [runtime, taskId, generation, sequence, document, id, siwe, signature,
                         verified, authAddress] {
                            if (verified)
                                runtime->finishSignIn(taskId, generation, sequence, document,
                                    id, siwe, signature, authAddress);
                            else
                                runtime->finishSignIn(taskId, generation, sequence, document, id);
                        }, Qt::QueuedConnection);
                });
            });
        });
    }
#endif

    void announceEthereumProvider(Session& session, const QJsonValue& id, const QString& document)
    {
        if (!session.foreground) {
            session.browser->reply(errorResponse(id, QStringLiteral("Wallet provider is unavailable to background Mini Apps")), document);
            return;
        }
        if (m_wallet == nullptr || !m_wallet->available()) {
            session.browser->reply(errorResponse(id, QStringLiteral("Ethereum wallet provider is unavailable")), document);
            return;
        }
        if (session.providerUuid.isEmpty()) {
            session.providerUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        const QString icon = QStringLiteral(
            "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E"
            "%3Crect width='64' height='64' rx='18' fill='%23111111'/%3E"
            "%3Cpath d='M17 18h30v8H25v12h22v8H17z' fill='%23fff'/%3E%3C/svg%3E");
        session.browser->miniAppEvent(QJsonObject{
            {QStringLiteral("event"), QStringLiteral("eip6963:announceProvider")},
            {QStringLiteral("info"), QJsonObject{
                {QStringLiteral("uuid"), session.providerUuid},
                {QStringLiteral("name"), QStringLiteral("GenyCaster (External Wallet)")},
                {QStringLiteral("icon"), icon},
                {QStringLiteral("rdns"), QStringLiteral("com.genyleap.genycaster")},
            }},
        }, document);
        session.browser->reply(rawResponse(id), document);
    }

    void receiveEthereumRequest(Session& session,
                                const QJsonValue& outerId,
                                const QJsonValue& argument,
                                const QString& document)
    {
        const auto rpcError = [&session, &outerId, &document](const QJsonValue& rpcId,
                                                              const int code,
                                                              const QString& message) {
            session.browser->reply(rawResponse(outerId, QJsonObject{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), rpcId},
                {QStringLiteral("error"), QJsonObject{
                    {QStringLiteral("code"), code},
                    {QStringLiteral("message"), message}}}}), document);
        };
        if (!argument.isObject()) {
            session.browser->reply(errorResponse(outerId, QStringLiteral("Invalid Ethereum provider request")), document);
            return;
        }
        const QJsonObject rpc = argument.toObject();
        const QJsonValue rpcIdValue = rpc.value(QStringLiteral("id"));
        const double rpcIdNumber = rpcIdValue.toDouble(-1.0);
        const QString method = rpc.value(QStringLiteral("method")).toString();
        QJsonValue paramsValue = rpc.value(QStringLiteral("params"));
        if (paramsValue.isUndefined() || paramsValue.isNull()) {
            paramsValue = QJsonArray{};
        }
        if (!std::isfinite(rpcIdNumber) || rpcIdNumber < 0.0 || std::floor(rpcIdNumber) != rpcIdNumber
            || rpcIdNumber > largestExactInteger || method.isEmpty() || !paramsValue.isArray()) {
            session.browser->reply(errorResponse(outerId, QStringLiteral("Invalid Ethereum provider request")), document);
            return;
        }
        if (!session.foreground || m_foregroundTaskId != session.taskId) {
            rpcError(rpcIdValue, 4100, QStringLiteral("Background Mini Apps are not authorized to access the wallet."));
            return;
        }
        if (m_wallet == nullptr || !m_wallet->available()) {
            rpcError(rpcIdValue, 4200, QStringLiteral("Ethereum wallet provider is unavailable."));
            return;
        }
        if (!ensureWalletSession(session)) {
            rpcError(rpcIdValue, 4900, QStringLiteral("Could not start the Genycaster wallet session."));
            return;
        }

        const auto rpcId = static_cast<std::uint64_t>(rpcIdNumber);
        const std::string paramsJson = QJsonDocument(paramsValue.toArray())
            .toJson(QJsonDocument::Compact).toStdString();
        const QString taskId = session.taskId;
        const quint64 documentGeneration = session.documentGeneration;
        const std::weak_ptr<CallbackGate> weakGate = m_callbackGate;
        m_wallet->request(capability::wallet::EthereumRpcRequest{
                .id = rpcId,
                .method = method.toStdString(),
                .paramsJson = paramsJson,
            },
            [weakGate, taskId, documentGeneration, outerId, document](
                capability::wallet::EthereumRpcResponse response) mutable {
                const auto gate = weakGate.lock();
                if (!gate) {
                    return;
                }
                std::scoped_lock lock{gate->mutex};
                if (gate->runtime == nullptr) {
                    return;
                }
                Impl* const runtime = gate->runtime;
                QMetaObject::invokeMethod(runtime,
                    [runtime, taskId, documentGeneration, outerId, document,
                     response = std::move(response)]() mutable {
                        Session* active = runtime->find(taskId);
                        if (active == nullptr || !active->browser
                            || active->documentGeneration != documentGeneration) {
                            return;
                        }
                        if (runtime->m_walletTaskId == taskId)
                            runtime->m_controller.walletBrowserRequestFinished(!response.error);
                        QJsonObject rpcResponse{
                            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                            {QStringLiteral("id"), static_cast<double>(response.id)},
                        };
                        if (response.error) {
                            QJsonObject error{
                                {QStringLiteral("code"), response.error->code},
                                {QStringLiteral("message"), QString::fromStdString(response.error->message)},
                            };
                            if (!response.error->details.empty()) {
                                error.insert(QStringLiteral("details"), QString::fromStdString(response.error->details));
                            }
                            rpcResponse.insert(QStringLiteral("error"), error);
                        } else {
                            rpcResponse.insert(QStringLiteral("result"), parseJsonValue(response.resultJson));
                        }
                        active->browser->reply(rawResponse(outerId, rpcResponse), document);
                    }, Qt::QueuedConnection);
            });
    }

    void forwardEthereumEvent(const capability::wallet::EthereumWalletEvent& event)
    {
        Session* session = find(m_walletTaskId);
        if (session == nullptr || !session->browser || !session->foreground
            || session->documentToken.isEmpty()) {
            return;
        }
        const QJsonValue parsed = parseJsonValue(event.paramsJson);
        if (!parsed.isArray()) {
            return;
        }
        session->browser->ethereumProviderEvent(
            ethereumEventName(event.kind), parsed.toArray(), session->documentToken);
    }

    MiniAppHostController& m_controller;
    MiniAppCatalogController& m_catalog;
    MiniAppTaskManagerController& m_taskManager;
    const qulonglong m_clientFid;
    capability::wallet::IEthereumWalletBridge* m_wallet{};
    const std::string m_optimismRpcUrl{};
    const bool m_useDirectProxy{};
    std::uint64_t m_nextSignInRpcId{8'000'000U};
    std::shared_ptr<CallbackGate> m_callbackGate{};
    QNetworkAccessManager m_network{};
    std::vector<std::unique_ptr<Session>> m_sessions{};
    QString m_foregroundTaskId{};
    QString m_focusIntentTaskId{};
    QString m_walletTaskId{};
    QString m_walletChooserHiddenTaskId{};
    QHash<QString, QString> m_composeRequestToTask{};
    QHash<QString, ManifestCacheEntry> m_manifestCache{};
    std::size_t m_allowedSessions{1U};
};

MiniAppRuntime::MiniAppRuntime(MiniAppHostController& controller,
                               MiniAppCatalogController& catalog,
                               MiniAppTaskManagerController& taskManager,
                               const bool useDirectProxy,
                               const qulonglong clientFid,
                               capability::wallet::IEthereumWalletBridge* walletBridge,
                               std::string optimismRpcUrl)
    : m_impl(std::make_unique<Impl>(
        controller, catalog, taskManager, useDirectProxy, clientFid, walletBridge,
        std::move(optimismRpcUrl)))
{}

MiniAppRuntime::~MiniAppRuntime() = default;

void MiniAppRuntime::setSessionLimit(const int limit)
{
    m_impl->setSessionLimit(limit);
}

} // namespace farcaster::runtime::miniapp
