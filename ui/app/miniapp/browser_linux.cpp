/*!
 * @file ui/app/miniapp/browser_linux.cpp
 * @brief Ephemeral, origin-bound Linux Mini App browser using Qt WebEngine.
 *
 * Compiled only with the separate, ABI-compatible Qt WebEngine extension.
 * This is a real native Mini App SDK transport; the web document never receives
 * a raw wallet handle, secure-vault key, or permission without the runtime.
 */
#include "browser.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QScreen>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QWebChannel>
#include <QWebEngineHttpRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace {
[[nodiscard]] bool publicHttps(const QUrl& url)
{
    const QString host = url.host().toLower();
    QHostAddress ip{};
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.userInfo().isEmpty() && url.port(443) == 443
        && host.contains(QLatin1Char('.')) && !host.endsWith(QLatin1Char('.'))
        && !host.endsWith(QStringLiteral(".localhost"))
        && !host.endsWith(QStringLiteral(".local"))
        && !host.endsWith(QStringLiteral(".internal"))
        && !host.endsWith(QStringLiteral(".test"))
        && !host.endsWith(QStringLiteral(".invalid"))
        && !ip.setAddress(host);
}

[[nodiscard]] QString documentBridgeScript()
{
    // qwebchannel.js ships as a Qt WebChannel resource. Keep the transport
    // initialized at document creation: the Farcaster SDK inspects
    // ReactNativeWebView during JavaScript import, before DOM ready.
    QFile library{QStringLiteral(":/qtwebchannel/qwebchannel.js")};
    if (!library.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(library.readAll()) + QStringLiteral(R"JS(
;(() => {
    const token = crypto.randomUUID();
    let channel = null, calls = 0, windowStart = performance.now();
    const pending = [];
    const send = (payload) => {
        if (channel) channel.postMessage(payload, token);
        else {
            if (pending.length >= 64) throw new Error('Mini App initialization queue full');
            pending.push(payload);
        }
    };
    const endpoint = Object.freeze({postMessage(payload) {
        if (typeof payload !== 'string' || payload.length > 32768)
            throw new RangeError('Mini App request exceeds the host limit');
        const now = performance.now();
        if (now - windowStart >= 1000) { windowStart = now; calls = 0; }
        if (++calls > 120) throw new Error('Mini App host rate limit exceeded');
        send(payload);
    }});
    Object.defineProperty(window, 'ReactNativeWebView', {value: endpoint});
    Object.defineProperty(window, '__genyMiniAppReply', {value: (doc, data) => {
        if (doc === token)
            document.dispatchEvent(new MessageEvent('FarcasterFrameCallback', {data}));
    }});
    Object.defineProperty(window, '__genyMiniAppEthProviderEvent', {value: (doc, data) => {
        if (doc === token)
            document.dispatchEvent(new MessageEvent('FarcasterFrameEthProviderEvent', {data}));
    }});
    Object.defineProperty(window, '__genyMiniAppEvent', {value: (doc, data) => {
        if (doc === token)
            document.dispatchEvent(new MessageEvent('FarcasterFrameEvent', {data}));
    }});
    if (typeof QWebChannel !== 'function' || !window.qt?.webChannelTransport) return;
    new QWebChannel(qt.webChannelTransport, (connection) => {
        channel = connection.objects.genyMiniAppBridge || null;
        if (!channel) return;
        for (const payload of pending.splice(0)) channel.postMessage(payload, token);
    });
})();
)JS");
}

class SecuredPage final : public QWebEnginePage {
public:
    SecuredPage(
        QWebEngineProfile* const profile, const QUrl& origin,
        std::function<void(const QString&)> failed, QObject* const parent)
        : QWebEnginePage{profile, parent}, m_origin{origin},
          m_failed{std::move(failed)} {}

protected:
    [[nodiscard]] bool acceptNavigationRequest(
        const QUrl& url, NavigationType type, const bool isMainFrame) override
    {
        if (!publicHttps(url)
            || (isMainFrame && url.host().compare(m_origin.host(), Qt::CaseInsensitive) != 0)) {
            if (isMainFrame && m_failed) {
                m_failed(QObject::tr("Mini App navigation blocked: untrusted origin"));
            }
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }

    QWebEnginePage* createWindow(WebWindowType) override { return nullptr; }

private:
    QUrl m_origin{};
    std::function<void(const QString&)> m_failed{};
};

class SdkTransport final : public QObject {
    Q_OBJECT
public:
    explicit SdkTransport(
        QWebEnginePage* const page, const QUrl& origin,
        std::function<void(const QJsonObject&, const QString&)> message,
        QObject* const parent)
        : QObject{parent}, m_page{page}, m_origin{origin},
          m_message{std::move(message)} {}

    void resetDocument()
    {
        m_document.clear();
        m_window.invalidate();
        m_requests = 0;
    }

    Q_INVOKABLE void postMessage(const QString& value, const QString& document)
    {
        if (!m_page || value.size() > 32768 || value.isEmpty()
            || document.isEmpty() || document.size() > 64
            || !publicHttps(m_page->url())
            || m_page->url().host().compare(m_origin.host(), Qt::CaseInsensitive) != 0) {
            return;
        }
        // The document nonce must not change during one navigation. Old RPCs
        // cannot issue work after the next main-frame document starts loading.
        if (m_document.isEmpty()) m_document = document;
        if (document != m_document) return;
        if (!m_window.isValid() || m_window.elapsed() >= 1000) {
            m_window.restart();
            m_requests = 0;
        }
        if (++m_requests > 120) return;
        QJsonParseError error{};
        const auto json = QJsonDocument::fromJson(value.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && json.isObject() && m_message) {
            m_message(json.object(), document);
        }
    }

private:
    QPointer<QWebEnginePage> m_page{};
    QUrl m_origin{};
    std::function<void(const QJsonObject&, const QString&)> m_message{};
    QElapsedTimer m_window{};
    QString m_document{};
    int m_requests{};
};

class HostWindow final : public QMainWindow {
public:
    std::function<void()> onClose{};

protected:
    void closeEvent(QCloseEvent* event) override
    {
        QMainWindow::closeEvent(event);
        if (!event->isAccepted() || !onClose) return;
        // Defer after the QWidget event stack. The runtime may destroy this
        // Browser in response to the closed signal.
        QTimer::singleShot(0, QCoreApplication::instance(), std::move(onClose));
    }
};

[[nodiscard]] QRect placement(const farcaster::platform::miniapp::PresentationMode mode)
{
    auto* const screen = QGuiApplication::primaryScreen();
    if (!screen) return QRect{100, 100, 800, 720};
    const QRect available = screen->availableGeometry();
    const int width = std::min(820, std::max(380, available.width() * 7 / 10));
    const int height = std::min(760, std::max(420, available.height() * 9 / 10));
    using Mode = farcaster::platform::miniapp::PresentationMode;
    switch (mode) {
    case Mode::Left:
        return QRect{available.left(), available.top(), width, available.height()};
    case Mode::Right:
        return QRect{available.right() - width + 1, available.top(), width, available.height()};
    case Mode::Bottom:
        return QRect{available.left(), available.bottom() - height + 1,
                     available.width(), height};
    case Mode::Window:
        return QRect{available.center().x() - width / 2,
                     available.center().y() - height / 2, width, height};
    }
    return QRect{100, 100, 800, 720};
}
} // namespace

namespace farcaster::platform::miniapp {
namespace {
class LinuxBrowser final : public Browser {
public:
    LinuxBrowser(const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
                 const PresentationMode mode, bool /*reduceMotion*/,
                 const bool initiallyVisible, const BrowserRole role)
        : m_profile{std::make_unique<QWebEngineProfile>()},
          m_callbacks{std::move(callbacks)}, m_role{role}
    {
        if (!publicHttps(origin)) return;
        m_window = std::make_unique<HostWindow>();
        m_window->setWindowTitle(title.isEmpty() ? origin.host() : title);
        m_window->setGeometry(placement(mode));
        m_window->setAttribute(Qt::WA_DeleteOnClose, false);

        auto* const toolbar = new QToolBar{m_window.get()};
        toolbar->setMovable(false);
        m_back = toolbar->addAction(QObject::tr("Back"));
        auto* const close = toolbar->addAction(QObject::tr("Close"));
        auto* const identity = new QLabel{origin.host(), toolbar};
        toolbar->addWidget(identity);
        m_window->addToolBar(toolbar);
        QObject::connect(close, &QAction::triggered, m_window.get(),
                         [this] { if (m_window) m_window->close(); });

        m_web = new QWebEngineView{m_window.get()};
        m_window->setCentralWidget(m_web);
        m_page = new SecuredPage{
            m_profile.get(), origin, m_callbacks.failed, m_web};
        m_web->setPage(m_page);
        QObject::connect(m_page, &QWebEnginePage::permissionRequested,
                         m_page, [](const QWebEnginePermission& request) {
            // Never grant camera, microphone, geolocation, notifications,
            // clipboard or desktop capture without a separate permission UI.
            request.deny();
        });
        m_page->settings()->setAttribute(
            QWebEngineSettings::JavascriptCanOpenWindows, false);
        m_page->settings()->setAttribute(
            QWebEngineSettings::LocalContentCanAccessFileUrls, false);
        m_page->settings()->setAttribute(
            QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
        m_page->settings()->setAttribute(
            QWebEngineSettings::PlaybackRequiresUserGesture, true);

        if (m_role == BrowserRole::MiniApp) {
            const QString bridgeSource = documentBridgeScript();
            if (bridgeSource.isEmpty()) {
                // Factory rejects this instance and tears down its window
                // before any unbridged document can navigate.
                m_page = nullptr;
                return;
            }
            m_channel = new QWebChannel{m_page};
            m_transport = new SdkTransport{
                m_page, origin, m_callbacks.message, m_page};
            m_channel->registerObject(
                QStringLiteral("genyMiniAppBridge"), m_transport);
            m_page->setWebChannel(m_channel, QWebEngineScript::MainWorld);
            QWebEngineScript script{};
            script.setName(QStringLiteral("GenyCasterMiniAppSdk"));
            script.setSourceCode(bridgeSource);
            script.setInjectionPoint(QWebEngineScript::DocumentCreation);
            script.setWorldId(QWebEngineScript::MainWorld);
            script.setRunsOnSubFrames(false);
            m_page->scripts().insert(script);
        }

        QObject::connect(m_page, &QWebEnginePage::loadStarted,
                         m_window.get(), [this] {
            if (m_transport) m_transport->resetDocument();
            if (m_callbacks.navigationStarted) m_callbacks.navigationStarted();
        });
        QObject::connect(m_page, &QWebEnginePage::loadFinished,
                         m_window.get(), [this](const bool success) {
            if (!success && m_callbacks.failed)
                m_callbacks.failed(QObject::tr("Mini App could not load"));
        });
        QObject::connect(m_page, &QWebEnginePage::urlChanged,
                         m_window.get(), [this](const QUrl&) {
            if (m_back && m_page)
                m_back->setEnabled(m_sdkBack || m_page->canGoBack());
        });
        QObject::connect(m_back, &QAction::triggered,
                         m_window.get(), [this] {
            if (m_page && m_page->canGoBack()) m_page->triggerAction(QWebEnginePage::Back);
        });
        m_back->setEnabled(false);
        m_window->onClose = m_callbacks.closed;
        if (initiallyVisible) m_window->show();
    }

    ~LinuxBrowser() override
    {
        if (m_window) {
            m_window->onClose = {};
            m_window.reset();
        }
        m_profile.reset();
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return m_page != nullptr && m_window != nullptr
            && (m_role != BrowserRole::MiniApp || m_transport != nullptr);
    }

    void load(const QUrl& url, const QUrl& referrer) override
    {
        if (!m_page || !publicHttps(url)) return;
        QWebEngineHttpRequest request{url};
        if (publicHttps(referrer))
            request.setHeader("Referer", referrer.toString(QUrl::FullyEncoded).toUtf8());
        m_page->load(request);
    }

    void status(const QString& message, const bool failed) override
    {
        if (!m_window) return;
        m_window->statusBar()->showMessage(message);
        if (failed && m_page) m_page->triggerAction(QWebEnginePage::Stop);
    }

    void ready(bool /*disableNativeGestures*/) override
    {
        if (m_window) m_window->statusBar()->clearMessage();
    }

    void setBackVisible(const bool visible) override
    {
        m_sdkBack = visible;
        if (m_back && m_page)
            m_back->setEnabled(visible || m_page->canGoBack());
    }

    void reply(const QJsonObject& value, const QString& document) override
    {
        dispatch(QStringLiteral("__genyMiniAppReply"), QJsonArray{document, value});
    }

    void ethereumProviderEvent(
        const QString& event, const QJsonArray& params,
        const QString& document) override
    {
        dispatch(QStringLiteral("__genyMiniAppEthProviderEvent"),
                 QJsonArray{document, QJsonObject{{QStringLiteral("event"), event},
                                                 {QStringLiteral("params"), params}}});
    }

    void miniAppEvent(const QJsonObject& event, const QString& document) override
    {
        dispatch(QStringLiteral("__genyMiniAppEvent"), QJsonArray{document, event});
    }

    void setVisible(const bool visible) override
    {
        if (!m_window) return;
        if (visible) {
            m_window->show();
            m_window->raise();
            m_window->activateWindow();
        } else {
            m_window->hide();
        }
    }

    [[nodiscard]] qlonglong processIdentifier() const noexcept override
    {
        // Chromium renderer processes are not the application PID. Qt does not
        // expose a stable public per-QWebEnginePage renderer PID.
        return 0;
    }

private:
    void dispatch(const QString& callback, const QJsonArray& arguments)
    {
        if (!m_page || m_role != BrowserRole::MiniApp) return;
        // JSON serialization prevents injecting arbitrary JavaScript source.
        const QString encoded = QString::fromUtf8(
            QJsonDocument{arguments}.toJson(QJsonDocument::Compact));
        m_page->runJavaScript(
            QStringLiteral("window.") + callback
                + QStringLiteral("?.(...") + encoded + QStringLiteral(")"),
            QWebEngineScript::MainWorld);
    }

    std::unique_ptr<QWebEngineProfile> m_profile{};
    std::unique_ptr<HostWindow> m_window{};
    BrowserCallbacks m_callbacks{};
    BrowserRole m_role{BrowserRole::MiniApp};
    QWebEngineView* m_web{};
    SecuredPage* m_page{};
    QWebChannel* m_channel{};
    SdkTransport* m_transport{};
    QAction* m_back{};
    bool m_sdkBack{false};
};
} // namespace

std::unique_ptr<Browser> createBrowser(
    const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
    PresentationMode mode, bool reduceMotion, bool initiallyVisible, BrowserRole role)
{
    auto browser = std::make_unique<LinuxBrowser>(
        title, origin, std::move(callbacks), mode, reduceMotion,
        initiallyVisible, role);
    if (!browser->valid()) return {};
    return browser;
}
} // namespace farcaster::platform::miniapp

#include "browser_linux.moc"
