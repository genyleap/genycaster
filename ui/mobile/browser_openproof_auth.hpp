#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <cstdint>
#include <functional>

// Browser-only OpenProof device authorization. Verifies the bearer session
// against OpenProof userinfo AND its sole linked Farcaster FID. No JS secrets,
// local credential persistence or protocol-write authorization.
struct BrowserOpenProofState final {
    enum class Phase { Starting, AwaitingApproval, Authenticated, Failed, SignedOut };
    Phase phase{Phase::SignedOut};
    QString approvalUrl{};
    QString error{};
    QString subject{};
    std::uint64_t fid{};
    QString username{};
    QString displayName{};
    QString avatarUrl{};
    int tokenExpiresSeconds{3600};
};

class BrowserOpenProofAuth final : public QObject {
public:
    using Observer = std::function<void(BrowserOpenProofState)>;
    // C++-only transient account capability, delivered after userinfo and
    // Farcaster-FID verification. Never exposed through QML/JS or persisted.
    using ConnectionGrant = std::function<void(QByteArray, QString, std::uint64_t, int)>;

    explicit BrowserOpenProofAuth(QObject* parent = nullptr);
    ~BrowserOpenProofAuth() override;

    void begin(Observer observer, ConnectionGrant connectionGrant = {});
    void beginFarcaster(Observer observer, ConnectionGrant connectionGrant = {});
    void cancel();
    void signOut();

private:
    using Response = std::function<void(int, QByteArray)>;

    void send(QNetworkRequest request, QByteArray body, Response response);
    void startPolling();
    void pollToken();
    void pollFarcaster();
    void checkUserinfo();
    void checkConnections();
    void fail(QString message);
    void emitState(BrowserOpenProofState state);
    void reset();

    QNetworkAccessManager m_network{this};
    QTimer m_pollTimer{this};
    QTimer m_sessionExpiryTimer{this};
    QPointer<QNetworkReply> m_reply{};
    Observer m_observer{};
    ConnectionGrant m_connectionGrant{};
    QByteArray m_deviceCode{};
    QByteArray m_farcasterTicket{};
    bool m_directFarcaster{false};
    QByteArray m_accessToken{};
    QString m_subject{};
    QString m_approvalUrl{};
    int m_intervalSeconds{5};
    int m_tokenExpiresSeconds{3600};
    qint64 m_deadlineMs{};
    std::uint64_t m_generation{};
};
