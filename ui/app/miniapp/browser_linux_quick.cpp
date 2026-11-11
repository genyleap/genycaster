/*!
 * @file ui/app/miniapp/browser_linux_quick.cpp
 * @brief Linux Mini App host embedded into the existing Qt Quick application window.
 *
 * The browser/SDK remains owned by the same runtime interface as macOS.
 * Qt WebEngine Quick supplies a QML item rather than a second native window.
 */
#include "browser_linux_quick.hpp"
#include "browser.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QVariant>
#include <QDebug>

#include <functional>
#include <memory>
#include <utility>

namespace farcaster::platform::miniapp {

// Defined in browser_linux.cpp so the embedded and separate-window hosts
// enforce exactly the same network policy and JavaScript transport contract.
[[nodiscard]] bool linuxBrowserPublicHttps(const QUrl& url);
[[nodiscard]] QString linuxBrowserPublicScript();
[[nodiscard]] QString linuxBrowserTransportScript();

namespace {
QPointer<QQuickWindow> applicationWindow{};
QPointer<QQmlEngine> applicationEngine{};

class QuickBrowser;

class QuickSdkTransport final : public QObject {
    Q_OBJECT
public:
    QuickSdkTransport(QuickBrowser* browser,
                      std::function<void(const QJsonObject&, const QString&)> message)
        : m_browser{browser}, m_message{std::move(message)} {}

    void resetDocument()
    {
        m_document.clear();
        m_window.invalidate();
        m_requests = 0;
    }

    Q_INVOKABLE void postMessage(const QString& data, const QString& document);

private:
    QuickBrowser* m_browser{};
    std::function<void(const QJsonObject&, const QString&)> m_message{};
    QString m_document{};
    QElapsedTimer m_window{};
    int m_requests{};
};

class QuickBrowser final : public QObject, public Browser {
    Q_OBJECT
    Q_PROPERTY(QObject* hostItem READ hostItem CONSTANT)
    Q_PROPERTY(QObject* transport READ transport CONSTANT)
    Q_PROPERTY(QString publicScript READ publicScript CONSTANT)
    Q_PROPERTY(QString transportScript READ transportScript CONSTANT)
    Q_PROPERTY(QString hostDomain READ hostDomain CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(int presentationMode READ presentationMode CONSTANT)
    Q_PROPERTY(bool initiallyVisible READ initiallyVisible CONSTANT)
    Q_PROPERTY(bool reduceMotion READ reduceMotion CONSTANT)
public:
    QuickBrowser(QString title, const QUrl& origin, BrowserCallbacks callbacks,
                 PresentationMode mode, bool reduceMotion, bool initiallyVisible)
        : m_origin{origin}, m_title{std::move(title)},
          m_callbacks{std::move(callbacks)}, m_mode{mode},
          m_reduceMotion{reduceMotion}, m_initiallyVisible{initiallyVisible}
    {
        if (!applicationWindow || !applicationEngine || !linuxBrowserPublicHttps(origin))
            return;
        m_publicScript = linuxBrowserPublicScript();
        m_transportScript = linuxBrowserTransportScript();
        // Never open a privileged Mini App page without the bridge.
        if (m_publicScript.isEmpty() || m_transportScript.isEmpty())
            return;

        m_transport = std::make_unique<QuickSdkTransport>(this, m_callbacks.message);
        QQmlEngine::setObjectOwnership(m_transport.get(), QQmlEngine::CppOwnership);

        QQmlComponent component{
            applicationEngine.data(),
            QUrl{QStringLiteral("qrc:/genycaster/miniapp/EmbeddedBrowser.qml")},
            QQmlComponent::PreferSynchronous
        };
        if (!component.isReady()) {
            qWarning().noquote() << "GenyCaster Linux Mini App drawer:"
                                 << component.errorString();
            return;
        }
        m_popup = component.createWithInitialProperties(
            {{QStringLiteral("adapter"), QVariant::fromValue(static_cast<QObject*>(this))}});
        if (!m_popup) {
            qWarning().noquote() << "GenyCaster Linux Mini App drawer:"
                                 << component.errorString();
            return;
        }
        m_view = m_popup->property("browserView").value<QObject*>();
        if (!m_view) {
            qWarning() << "GenyCaster Linux Mini App drawer lacks WebEngineView";
            delete m_popup;
            m_popup = nullptr;
        }
    }

    ~QuickBrowser() override
    {
        // Hide before destruction to prevent QML onClosed from issuing a
        // second runtime teardown while the view is being destroyed.
        if (m_popup) {
            m_popup->setProperty("hostVisible", false);
            delete m_popup;
        }
    }

    [[nodiscard]] bool valid() const noexcept { return m_popup && m_view && m_transport; }
    [[nodiscard]] QObject* hostItem() const
    {
        return applicationWindow ? applicationWindow->contentItem() : nullptr;
    }
    [[nodiscard]] QObject* transport() const { return m_transport.get(); }
    [[nodiscard]] QString publicScript() const { return m_publicScript; }
    [[nodiscard]] QString transportScript() const { return m_transportScript; }
    [[nodiscard]] QString hostDomain() const { return m_origin.host(); }
    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] int presentationMode() const { return static_cast<int>(m_mode); }
    [[nodiscard]] bool initiallyVisible() const { return m_initiallyVisible; }
    [[nodiscard]] bool reduceMotion() const { return m_reduceMotion; }
    [[nodiscard]] QUrl currentPageUrl() const { return m_url; }
    [[nodiscard]] QUrl trustedOrigin() const { return m_origin; }

    Q_INVOKABLE bool allowNavigation(const QUrl& url, bool isMainFrame)
    {
        if (linuxBrowserPublicHttps(url)
            && (!isMainFrame
                || url.host().compare(m_origin.host(), Qt::CaseInsensitive) == 0))
            return true;
        if (isMainFrame && m_callbacks.failed)
            m_callbacks.failed(tr("Mini App navigation blocked: untrusted origin"));
        return false;
    }

    Q_INVOKABLE void pageUrlChanged(const QUrl& url) { m_url = url; }
    Q_INVOKABLE void navigationStarted()
    {
        m_transport->resetDocument();
        if (m_callbacks.navigationStarted) m_callbacks.navigationStarted();
    }
    Q_INVOKABLE void loadFailed()
    {
        if (m_callbacks.failed) m_callbacks.failed(tr("Mini App could not load"));
    }
    Q_INVOKABLE void requestClose()
    {
        if (m_closePosted) return;
        m_closePosted = true;
        auto callback = m_callbacks.closed;
        QTimer::singleShot(0, QCoreApplication::instance(),
                           [callback = std::move(callback)] {
            if (callback) callback();
        });
    }
    Q_INVOKABLE void requestActivated()
    {
        if (m_callbacks.activated) m_callbacks.activated();
    }

    void load(const QUrl& url, const QUrl& /*referrer*/) override
    {
        if (m_popup && linuxBrowserPublicHttps(url)
            && url.host().compare(m_origin.host(), Qt::CaseInsensitive) == 0)
            m_popup->setProperty("requestedUrl", url);
    }

    void status(const QString& message, bool failed) override
    {
        if (!m_popup) return;
        m_popup->setProperty("statusText", message);
        if (failed && m_view)
            QMetaObject::invokeMethod(m_view, "stop");
    }

    void ready(bool /*disableNativeGestures*/) override
    {
        if (m_popup) m_popup->setProperty("statusText", QString{});
    }

    void setBackVisible(bool visible) override
    {
        if (m_popup) m_popup->setProperty("sdkBack", visible);
    }

    void reply(const QJsonObject& value, const QString& document) override
    {
        dispatch(QStringLiteral("__genyMiniAppReply"), QJsonArray{document, value});
    }
    void ethereumProviderEvent(const QString& event, const QJsonArray& params,
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

    void setVisible(bool visible) override
    {
        if (m_popup) m_popup->setProperty("hostVisible", visible);
    }
    [[nodiscard]] qlonglong processIdentifier() const noexcept override
    {
        // Unlike Widgets, Qt WebEngine Quick publicly exposes its renderer PID.
        return m_popup ? m_popup->property("rendererPid").toLongLong() : 0;
    }

private:
    void dispatch(const QString& callback, const QJsonArray& arguments)
    {
        if (!m_popup) return;
        const QString encoded = QString::fromUtf8(
            QJsonDocument{arguments}.toJson(QJsonDocument::Compact));
        // The counter forces a property change even for identical SDK events.
        const QString script = QStringLiteral("window.") + callback
            + QStringLiteral("?.(...") + encoded + QStringLiteral(");\n// ")
            + QString::number(++m_dispatchCounter);
        m_popup->setProperty("outboundScript", script);
    }

    QUrl m_origin{};
    QUrl m_url{};
    QString m_title{};
    BrowserCallbacks m_callbacks{};
    PresentationMode m_mode{PresentationMode::Bottom};
    bool m_reduceMotion{};
    bool m_initiallyVisible{};
    bool m_closePosted{};
    quint64 m_dispatchCounter{};
    QString m_publicScript{};
    QString m_transportScript{};
    QPointer<QObject> m_popup{};
    QPointer<QObject> m_view{};
    std::unique_ptr<QuickSdkTransport> m_transport{};
};

void QuickSdkTransport::postMessage(const QString& data, const QString& document)
{
    if (!m_browser || !linuxBrowserPublicHttps(m_browser->currentPageUrl())
        || m_browser->currentPageUrl().host().compare(
               m_browser->trustedOrigin().host(), Qt::CaseInsensitive) != 0
        || data.isEmpty() || data.size() > 32768
        || document.isEmpty() || document.size() > 64)
        return;

    if (m_document.isEmpty()) m_document = document;
    if (document != m_document) return;
    if (!m_window.isValid() || m_window.elapsed() >= 1000) {
        m_window.restart();
        m_requests = 0;
    }
    if (++m_requests > 120) return;
    QJsonParseError error{};
    const auto parsed = QJsonDocument::fromJson(data.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && parsed.isObject() && m_message)
        m_message(parsed.object(), document);
}
} // namespace

void setLinuxMiniAppQuickHost(QQuickWindow* window, QQmlEngine* engine)
{
    applicationWindow = window;
    applicationEngine = engine;
}

std::unique_ptr<Browser> createLinuxQuickBrowser(
    const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
    PresentationMode mode, bool reduceMotion, bool initiallyVisible)
{
    auto browser = std::make_unique<QuickBrowser>(
        title, origin, std::move(callbacks), mode, reduceMotion, initiallyVisible);
    if (!browser->valid()) return {};
    return browser;
}

} // namespace farcaster::platform::miniapp

#include "browser_linux_quick.moc"
