/*!
 * @file        ui/app/social/channel_administration.hpp
 * @brief       Capability-gated channel administration controller.
 *
 * @details
 * Reads channel roles and validates permission state before actions.
 * Keeps managed channel operations separate from the cast-signing authority.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QNetworkReply;

class ChannelAdministrationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList following READ following NOTIFY changed FINAL)
    Q_PROPERTY(QVariantList members READ members NOTIFY changed FINAL)
    Q_PROPERTY(QVariantList invites READ invites NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap channel READ channel NOTIFY changed FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY changed FINAL)
    Q_PROPERTY(bool actionBusy READ actionBusy NOTIFY changed FINAL)
    Q_PROPERTY(bool canModerate READ canModerate NOTIFY changed FINAL)
    Q_PROPERTY(bool canWrite READ canWrite NOTIFY changed FINAL)
    Q_PROPERTY(bool followsAvailable READ followsAvailable NOTIFY changed FINAL)
    Q_PROPERTY(bool invitesAvailable READ invitesAvailable NOTIFY changed FINAL)
    Q_PROPERTY(QString providerNotice READ providerNotice NOTIFY changed FINAL)
    Q_PROPERTY(QString message READ message NOTIFY changed FINAL)
    Q_PROPERTY(QString activeReadContract READ activeReadContract NOTIFY changed FINAL)

public:
    ChannelAdministrationController(QString neynarApiKey, QString neynarSignerUuid,
                                    bool networkEnabled, bool directProxy,
                                    QObject* parent = nullptr);

    [[nodiscard]] QVariantList following() const { return m_following; }
    [[nodiscard]] QVariantList members() const { return m_members; }
    [[nodiscard]] QVariantList invites() const { return m_invites; }
    [[nodiscard]] QVariantMap channel() const { return m_channel; }
    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] bool actionBusy() const noexcept { return m_actionBusy; }
    [[nodiscard]] bool canModerate() const noexcept { return m_canModerate; }
    [[nodiscard]] bool canWrite() const noexcept { return m_signerApproved && !m_actionBusy; }
    [[nodiscard]] bool followsAvailable() const noexcept;
    [[nodiscard]] bool invitesAvailable() const noexcept;
    [[nodiscard]] QString providerNotice() const;
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] QString activeReadContract() const { return m_contract; }

    Q_INVOKABLE void configureSession(QString contract, QString endpoint,
                                     qulonglong viewerFid, QString appliedWriteProviderId);
    Q_INVOKABLE void loadFollowing();
    Q_INVOKABLE void openChannel(QString channelId);
    Q_INVOKABLE void loadMembers();
    Q_INVOKABLE void loadInvites();
    Q_INVOKABLE void refresh();

    // Every call is initiated by a separate, explicit user confirmation in QML.
    // All writes are denied unless the injected managed signer is approved
    // for the exact current viewer and the chosen protocol is Neynar.
    Q_INVOKABLE void followChannel(bool follow);
    Q_INVOKABLE void inviteMember(qulonglong targetFid);
    Q_INVOKABLE void removeMember(qulonglong targetFid);
    Q_INVOKABLE void respondToInvite(QString channelId, bool accept);

signals:
    void changed();
    void actionCompleted(bool success, QString message);

private:
    using JsonCallback = std::function<void(const QJsonObject&, const QString&)>;
    void get(QString path, QVariantMap query, quint64 generation, JsonCallback callback);
    void submit(QString action, QString path, QJsonObject body);
    void verifyManagedSigner(quint64 generation);
    void loadFollowingPage(QString cursor, int page, quint64 generation);
    void request(QString method, QString path, QVariantMap query, QJsonObject body,
                 quint64 generation, JsonCallback callback, bool managedWrite = false);
    void setMessage(QString value);
    void setBusy(bool value);
    void beginRead();
    void finishRead();
    void updateRole(const QJsonObject& raw);
    [[nodiscard]] bool validateMutation(const QString& action, qulonglong targetFid) const;
    [[nodiscard]] bool validChannelId(const QString& value) const;
    [[nodiscard]] static QVariantMap projectChannel(const QJsonObject& raw);
    [[nodiscard]] static QVariantMap projectUser(const QJsonObject& raw);
    [[nodiscard]] static QVariantList projectChannels(const QJsonArray& raw);
    [[nodiscard]] static QVariantList projectMembers(const QJsonObject& raw);
    [[nodiscard]] static QVariantList projectInvites(const QJsonObject& raw);

    QNetworkAccessManager m_network{};
    QString m_neynarApiKey{};
    QString m_neynarSignerUuid{};
    QString m_contract{};
    QString m_endpoint{};
    QString m_writeProviderId{};
    QString m_channelId{};
    QString m_message{};
    QVariantMap m_channel{};
    QVariantList m_following{};
    QVariantList m_members{};
    QVariantList m_invites{};
    qulonglong m_viewerFid{0};
    quint64 m_generation{0};
    quint64 m_channelGeneration{0};
    quint64 m_followingGeneration{0};
    int m_activeReads{0};
    bool m_networkEnabled{false};
    bool m_busy{false};
    bool m_actionBusy{false};
    bool m_signerApproved{false};
    bool m_canModerate{false};
    bool m_roleVerified{false};
};
