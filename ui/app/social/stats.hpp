#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QNetworkReply;

// Read-only, FID/provider-scoped activity analytics. No QML HTTP or guessed metrics.
// All aggregation and request cancellation live on the GUI thread.
class StatsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed FINAL)
    Q_PROPERTY(QVariantList activity READ activity NOTIFY changed FINAL)
    Q_PROPERTY(QVariantList topCasts READ topCasts NOTIFY changed FINAL)
    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY changed FINAL)
    Q_PROPERTY(int loadedCount READ loadedCount NOTIFY changed FINAL)
public:
    explicit StatsController(bool directProxy, bool networkEnabled, QObject* parent = nullptr);
    ~StatsController() override;

    [[nodiscard]] QString state() const { return m_state; }
    [[nodiscard]] QVariantMap summary() const { return m_summary; }
    [[nodiscard]] QVariantList activity() const { return m_activity; }
    [[nodiscard]] QVariantList topCasts() const { return m_topCasts; }
    [[nodiscard]] QString updatedAt() const { return m_updatedAt; }
    [[nodiscard]] int loadedCount() const { return m_loadedCount; }

    Q_INVOKABLE void load(QString fid, QString contract, QString endpoint,
                          int days, bool force = false);
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void cancel();
    void fail(QString reason);
    void getProfile();
    void getCasts();
    void processPage(const QJsonObject& data);
    void complete();
    void emitChanges();
    [[nodiscard]] QUrl endpointUrl(const QString& route,
                                    const QHash<QString, QString>& query) const;

    QNetworkAccessManager m_network{};
    QPointer<QNetworkReply> m_reply{};
    QString m_state{QStringLiteral("idle")};
    QString m_endpoint{};
    QString m_contract{};
    QString m_fid{};
    QString m_key{};
    QString m_lastSuccessfulKey{};
    QString m_cursor{};
    QString m_updatedAt{};
    QVariantMap m_summary{};
    QVariantList m_activity{};
    QVariantList m_topCasts{};
    QSet<QString> m_seenHashes{};
    // Responses are snapshot totals on posts authored on each date. This
    // series is NOT an event-by-event history of when likes arrived.
    struct ReceivedOnPublishedPosts {
        qint64 likes{};
        qint64 recasts{};
        qint64 receivedReplies{};
    };
    QHash<QString, QPair<int, int>> m_dayCounts{};
    QHash<QString, ReceivedOnPublishedPosts> m_receivedByPublishedDay{};
    QDateTime m_cutoff{};
    qint64 m_likes{0};
    qint64 m_recasts{0};
    qint64 m_receivedReplies{0};
    qint64 m_followers{-1};
    qint64 m_following{-1};
    int m_casts{0};
    int m_replies{0};
    int m_loadedCount{0};
    int m_pages{0};
    int m_days{30};
    bool m_raw{false};
    bool m_networkEnabled{false};
    bool m_partial{false};
    bool m_fetchedProfile{false};
    bool m_endedByDate{false};
    qint64 m_cachedAtMs{0};
    quint64 m_epoch{0};
};
