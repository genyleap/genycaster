#pragma once

// Desktop process arbitration. Keep the OS-visible program single-instance
// without preventing independent Qt smoke/visual-test processes.
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QPointer>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QString>

#include <memory>

namespace genycaster::ui {

class DesktopSingleInstance final {
public:
    enum class Result { Primary, ActivatedExisting, Error };

    [[nodiscard]] Result acquire()
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
                socket.write("activate\n");
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
                connection->deleteLater();
                activate();
            }
        });
        return Result::Primary;
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
        m_window->showNormal();
        m_window->raise();
        m_window->requestActivate();
    }

    std::unique_ptr<QLockFile> m_lock;
    std::unique_ptr<QLocalServer> m_server;
    QPointer<QQuickWindow> m_window;
    QString m_serverName;
    QString m_error;
    bool m_pendingActivation{false};
};

} // namespace genycaster::ui

#endif
