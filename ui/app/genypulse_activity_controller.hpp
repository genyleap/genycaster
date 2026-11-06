/*!
 * @file        ui/app/genypulse_activity_controller.hpp
 * @brief       GenyPulse activity aggregation controller.
 *
 * @details
 * Presents independently sampled public activity signals and bounded
 * refresh state without promoting derived insights to protocol authority.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QVariantMap>
#include <QSet>
#include <QDateTime>
#include <QString>
#include <QUrl>

/** @brief Independently samples activity from public Farcaster and GitHub reads.
 * No provider credentials, private history or QML network calls. Each source
 * has its own request lifecycle, bounded pagination and explicit coverage.
 * Network failure cannot erase a previously confirmed snapshot of that identity.
 */
class GenyPulseActivityController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap farcaster READ farcaster NOTIFY farcasterChanged)
    Q_PROPERTY(QVariantMap github READ github NOTIFY githubChanged)
    Q_PROPERTY(QVariantMap githubContributions READ githubContributions NOTIFY githubContributionsChanged)
    Q_PROPERTY(QVariantMap onchain READ onchain NOTIFY onchainChanged)
public:
    explicit GenyPulseActivityController(QObject* parent = nullptr);
    [[nodiscard]] QVariantMap farcaster() const { return m_farcaster; }
    [[nodiscard]] QVariantMap github() const { return m_github; }
    [[nodiscard]] QVariantMap githubContributions() const { return m_githubContributions; }
    [[nodiscard]] QVariantMap onchain() const { return m_onchain; }
    Q_INVOKABLE void requestFarcaster(qulonglong fid);
    /** Only pass an OpenProof-confirmed GitHub account identifier, never a bio URL. */
    Q_INVOKABLE void requestGitHub(const QString& username);
    /** Resolve a numeric provider subject only after OpenProof confirmed the link. */
    Q_INVOKABLE void requestGitHubId(const QString& accountId);
    Q_INVOKABLE void requestGitHubContributions(const QString& username, int year);
    /** No chain-agnostic activity is inferred from a wallet verification. */
    Q_INVOKABLE void setVerifiedWallet(const QString& address);
signals:
    void farcasterChanged();
    void githubChanged();
    void githubContributionsChanged();
    void onchainChanged();
private:
    void fetchFarcasterPage(qulonglong fid, QString cursor, int page);
    void fetchGitHubPage(const QString& username, int page);
    void publishFarcaster(bool complete);
    void publishGitHub(bool complete);
    QNetworkAccessManager m_http{this};
    QVariantMap m_farcaster{{QStringLiteral("status"), QStringLiteral("idle")}};
    QVariantMap m_github{{QStringLiteral("status"), QStringLiteral("unlinked")}};
    QVariantMap m_onchain{{QStringLiteral("status"), QStringLiteral("unlinked")}};
    QVariantMap m_githubContributions{{QStringLiteral("status"), QStringLiteral("idle")}};
    QPointer<QNetworkReply> m_farcasterReply;
    QPointer<QNetworkReply> m_githubReply;
    QPointer<QNetworkReply> m_githubResolveReply;
    QPointer<QNetworkReply> m_githubContributionsReply;
    QString m_githubCalendarLogin;
    int m_githubCalendarYear{};
    qint64 m_githubCalendarFetchedAt{};
    quint64 m_githubCalendarGeneration{};
    QString m_githubAccountId{};
    qulonglong m_fid{};
    QString m_githubLogin{};
    QString m_wallet{};
    QSet<QString> m_castHashes;
    QSet<QString> m_githubEvents;
    QVariantMap m_castBuckets;
    QVariantMap m_githubBuckets;
    qint64 m_castOldestObservedAt{};
    int m_castCount{};
    int m_replyCount{};
    int m_eventCount{};
    qint64 m_castFetchedAt{};
    qint64 m_githubFetchedAt{};
    quint64 m_castGeneration{};
    quint64 m_githubGeneration{};
};
