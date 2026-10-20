#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>

class QNetworkReply;

// Browser-only positive reaction evidence. This outer Qt adapter owns HTTP;
// the Farcaster domain and application layers remain provider-neutral.
class BrowserReactionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint64 revision READ revision NOTIFY revisionChanged FINAL)

public:
    explicit BrowserReactionController(
        bool useDirectProxy, bool networkEnabled, QObject* parent = nullptr,
        QUrl endpoint = {}, int dispatchDelayMs = 220);
    ~BrowserReactionController() override;

    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    Q_INVOKABLE void retain(const QString& castId, double viewerFid);
    Q_INVOKABLE void release(const QString& castId, double viewerFid);
    Q_INVOKABLE void ensure(const QString& castId, double viewerFid);
    Q_INVOKABLE [[nodiscard]] bool reacted(
        const QString& castId, double viewerFid, const QString& kind) const;
    Q_INVOKABLE [[nodiscard]] QVariant localValue(
        const QString& castId, double viewerFid, const QString& kind) const;
    Q_INVOKABLE void setLocal(
        const QString& castId, double viewerFid, const QString& kind, bool active);

signals:
    void revisionChanged();

private:
    struct Entry final {
        QString castId{};
        qint64 viewerFid{};
        qint64 retryAfter{0};
        qint64 lastUsed{0};
        int consumers{0};
        bool inFlight{false};
        bool liked{false};
        bool recasted{false};
    };

    [[nodiscard]] static QString canonicalKey(const QString& castId, double viewerFid);
    [[nodiscard]] Entry* getOrCreate(const QString& key);
    void queueInterested();
    void dispatch();
    void finish(QNetworkReply* reply, QStringList keys);
    void notifyChange();

    QHash<QString, Entry> m_entries{};
    QHash<QString, bool> m_local{};
    QStringList m_pending{};
    QElapsedTimer m_clock{};
    QTimer m_dispatchTimer{};
    QTimer m_retryTimer{};
    QNetworkAccessManager m_network{};
    QNetworkReply* m_activeReply{nullptr};
    QByteArray m_response{};
    QUrl m_endpoint{};
    quint64 m_revision{0};
    bool m_enabled{false};
    bool m_tooLarge{false};
};
