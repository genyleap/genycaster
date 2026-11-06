/*!
 * @file        ui/app/cast_view_controller.hpp
 * @brief       Qt cast-detail view state controller.
 *
 * @details
 * Maintains per-cast reactive state and bounded update requests so
 * conversation detail presentation remains consistent during navigation.
 *
 * @author      Kambiz Asadzadeh
 * @since       20 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
    enum class Lifecycle { Idle, Queued, Active, Resolved, Unavailable, Failed, CanceledBeforeDispatch, Invalid };
    Lifecycle m_state{Lifecycle::Idle};
    qint64 m_desiredAuthorFid{0};
    QString m_desiredText{};
    qint64 m_sentAuthorFid{0};
    QString m_sentText{};
    bool m_metadataFollowUpUsed{false};
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
        QUrl endpoint = {}, qint64 retryIntervalMs = 90 * 1000);
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
    void queueInterested();
    void dispatch();
    void finish(QNetworkReply* reply, const QStringList& hashes);

    QElapsedTimer m_clock{};
    QHash<QString, CastViewEntry*> m_entries{};
    QHash<QString, Pending> m_pending{};
    QNetworkAccessManager m_network{};
    QUrl m_endpoint{};
    QTimer m_dispatchTimer{};
    QTimer m_retryTimer{};
    QNetworkReply* m_activeReply{nullptr};
    bool m_networkEnabled{false};
    qint64 m_retryIntervalMs{90 * 1000};
};
