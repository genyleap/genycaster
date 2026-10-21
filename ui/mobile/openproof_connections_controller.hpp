#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

/** @brief Qt presentation adapter for public OpenProof capabilities and
 * first-party connection management. Never reads or stores account tokens,
 * connection claims or identity-provider credentials. */
class OpenProofConnectionsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList providers READ providers NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString identityIssuer READ identityIssuer CONSTANT)
public:
    explicit OpenProofConnectionsController(QObject* parent = nullptr);

    [[nodiscard]] QStringList providers() const;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QString identityIssuer() const;

    /** @brief Refreshes public provider capabilities over bounded HTTPS.
     * Network or CORS errors preserve the last known display-only list. */
    Q_INVOKABLE void refresh();
    /** @brief Opens the official browser account manager for a known provider.
     * This method does not itself link accounts or assert connected status. */
    Q_INVOKABLE bool openManager(const QString& provider);

    /** @brief Decodes a public provider catalog, excluding local login factors.
     * Invalid, oversized and incomplete payloads yield an empty list. */
    [[nodiscard]] static QStringList parseProviderCatalog(const QByteArray& json);

signals:
    void stateChanged();

private:
    QNetworkAccessManager m_network{this};
    QUrl m_catalogUrl{};
    QUrl m_issuerUrl{};
    QUrl m_accountUrl{};
    QStringList m_providers{};
    QString m_error{};
    bool m_loading{};
};
