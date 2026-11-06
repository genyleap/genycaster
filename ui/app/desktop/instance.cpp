module;

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include <utility>

module genycaster.ui.desktop.instance;

namespace genycaster::ui {

struct DesktopSingleInstance::Impl {
    using Result = DesktopSingleInstance::Result;

    [[nodiscard]] Result acquire(const QString& requestedUrl = {})
    {
        const QString directory = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        // Do not use a global mutex: different OS users must not affect one
        // another, and this must work with standard-user installations.
        if (directory.isEmpty() || !QDir{}.mkpath(directory)) {
            m_error = QStringLiteral("Unable to create the per-user instance directory");
            return Result::Error;
        }

        const QString lockPath = QDir{directory}.filePath(
            QStringLiteral("genycaster.instance.lock"));
        const QByteArray fingerprint = QCryptographicHash::hash(
            QFileInfo{lockPath}.absoluteFilePath().toUtf8(),
            QCryptographicHash::Sha256).toHex().left(24);
        m_serverName = QStringLiteral("GenyCaster-") + QString::fromLatin1(fingerprint);
        m_lock = std::make_unique<QLockFile>(lockPath);
        m_lock->setStaleLockTime(0);
        if (!m_lock->tryLock(0)) {
            if (m_lock->error() != QLockFile::LockFailedError) {
                m_error = QStringLiteral("Unable to acquire the per-user instance lock");
                return Result::Error;
            }
            QLocalSocket socket;
            socket.connectToServer(m_serverName, QIODevice::WriteOnly);
            if (socket.waitForConnected(700)) {
                QJsonObject message{{QStringLiteral("action"), QStringLiteral("activate")}};
                const QUrl url{requestedUrl, QUrl::StrictMode};
                if (requestedUrl.size() <= 2048 && url.isValid() &&
                    url.scheme() == QStringLiteral("https") && !url.host().isEmpty() &&
                    url.userInfo().isEmpty()) {
                    message.insert(QStringLiteral("url"), requestedUrl);
                }
                QByteArray bytes = QJsonDocument{message}.toJson(QJsonDocument::Compact);
                bytes.append('\n');
                socket.write(bytes);
                socket.flush();
                socket.waitForBytesWritten(700);
            }
            return Result::ActivatedExisting;
        }

        // Only the lock owner may remove a stale endpoint; never disconnect a
        // live process's local server. The unique per-user name avoids collisions.
        QLocalServer::removeServer(m_serverName);
        m_server = std::make_unique<QLocalServer>();
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        if (!m_server->listen(m_serverName)) {
            m_error = QStringLiteral("Unable to listen on the instance activation channel: ")
                + m_server->errorString();
            return Result::Error;
        }
        QObject::connect(m_server.get(), &QLocalServer::newConnection,
                         m_server.get(), [this] {
            while (QLocalSocket* connection = m_server->nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                const auto consume = [this, connection, buffer] {
                    buffer->append(connection->readAll());
                    if (buffer->size() > 4096) {
                        connection->disconnectFromServer();
                        return;
                    }
                    const qsizetype end = buffer->indexOf('\n');
                    if (end < 0) return;
                    const auto doc = QJsonDocument::fromJson(buffer->left(end));
                    const auto data = doc.object();
                    if (doc.isObject() &&
                        data.value(QStringLiteral("action")).toString()
                            == QStringLiteral("activate")) {
                        activate();
                        const QString incoming = data.value(QStringLiteral("url")).toString();
                        const QUrl url{incoming, QUrl::StrictMode};
                        if (incoming.size() <= 2048 && url.isValid() &&
                            url.scheme() == QStringLiteral("https") &&
                            !url.host().isEmpty() && url.userInfo().isEmpty()) {
                            if (m_openUrlHandler) m_openUrlHandler(incoming);
                            else m_pendingUrl = incoming;
                        }
                    }
                    connection->disconnectFromServer();
                };
                QObject::connect(connection, &QLocalSocket::readyRead,
                                 connection, consume);
                QObject::connect(connection, &QLocalSocket::disconnected,
                                 connection, &QObject::deleteLater);
                if (connection->bytesAvailable()) consume();
            }
        });
        return Result::Primary;
    }

    void setOpenUrlHandler(std::function<void(const QString&)> handler)
    {
        m_openUrlHandler = std::move(handler);
        if (m_openUrlHandler && !m_pendingUrl.isEmpty()) {
            const QString incoming = std::exchange(m_pendingUrl, {});
            m_openUrlHandler(incoming);
        }
    }

    void setWindow(QQuickWindow* window)
    {
        m_window = window;
        if (m_pendingActivation) activate();
    }

    [[nodiscard]] QString errorString() const { return m_error; }

private:
    void activate()
    {
        if (!m_window) {
            m_pendingActivation = true;
            return;
        }
        m_pendingActivation = false;
        // Keep a maximized window maximized. Only restore from minimized;
        // unconditional showNormal() unexpectedly changes the user's layout.
        if (m_window->visibility() == QWindow::Minimized)
            m_window->showNormal();
        else if (!m_window->isVisible())
            m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }

    std::unique_ptr<QLockFile> m_lock;
    std::unique_ptr<QLocalServer> m_server;
    QPointer<QQuickWindow> m_window;
    QString m_serverName;
    QString m_error;
    QString m_pendingUrl;
    std::function<void(const QString&)> m_openUrlHandler;
    bool m_pendingActivation{false};
};


DesktopSingleInstance::DesktopSingleInstance()
    : m_impl{std::make_unique<Impl>()} {}

DesktopSingleInstance::~DesktopSingleInstance() = default;

DesktopSingleInstance::Result DesktopSingleInstance::acquire(const QString& requestedUrl)
{
    return m_impl->acquire(requestedUrl);
}

void DesktopSingleInstance::setWindow(QQuickWindow* window)
{
    m_impl->setWindow(window);
}

void DesktopSingleInstance::setOpenUrlHandler(std::function<void(const QString&)> handler)
{
    m_impl->setOpenUrlHandler(std::move(handler));
}

QString DesktopSingleInstance::errorString() const
{
    return m_impl->errorString();
}

} // namespace genycaster::ui
