#include "browser.hpp"
#include "browser_macos_native.h"

#include <QByteArray>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>
#include <QUrlQuery>
#include <QtTranslation>
#include <utility>

namespace farcaster::platform::miniapp {
namespace {
static_assert(static_cast<int>(PresentationMode::Window) == GENY_MAC_WINDOW);
static_assert(static_cast<int>(PresentationMode::Bottom) == GENY_MAC_BOTTOM);
static_assert(static_cast<int>(PresentationMode::Left) == GENY_MAC_LEFT);
static_assert(static_cast<int>(PresentationMode::Right) == GENY_MAC_RIGHT);
static_assert(static_cast<int>(BrowserRole::MiniApp) == GENY_MAC_MINIAPP);
static_assert(static_cast<int>(BrowserRole::Media) == GENY_MAC_MEDIA);

// Preserve the native toolbar and WebKit error IDs for Qt lupdate, while
// the Objective-C implementation obtains translations through the C callback.
[[maybe_unused]] constexpr const char* nativeTranslationIds[] = {
    QT_TRID_NOOP("genycaster.miniapp.back"),
    QT_TRID_NOOP("genycaster.miniapp.retry"),
    QT_TRID_NOOP("genycaster.miniapp.minimize"),
    QT_TRID_NOOP("genycaster.miniapp.share"),
    QT_TRID_NOOP("genycaster.miniapp.close"),
    QT_TRID_NOOP("genycaster.miniapp.loading"),
    QT_TRID_NOOP("genycaster.miniapp.rpc_limit"),
    QT_TRID_NOOP("genycaster.miniapp.navigation_blocked"),
    QT_TRID_NOOP("genycaster.miniapp.load_failed"),
    QT_TRID_NOOP("genycaster.miniapp.renderer_failed"),
    QT_TRID_NOOP("genycaster.player.fullscreen"),
    QT_TRID_NOOP("genycaster.player.exit_fullscreen")
};

bool allowedWebUrl(const QUrl& parsed) {
    const QString host = parsed.host().toLower();
    QHostAddress address;
    return parsed.scheme() == QStringLiteral("https") && parsed.userInfo().isEmpty()
        && parsed.port(443) == 443 && host.contains(QLatin1Char('.'))
        && !host.endsWith(QLatin1Char('.')) && !host.endsWith(QStringLiteral(".local"))
        && !host.endsWith(QStringLiteral(".localhost"))
        && !host.endsWith(QStringLiteral(".internal"))
        && !host.endsWith(QStringLiteral(".test")) && !address.setAddress(host);
}

QUrl youtubeWatchUrlForEmbed(const QUrl& parsed) {
    const QString host = parsed.host().toLower();
    if (host != QStringLiteral("youtube.com") && host != QStringLiteral("www.youtube.com")
        && host != QStringLiteral("youtube-nocookie.com")
        && host != QStringLiteral("www.youtube-nocookie.com"))
        return {};
    const QStringList segments = parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.size() != 2 || segments.front() != QStringLiteral("embed"))
        return {};
    const QString videoId = segments.back().trimmed();
    static const QRegularExpression safeVideoId{QStringLiteral(R"(^[A-Za-z0-9_-]{6,32}$)")};
    if (!safeVideoId.match(videoId).hasMatch())
        return {};
    QUrl watch{QStringLiteral("https://www.youtube.com/watch")};
    QUrlQuery query{};
    query.addQueryItem(QStringLiteral("v"), videoId);
    watch.setQuery(query);
    return watch;
}

class MacBrowser final : public Browser {
public:
    MacBrowser(const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
               PresentationMode mode, bool reduceMotion, bool initiallyVisible, BrowserRole role)
        : m_callbacks(std::move(callbacks)), m_role(role), m_origin(origin) {
        GenyMacBrowserCallbacks native{};
        native.context = this;
        native.event = &MacBrowser::nativeEvent;
        native.message = &MacBrowser::nativeMessage;
        native.failed = &MacBrowser::nativeFailed;
        native.allowed_url = &MacBrowser::nativeAllowedUrl;
        native.localize = &MacBrowser::nativeLocalize;
        native.youtube_watch_url = &MacBrowser::nativeYoutubeWatchUrl;
        const QByteArray nativeTitle = title.toUtf8();
        const QByteArray nativeOrigin = origin.toString(QUrl::FullyEncoded).toUtf8();
        m_controller = geny_mac_browser_create(
            nativeTitle.constData(), nativeOrigin.constData(),
            static_cast<GenyMacBrowserMode>(mode), reduceMotion, initiallyVisible,
            static_cast<GenyMacBrowserRole>(role), native);
    }
    ~MacBrowser() override {
        geny_mac_browser_destroy(m_controller);
        m_controller = nullptr;
    }
    [[nodiscard]] bool valid() const noexcept {
        return geny_mac_browser_valid(m_controller) != 0;
    }
    void load(const QUrl& url, const QUrl& referrer) override {
        const QByteArray target = url.toString(QUrl::FullyEncoded).toUtf8();
        QByteArray referer;
        if (referrer.isValid()
            && (referrer.scheme() == QStringLiteral("https")
                || referrer.scheme() == QStringLiteral("http"))
            && !referrer.host().isEmpty()) {
            referer = referrer.toString(QUrl::FullyEncoded).toUtf8();
        }
        geny_mac_browser_load(m_controller, target.constData(), referer.constData());
    }
    void status(const QString& message, bool failed) override {
        const QByteArray text = message.toUtf8();
        geny_mac_browser_status(m_controller, text.constData(), failed);
    }
    void ready(bool disableNativeGestures) override {
        // Desktop host has no swipe-to-dismiss gesture. Do not alter WebKit history.
        (void)disableNativeGestures;
        geny_mac_browser_ready(m_controller);
    }
    void setBackVisible(bool visible) override {
        geny_mac_browser_set_back(m_controller, visible);
    }
    void reply(const QJsonObject& value, const QString& document) override {
        const QJsonArray arguments{document, value};
        const QString json = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        evaluate(QStringLiteral("window.__genyMiniAppReply?.(...") + json + QStringLiteral(")"));
    }
    void ethereumProviderEvent(const QString& event, const QJsonArray& params,
                               const QString& document) override {
        const QJsonObject wire{{QStringLiteral("event"), event}, {QStringLiteral("params"), params}};
        const QJsonArray arguments{document, wire};
        const QString json = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        evaluate(QStringLiteral("window.__genyMiniAppEthProviderEvent?.(...") + json + QStringLiteral(")"));
    }
    void miniAppEvent(const QJsonObject& event, const QString& document) override {
        const QJsonArray arguments{document, event};
        const QString json = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        evaluate(QStringLiteral("window.__genyMiniAppEvent?.(...") + json + QStringLiteral(")"));
    }
    void setVisible(bool visible) override {
        geny_mac_browser_set_visible(m_controller, visible);
    }
    qlonglong processIdentifier() const noexcept override {
        // Public WKWebView has no stable WebContent PID API.
        return 0;
    }
private:
    void evaluate(const QString& script) {
        const QByteArray utf8 = script.toUtf8();
        geny_mac_browser_evaluate(m_controller, utf8.constData());
    }

    static void nativeEvent(void* context, GenyMacBrowserEvent event) {
        auto& callbacks = static_cast<MacBrowser*>(context)->m_callbacks;
        switch (event) {
        case GENY_MAC_CLOSED: if (callbacks.closed) callbacks.closed(); break;
        case GENY_MAC_RETRY: if (callbacks.retry) callbacks.retry(); break;
        case GENY_MAC_MINIMIZE: if (callbacks.minimize) callbacks.minimize(); break;
        case GENY_MAC_ACTIVATED: if (callbacks.activated) callbacks.activated(); break;
        case GENY_MAC_SHARE: if (callbacks.share) callbacks.share(); break;
        case GENY_MAC_NAVIGATION_STARTED:
            if (callbacks.navigationStarted) callbacks.navigationStarted();
            break;
        }
    }
    static void nativeMessage(void* context, const char* json, const char* document) {
        auto& callbacks = static_cast<MacBrowser*>(context)->m_callbacks;
        if (!json || !document || !callbacks.message) return;
        const QJsonDocument payload = QJsonDocument::fromJson(QByteArray(json));
        if (payload.isObject())
            callbacks.message(payload.object(), QString::fromUtf8(document));
    }
    static void nativeFailed(void* context, const char* translationId) {
        auto& callbacks = static_cast<MacBrowser*>(context)->m_callbacks;
        if (callbacks.failed && translationId)
            callbacks.failed(qtTrId(translationId));
    }
    static int nativeAllowedUrl(void* context, const char* url) {
        if (!url) return 0;
        const auto* self = static_cast<MacBrowser*>(context);
        const QUrl parsed = QUrl::fromEncoded(QByteArray(url), QUrl::StrictMode);
#if defined(Q_OS_IOS)
        if (self->m_role == BrowserRole::WalletApproval) {
            static const QRegularExpression path{QStringLiteral("^/session/[0-9a-f]{64}$")};
            return parsed.isValid() && parsed == self->m_origin
                && parsed.scheme() == QStringLiteral("http")
                && parsed.host() == QStringLiteral("127.0.0.1") && parsed.port() > 0
                && parsed.userInfo().isEmpty() && !parsed.hasQuery() && !parsed.hasFragment()
                && path.match(parsed.path()).hasMatch();
        }
#else
        (void)self;
#endif
        return allowedWebUrl(parsed) ? 1 : 0;
    }
    static const char* nativeLocalize(void*, const char* translationId) {
        static thread_local QByteArray translation;
        translation = qtTrId(translationId ? translationId : "").toUtf8();
        return translation.constData();
    }
    static const char* nativeYoutubeWatchUrl(void*, const char* url) {
        static thread_local QByteArray watchText;
        const QUrl watch = youtubeWatchUrlForEmbed(url ? QUrl::fromEncoded(QByteArray(url)) : QUrl{});
        watchText = watch.isValid() && !watch.isEmpty()
            ? watch.toString(QUrl::FullyEncoded).toUtf8() : QByteArray{};
        return watchText.constData();
    }

    BrowserCallbacks m_callbacks;
    [[maybe_unused]] BrowserRole m_role;
    [[maybe_unused]] QUrl m_origin;
    GenyMacBrowser* m_controller = nullptr;
};
} // namespace

std::unique_ptr<Browser> createBrowser(const QString& title, const QUrl& origin,
    BrowserCallbacks callbacks, PresentationMode mode, bool reduceMotion,
    bool initiallyVisible, BrowserRole role) {
    auto browser = std::make_unique<MacBrowser>(
        title, origin, std::move(callbacks), mode, reduceMotion, initiallyVisible, role);
    if (!browser->valid()) return {};
    return browser;
}
} // namespace farcaster::platform::miniapp
