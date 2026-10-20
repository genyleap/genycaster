#pragma once

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

// A per-cast observable Qt value; changing a view count never invalidates
// unrelated cast, text, media or reaction bindings in QML.
class CastViewEntry final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariant count READ count NOTIFY countChanged FINAL)

public:
    explicit CastViewEntry(QString castId, QObject* parent = nullptr);
    [[nodiscard]] QVariant count() const { return m_count; }

signals:
    void countChanged();

private:
    friend class CastViewController;
    QString m_castId{};
    QVariant m_count{};
    qint64 m_freshUntil{0};
    qint64 m_retryAfter{0};
    qint64 m_touched{0};
    int m_consumers{0};
    bool m_inFlight{false};
};

// Presentation-side adapter for derived, non-protocol cast analytics.
// The gateway only returns positive numeric evidence; missing means unknown.
// No QML-owned HTTP, JSON parsing or global count revision.
class CastViewController final : public QObject {
    Q_OBJECT
public:
    explicit CastViewController(
        bool useDirectProxy, bool networkEnabled, QObject* parent = nullptr,
        QUrl endpoint = {});
    ~CastViewController() override;

    Q_INVOKABLE [[nodiscard]] QObject* entryFor(const QString& castId);
    Q_INVOKABLE void retain(const QString& castId);
    Q_INVOKABLE void release(const QString& castId);
    Q_INVOKABLE void ensure(const QString& castId, double authorFid, const QString& text);

private:
    struct Pending {
        QString castId{};
        qint64 authorFid{};
        QString text{};
    };

    [[nodiscard]] static QString normalizeHash(const QString& input);
    [[nodiscard]] CastViewEntry* getOrCreate(const QString& normalized);
    void evictUnused();
    void dispatch();
    void finish(QNetworkReply* reply, const QStringList& hashes);

    QElapsedTimer m_clock{};
    QHash<QString, CastViewEntry*> m_entries{};
    QHash<QString, Pending> m_pending{};
    QNetworkAccessManager m_network{};
    QUrl m_endpoint{};
    QTimer m_dispatchTimer{};
    QNetworkReply* m_activeReply{nullptr};
    bool m_networkEnabled{false};
};
