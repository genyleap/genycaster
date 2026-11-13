#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QUrl>

/**
 * Read-only HyperSnap native registration readiness.
 * Never interprets an HTTP 200 as permission to create a wallet or assign an
 * FID. Those actions require a separate audited custody/signer onboarding.
 */
class NativeRegistrationStatusController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int difficultyBits READ difficultyBits NOTIFY changed)
    Q_PROPERTY(QString firstNativeFid READ firstNativeFid NOTIFY changed)
public:
    explicit NativeRegistrationStatusController(QString baseUrl, QObject* parent = nullptr);
    [[nodiscard]] bool active() const noexcept { return m_active; }
    [[nodiscard]] bool checking() const noexcept { return m_checking; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int difficultyBits() const noexcept { return m_bits; }
    [[nodiscard]] QString firstNativeFid() const { return m_firstFid; }
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    QNetworkAccessManager m_network{};
    QUrl m_base{};
    QPointer<QNetworkReply> m_reply{};
    bool m_active{false};
    bool m_checking{false};
    int m_bits{0};
    QString m_firstFid{};
    QString m_error{};
};
