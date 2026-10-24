#include "provider_profiles_controller.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace {

constexpr auto kReadProfiles = "network/readProviderProfilesJson";
constexpr auto kWriteProfiles = "network/writeProviderProfilesJson";
constexpr auto kReadSelection = "network/preferredReadProviderId";
constexpr auto kWriteSelection = "network/preferredWriteProviderId";
constexpr int kMaximumProfiles = 16;

[[nodiscard]] QString settingKey(const char* key)
{
    return QString::fromLatin1(key);
}

[[nodiscard]] QJsonArray parseProfiles(const QString& input)
{
    if (input.trimmed().isEmpty()) return {};
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(input.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return {};
    return document.array();
}

[[nodiscard]] QString canonicalProfiles(const QString& input)
{
    return QString::fromUtf8(QJsonDocument{parseProfiles(input)}.toJson(
        QJsonDocument::Compact));
}

[[nodiscard]] bool validId(const QString& id)
{
    if (id.isEmpty() || id.size() > 64) return false;
    for (const QChar ch : id) {
        const ushort code = ch.unicode();
        if (!((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
              || (code >= '0' && code <= '9') || code == '-' || code == '_'
              || code == '.')) return false;
    }
    return true;
}

[[nodiscard]] bool reservedId(const QString& id)
{
    return id == QStringLiteral("hypersnap-v2")
        || id == QStringLiteral("farcaster-snapchain")
        || id == QStringLiteral("primary-protocol-writer");
}

[[nodiscard]] QString normalizedOrigin(QString input)
{
    input = input.trimmed();
    if (input.size() > 2048 || input.contains(QLatin1Char('\n'))
        || input.contains(QLatin1Char('\r')) || input.contains(QLatin1Char(' '))
        || input.contains(QLatin1Char('\\'))) return {};
    const QUrl url{input, QUrl::StrictMode};
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()
        || url.hasQuery() || url.hasFragment()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http")))
        return {};
    if (url.port(-1) == 0) return {};
    while (input.endsWith(QLatin1Char('/'))) input.chop(1);
    return input;
}

[[nodiscard]] bool knownContract(const QString& name, const bool writer)
{
    if (writer) {
#if FARCASTER_HAS_SNAPCHAIN_WRITE
        return name == QStringLiteral("snapchain-v1");
#else
        Q_UNUSED(name);
        return false;
#endif
    }
#if FARCASTER_HAS_SNAPCHAIN_READ
    if (name == QStringLiteral("snapchain-v1")) return true;
#endif
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    if (name == QStringLiteral("hypersnap-v2")) return true;
#endif
    return false;
}

[[nodiscard]] bool exists(const QJsonArray& profiles, const QString& id)
{
    for (const auto& item : profiles) {
        if (item.isObject()
            && item.toObject().value(QStringLiteral("id")).toString() == id)
            return true;
    }
    return false;
}

[[nodiscard]] QVariantList asVariantList(const QJsonArray& profiles)
{
    QVariantList result{};
    result.reserve(profiles.size());
    for (const auto& item : profiles) {
        if (item.isObject()) result.push_back(item.toObject().toVariantMap());
    }
    return result;
}

} // namespace

ProviderProfilesController::ProviderProfilesController(
    QSettings& settings,
    QString appliedReadProfiles,
    QString appliedWriteProfiles,
    QString appliedReadProviderId,
    QString appliedWriteProviderId,
    QObject* parent)
    : QObject(parent),
      m_settings(settings),
      m_appliedReadProfiles(canonicalProfiles(appliedReadProfiles)),
      m_appliedWriteProfiles(canonicalProfiles(appliedWriteProfiles)),
      m_appliedReadId(std::move(appliedReadProviderId)),
      m_appliedWriteId(std::move(appliedWriteProviderId))
{
}

QVariantList ProviderProfilesController::readProfiles() const
{
    return asVariantList(parseProfiles(m_settings.value(settingKey(kReadProfiles)).toString()));
}

QVariantList ProviderProfilesController::writeProfiles() const
{
    return asVariantList(parseProfiles(m_settings.value(settingKey(kWriteProfiles)).toString()));
}

QString ProviderProfilesController::selectedReadProviderId() const
{
    return m_settings.value(settingKey(kReadSelection)).toString().trimmed();
}

QString ProviderProfilesController::selectedWriteProviderId() const
{
    return m_settings.value(settingKey(kWriteSelection)).toString().trimmed();
}

bool ProviderProfilesController::readRestartRequired() const
{
    return canonicalProfiles(m_settings.value(settingKey(kReadProfiles)).toString())
                != m_appliedReadProfiles
        || selectedReadProviderId() != m_appliedReadId;
}

bool ProviderProfilesController::writeRestartRequired() const
{
    return canonicalProfiles(m_settings.value(settingKey(kWriteProfiles)).toString())
                != m_appliedWriteProfiles
        || selectedWriteProviderId() != m_appliedWriteId;
}

bool ProviderProfilesController::indexedReadSupported() const noexcept
{
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    return true;
#else
    return false;
#endif
}

bool ProviderProfilesController::rawReadSupported() const noexcept
{
#if FARCASTER_HAS_SNAPCHAIN_READ
    return true;
#else
    return false;
#endif
}

bool ProviderProfilesController::writeSupported() const noexcept
{
#if FARCASTER_HAS_SNAPCHAIN_WRITE
    return true;
#else
    return false;
#endif
}

QString ProviderProfilesController::lastError() const
{
    return m_lastError;
}

void ProviderProfilesController::refresh()
{
    emit profilesChanged();
    emit selectionChanged();
}

bool ProviderProfilesController::fail(const QString& message)
{
    if (m_lastError != message) {
        m_lastError = message;
        emit errorChanged();
    }
    return false;
}

void ProviderProfilesController::clearError()
{
    if (m_lastError.isEmpty()) return;
    m_lastError.clear();
    emit errorChanged();
}

bool ProviderProfilesController::save(const QString& key, const QString& serialized)
{
    const bool existed = m_settings.contains(key);
    const QVariant previous = m_settings.value(key);
    m_settings.setValue(key, serialized);
    m_settings.sync();
    if (m_settings.status() == QSettings::NoError) return true;
    if (existed) m_settings.setValue(key, previous);
    else m_settings.remove(key);
    m_settings.sync();
    return fail(QStringLiteral("storage"));
}

bool ProviderProfilesController::addProfile(
    const QString& key, const bool writer, const QString& id,
    const QString& label, const QString& contract, const QString& endpoint,
    const QString& readEndpoint, const bool selfHosted)
{
    const QString identity = id.trimmed();
    const QString contractId = contract.trimmed();
    const QString displayName = label.trimmed();
    if (!validId(identity) || reservedId(identity))
        return fail(QStringLiteral("invalid_id"));
    if (displayName.isEmpty() || displayName.size() > 96)
        return fail(QStringLiteral("invalid_label"));
    if (!knownContract(contractId, writer))
        return fail(QStringLiteral("unsupported_contract"));
    const QString origin = normalizedOrigin(endpoint);
    const QString reader = readEndpoint.trimmed().isEmpty()
        ? QString{} : normalizedOrigin(readEndpoint);
    if (origin.isEmpty() || (!readEndpoint.trimmed().isEmpty() && reader.isEmpty()))
        return fail(QStringLiteral("invalid_endpoint"));
    auto profiles = parseProfiles(m_settings.value(key).toString());
    if (profiles.size() >= kMaximumProfiles)
        return fail(QStringLiteral("limit_reached"));
    const QString other = writer ? settingKey(kReadProfiles) : settingKey(kWriteProfiles);
    if (exists(profiles, identity)
        || exists(parseProfiles(m_settings.value(other).toString()), identity))
        return fail(QStringLiteral("duplicate_id"));
    QJsonObject profile{
        {QStringLiteral("id"), identity},
        {QStringLiteral("label"), displayName},
        {QStringLiteral("contract"), contractId},
        {QStringLiteral("endpoint"), origin},
        {QStringLiteral("selfHosted"), selfHosted},
    };
    if (writer && !reader.isEmpty())
        profile.insert(QStringLiteral("readEndpoint"), reader);
    profiles.push_back(profile);
    if (!save(key, QString::fromUtf8(QJsonDocument{profiles}.toJson(QJsonDocument::Compact))))
        return false;
    clearError();
    emit profilesChanged();
    emit selectionChanged();
    return true;
}

bool ProviderProfilesController::addReadProfile(
    const QString& id, const QString& label, const QString& contract,
    const QString& endpoint, const bool selfHosted)
{
    return addProfile(settingKey(kReadProfiles), false,
                      id, label, contract, endpoint, {}, selfHosted);
}

bool ProviderProfilesController::addWriteProfile(
    const QString& id, const QString& label, const QString& contract,
    const QString& endpoint, const QString& readEndpoint, const bool selfHosted)
{
    return addProfile(settingKey(kWriteProfiles), true,
                      id, label, contract, endpoint, readEndpoint, selfHosted);
}

bool ProviderProfilesController::removeProfile(
    const QString& key, const bool writer, const QString& id)
{
    QJsonArray profiles = parseProfiles(m_settings.value(key).toString());
    int position{-1};
    for (int index = 0; index < profiles.size(); ++index) {
        if (profiles.at(index).isObject()
            && profiles.at(index).toObject().value(QStringLiteral("id")).toString() == id) {
            position = index;
            break;
        }
    }
    if (position < 0) return fail(QStringLiteral("unknown_provider"));
    profiles.removeAt(position);
    const QString selectionKey = writer
        ? settingKey(kWriteSelection) : settingKey(kReadSelection);
    const bool hadSelection = m_settings.contains(selectionKey);
    const QVariant previousSelection = m_settings.value(selectionKey);
    const bool hadProfiles = m_settings.contains(key);
    const QVariant previousProfiles = m_settings.value(key);
    // Update profile list and selected provider in one QSettings sync so a
    // removed provider cannot be left selected after process termination.
    m_settings.setValue(
        key, QString::fromUtf8(QJsonDocument{profiles}.toJson(QJsonDocument::Compact)));
    if (previousSelection.toString() == id)
        m_settings.remove(selectionKey);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        if (hadProfiles) m_settings.setValue(key, previousProfiles);
        else m_settings.remove(key);
        if (hadSelection) m_settings.setValue(selectionKey, previousSelection);
        else m_settings.remove(selectionKey);
        m_settings.sync();
        return fail(QStringLiteral("storage"));
    }
    clearError();
    emit profilesChanged();
    emit selectionChanged();
    return true;
}

bool ProviderProfilesController::removeReadProfile(const QString& id)
{
    return removeProfile(settingKey(kReadProfiles), false, id);
}

bool ProviderProfilesController::removeWriteProfile(const QString& id)
{
    return removeProfile(settingKey(kWriteProfiles), true, id);
}

bool ProviderProfilesController::selectReadProvider(const QString& id)
{
    const QString selected = id.trimmed();
    if (!selected.isEmpty() && selected != QStringLiteral("hypersnap-v2")
        && selected != QStringLiteral("farcaster-snapchain")
        && !exists(parseProfiles(m_settings.value(settingKey(kReadProfiles)).toString()), selected))
        return fail(QStringLiteral("unknown_provider"));
    if (selected == QStringLiteral("hypersnap-v2") && !indexedReadSupported())
        return fail(QStringLiteral("unsupported_contract"));
    // A custom/hosted provider ID can be persisted only when actually registered.
    if (selected == QStringLiteral("farcaster-snapchain"))
        return fail(QStringLiteral("unknown_provider"));
    if (selectedReadProviderId() == selected) {
        clearError();
        return true;
    }
    if (selected.isEmpty()) {
        m_settings.remove(settingKey(kReadSelection));
        m_settings.sync();
        if (m_settings.status() != QSettings::NoError)
            return fail(QStringLiteral("storage"));
    } else if (!save(settingKey(kReadSelection), selected)) {
        return false;
    }
    clearError();
    emit selectionChanged();
    return true;
}

bool ProviderProfilesController::selectWriteProvider(const QString& id)
{
    const QString selected = id.trimmed();
    if (!selected.isEmpty() && selected != QStringLiteral("primary-protocol-writer")
        && !exists(parseProfiles(m_settings.value(settingKey(kWriteProfiles)).toString()), selected))
        return fail(QStringLiteral("unknown_provider"));
    if (!selected.isEmpty() && !writeSupported())
        return fail(QStringLiteral("unsupported_contract"));
    if (selectedWriteProviderId() == selected) {
        clearError();
        return true;
    }
    if (selected.isEmpty()) {
        m_settings.remove(settingKey(kWriteSelection));
        m_settings.sync();
        if (m_settings.status() != QSettings::NoError)
            return fail(QStringLiteral("storage"));
    } else if (!save(settingKey(kWriteSelection), selected)) {
        return false;
    }
    clearError();
    emit selectionChanged();
    return true;
}
