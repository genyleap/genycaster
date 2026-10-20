#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QNetworkReply;

/**
 * @brief Optional Qt-network browser token read adapter.
 * HTTP and JSON never cross into QML or core. Exact token identity is
 * validated before a positive snapshot is published; errors remain unknown.
 */
class BrowserTokenPreviewController final : public QObject {
    Q_OBJECT
public:
    explicit BrowserTokenPreviewController(bool useDirectProxy, bool enabled,
        QObject* parent = nullptr, QUrl gatewayBase = {});
    ~BrowserTokenPreviewController() override;

    Q_INVOKABLE void requestToken(const QString& locator);
    Q_INVOKABLE [[nodiscard]] QVariantMap tokenFor(const QString& locator) const;

signals:
    void tokenReady(const QString& locator, const QVariantMap& snapshot);

private:
    [[nodiscard]] static QString normalize(const QString& locator);
    void dispatch();
    void complete(QNetworkReply* reply, const QString& locator);
    void retain(const QString& locator, QVariantMap snapshot);

    QNetworkAccessManager m_network{};
    QUrl m_gatewayBase{};
    QElapsedTimer m_clock{};
    QQueue<QString> m_pending{};
    QHash<QString, QNetworkReply*> m_active{};
    struct Stored final {
        QVariantMap snapshot{};
        qint64 freshUntil{};
    };
    QHash<QString, Stored> m_cache{};
    QHash<QString, qint64> m_retryAfter{};
    bool m_enabled{};
};
