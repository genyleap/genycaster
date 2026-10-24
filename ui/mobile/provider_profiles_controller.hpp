#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QSettings;

/**
 * @brief Manages non-secret, explicitly typed read/write provider instances.
 *
 * Provider profiles are persisted in QSettings using the exact JSON schema
 * understood by the native composition root. Unknown API contracts and unsafe
 * URL shapes are rejected. Changes take effect after application restart;
 * this controller does not mutate the active runtime or move pending writes.
 *
 * All calls must occur on the Qt GUI thread. QSettings is borrowed and must
 * outlive this controller. Secrets are never accepted or persisted here.
 */
class ProviderProfilesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList readProfiles READ readProfiles NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList writeProfiles READ writeProfiles NOTIFY profilesChanged)
    Q_PROPERTY(QString selectedReadProviderId READ selectedReadProviderId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedWriteProviderId READ selectedWriteProviderId NOTIFY selectionChanged)
    Q_PROPERTY(bool readRestartRequired READ readRestartRequired NOTIFY selectionChanged)
    Q_PROPERTY(bool writeRestartRequired READ writeRestartRequired NOTIFY selectionChanged)
    Q_PROPERTY(bool indexedReadSupported READ indexedReadSupported CONSTANT)
    Q_PROPERTY(bool rawReadSupported READ rawReadSupported CONSTANT)
    Q_PROPERTY(bool writeSupported READ writeSupported CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
    ProviderProfilesController(
        QSettings& settings,
        QString appliedReadProfiles,
        QString appliedWriteProfiles,
        QString appliedReadProviderId,
        QString appliedWriteProviderId,
        QObject* parent = nullptr);

    [[nodiscard]] QVariantList readProfiles() const;
    [[nodiscard]] QVariantList writeProfiles() const;
    [[nodiscard]] QString selectedReadProviderId() const;
    [[nodiscard]] QString selectedWriteProviderId() const;
    [[nodiscard]] bool readRestartRequired() const;
    [[nodiscard]] bool writeRestartRequired() const;
    [[nodiscard]] bool indexedReadSupported() const noexcept;
    [[nodiscard]] bool rawReadSupported() const noexcept;
    [[nodiscard]] bool writeSupported() const noexcept;
    [[nodiscard]] QString lastError() const;

    /** @brief Notifies QML when another settings panel changes the saved selection. */
    Q_INVOKABLE void refresh();

    /** @brief Registers an explicitly compatible, credential-free indexed or raw read node. */
    Q_INVOKABLE bool addReadProfile(const QString& id, const QString& label,
                                    const QString& contract, const QString& endpoint,
                                    bool selfHosted);
    /** @brief Registers a validator with an optional distinct read/reconciliation endpoint. */
    Q_INVOKABLE bool addWriteProfile(const QString& id, const QString& label,
                                     const QString& contract, const QString& endpoint,
                                     const QString& readEndpoint, bool selfHosted);
    /** @brief Removes one read node, also clearing its persisted preference if selected. */
    Q_INVOKABLE bool removeReadProfile(const QString& id);
    /** @brief Removes one write node, without modifying queued write route bindings. */
    Q_INVOKABLE bool removeWriteProfile(const QString& id);
    /** @brief Selects a registered or built-in compatible reader; empty means automatic. */
    Q_INVOKABLE bool selectReadProvider(const QString& id);
    /** @brief Selects a registered or built-in compatible writer; empty means default. */
    Q_INVOKABLE bool selectWriteProvider(const QString& id);

signals:
    void profilesChanged();
    void selectionChanged();
    void errorChanged();

private:
    bool addProfile(const QString& key, bool writer, const QString& id,
                    const QString& label, const QString& contract,
                    const QString& endpoint, const QString& readEndpoint,
                    bool selfHosted);
    bool removeProfile(const QString& key, bool writer, const QString& id);
    bool save(const QString& key, const QString& serialized);
    bool fail(const QString& message);
    void clearError();

    QSettings& m_settings;
    QString m_appliedReadProfiles;
    QString m_appliedWriteProfiles;
    QString m_appliedReadId;
    QString m_appliedWriteId;
    QString m_lastError;
};
