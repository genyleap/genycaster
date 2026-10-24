#include "provider_profiles_controller.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>
#endif

namespace {

constexpr auto kReadProfiles = "network/readProviderProfilesJson";
constexpr auto kWriteProfiles = "network/writeProviderProfilesJson";
constexpr auto kReadSelection = "network/preferredReadProviderId";
constexpr auto kReadFallback = "network/allowCompatibleReadFallback";
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
    const bool appliedCompatibleReadFallback,
    QObject* parent,
    QString defaultIndexedEndpoint)
    : QObject(parent),
      m_settings(settings),
      m_appliedReadProfiles(canonicalProfiles(appliedReadProfiles)),
      m_appliedWriteProfiles(canonicalProfiles(appliedWriteProfiles)),
      m_appliedReadId(std::move(appliedReadProviderId)),
      m_defaultIndexedEndpoint(std::move(defaultIndexedEndpoint)),
      m_appliedCompatibleReadFallback(appliedCompatibleReadFallback),
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

bool ProviderProfilesController::compatibleReadFallback() const
{
    return m_settings.value(settingKey(kReadFallback), false).toBool();
}

QString ProviderProfilesController::selectedWriteProviderId() const
{
    return m_settings.value(settingKey(kWriteSelection)).toString().trimmed();
}

bool ProviderProfilesController::readRestartRequired() const
{
    return canonicalProfiles(m_settings.value(settingKey(kReadProfiles)).toString())
                != m_appliedReadProfiles
        || selectedReadProviderId() != m_appliedReadId
        || compatibleReadFallback() != m_appliedCompatibleReadFallback
        || m_sessionKeyChanged;
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
#ifdef __EMSCRIPTEN__
    // Removing a profile also clears its per-tab credential so a later
    // profile cannot accidentally inherit an old provider's secret.
    if (!writer) {
        const QByteArray tokenId = id.toUtf8();
        EM_ASM({
            try {
                const id = UTF8ToString($0);
                const name = "genycaster:read-session-keys";
                const values = JSON.parse(sessionStorage.getItem(name) || "{}");
                delete values[id];
                sessionStorage.setItem(name, JSON.stringify(values));
            } catch (_) {}
        }, tokenId.constData());
    }
#endif
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

bool ProviderProfilesController::setCompatibleReadFallback(const bool enabled)
{
    if (compatibleReadFallback() == enabled) {
        clearError();
        return true;
    }
    if (!save(settingKey(kReadFallback), enabled ? QStringLiteral("true")
                                              : QStringLiteral("false"))) return false;
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


bool ProviderProfilesController::probeReadProvider(const QString& id)
{
    if (id.isEmpty() || id == QStringLiteral("hypersnap-v2"))
        return startProbe(QStringLiteral("hypersnap-v2"), QStringLiteral("hypersnap-v2"),
                          m_defaultIndexedEndpoint);
    for (const auto& entry : parseProfiles(
             m_settings.value(settingKey(kReadProfiles)).toString())) {
        const auto profile = entry.toObject();
        if (profile.value(QStringLiteral("id")).toString() != id) continue;
        return startProbe(id, profile.value(QStringLiteral("contract")).toString(),
                          profile.value(QStringLiteral("endpoint")).toString());
    }
    return fail(QStringLiteral("unknown_provider"));
}

bool ProviderProfilesController::probeDraftNode(
    const QString& contract, const QString& endpoint, const QString& apiKey)
{
    return startProbe(QStringLiteral("draft"), contract, endpoint, apiKey);
}

bool ProviderProfilesController::startProbe(
    const QString& id, const QString& contract, const QString& endpoint,
    const QString& draftApiKey)
{
    // A ping to / is meaningless: a host can return a landing page and not
    // implement our selected adapter. Probe a real read endpoint and verify
    // its JSON shape, with the same browser CORS restrictions as normal reads.
    const QString origin = normalizedOrigin(endpoint);
    if (origin.isEmpty() || (contract != QStringLiteral("snapchain-v1")
                             && contract != QStringLiteral("hypersnap-v2"))) {
        if (m_probeReply) {
            auto* oldReply = m_probeReply.data();
            m_probeReply = nullptr;
            oldReply->abort();
            oldReply->deleteLater();
        }
        m_probeProviderId = id;
        m_probeState = QStringLiteral("invalid");
        m_probeMessage = QStringLiteral("invalid_address");
        m_probeLatencyMs = -1;
        emit probeChanged();
        return false;
    }
    if (m_probeReply) {
        m_probeReply->abort();
        m_probeReply->deleteLater();
        m_probeReply = nullptr;
    }
    m_probeProviderId = id;
    m_probeState = QStringLiteral("checking");
    m_probeMessage.clear();
    m_probeLatencyMs = -1;
    m_probeHttpStatus = 0;
    emit probeChanged();

    const bool raw = contract == QStringLiteral("snapchain-v1");
    const QString path = raw
        ? QStringLiteral("/v1/info")
        : QStringLiteral("/v2/farcaster/user?fid=3");
    QNetworkRequest request{QUrl(origin + path)};
    request.setRawHeader("Accept", "application/json");
    const QByteArray providedKey = draftApiKey.toUtf8();
    if (id == QStringLiteral("draft") && !providedKey.isEmpty()) {
        if (!origin.startsWith(QStringLiteral("https://"))
            || providedKey.size() > 256
            || std::any_of(providedKey.cbegin(), providedKey.cend(),
                 [](const char value) {
                     const auto byte = static_cast<unsigned char>(value);
                     return byte < 33U || byte > 126U;
                 })) {
            m_probeState = QStringLiteral("invalid");
            m_probeMessage = QStringLiteral("invalid_api_key");
            emit probeChanged();
            return false;
        }
        request.setRawHeader("x-api-key", providedKey);
    }
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(8'000);
#ifdef __EMSCRIPTEN__
    if (id != QStringLiteral("draft")) {
        const QByteArray idUtf8 = id.toUtf8();
        // This key is explicitly session-scoped and never put in QSettings.
        const char* key = reinterpret_cast<const char*>(EM_ASM_PTR({
            try {
                const id = UTF8ToString($0);
                const values = JSON.parse(sessionStorage.getItem("genycaster:read-session-keys") || "{}");
                const value = typeof values[id] === "string" ? values[id] : "";
                const size = lengthBytesUTF8(value) + 1;
                const ptr = _malloc(size);
                stringToUTF8(value, ptr, size);
                return ptr;
            } catch (_) { return 0; }
        }, idUtf8.constData()));
        if (key) {
            const QByteArray secret{key};
            std::free(const_cast<char*>(key));
            if (!secret.isEmpty()) request.setRawHeader("x-api-key", secret);
        }
    }
#endif
    m_probeTimer.start();
    QNetworkReply* reply = m_network.get(request);
    m_probeReply = reply;
    QTimer::singleShot(8'500, reply, [reply] {
        if (!reply->isFinished()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, raw] {
        if (m_probeReply != reply) {
            reply->deleteLater();
            return;
        }
        m_probeReply = nullptr;
        m_probeLatencyMs = static_cast<int>(m_probeTimer.elapsed());
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_probeHttpStatus = http;
        const QByteArray body = reply->read(32 * 1024);
        const auto parsed = QJsonDocument::fromJson(body);
        const QJsonObject json = parsed.isObject() ? parsed.object() : QJsonObject{};
        const bool validShape = raw
            ? json.contains(QStringLiteral("version"))
                && json.contains(QStringLiteral("dbStats"))
            : json.value(QStringLiteral("user")).isObject();
        if (http == 401 || http == 403 || http == 402) {
            m_probeState = QStringLiteral("auth_required");
            m_probeMessage = QStringLiteral("api_key_required");
        } else if (reply->error() != QNetworkReply::NoError) {
            m_probeState = QStringLiteral("failed");
            m_probeMessage = QStringLiteral("network_or_cors");
        } else if (http < 200 || http >= 300) {
            m_probeState = QStringLiteral("failed");
            m_probeMessage = QStringLiteral("http_error");
        } else if (!validShape) {
            m_probeState = QStringLiteral("incompatible");
            m_probeMessage = QStringLiteral("unexpected_json");
        } else {
            m_probeState = QStringLiteral("healthy");
            m_probeMessage = QStringLiteral("compatible");
        }
        emit probeChanged();
        reply->deleteLater();
    });
    return true;
}

bool ProviderProfilesController::setSessionReadApiKey(
    const QString& id, const QString& key)
{
    const QByteArray secret = key.toUtf8();
    if (secret.size() > 256 || std::any_of(secret.cbegin(), secret.cend(),
            [](char c) { return static_cast<unsigned char>(c) < 33
                              || static_cast<unsigned char>(c) > 126; })) {
        return fail(QStringLiteral("invalid_api_key"));
    }
    bool eligible = false;
    for (const auto& entry : parseProfiles(
             m_settings.value(settingKey(kReadProfiles)).toString())) {
        const auto item = entry.toObject();
        if (item.value(QStringLiteral("id")).toString() == id
            && (item.value(QStringLiteral("contract")).toString()
                == QStringLiteral("hypersnap-v2")
                || item.value(QStringLiteral("contract")).toString()
                    == QStringLiteral("snapchain-v1"))
            && item.value(QStringLiteral("endpoint")).toString()
                .startsWith(QStringLiteral("https://"))) {
            eligible = true;
        }
    }
    if (!eligible) return fail(QStringLiteral("unknown_provider"));
#ifdef __EMSCRIPTEN__
    const QByteArray name = id.toUtf8();
    const bool stored = EM_ASM_INT({
        try {
            const id = UTF8ToString($0);
            const value = UTF8ToString($1);
            const key = "genycaster:read-session-keys";
            const saved = JSON.parse(sessionStorage.getItem(key) || "{}");
            if (value) saved[id] = value;
            else delete saved[id];
            sessionStorage.setItem(key, JSON.stringify(saved));
            return 1;
        } catch (_) { return 0; }
    }, name.constData(), secret.constData()) != 0;
    if (!stored) return fail(QStringLiteral("storage"));
    m_sessionKeyChanged = true;
    clearError();
    emit selectionChanged();
    return true;
#else
    return fail(QStringLiteral("unsupported_contract"));
#endif
}

void ProviderProfilesController::acceptBrowserPaste(const QString& text)
{
    if (text.isEmpty() || text.size() > 4096) return;
    m_clipboardText = text;
    emit clipboardChanged();
}

#ifdef __EMSCRIPTEN__
namespace {
QPointer<ProviderProfilesController> pendingClipboardController{};
}
extern "C" EMSCRIPTEN_KEEPALIVE void genycasterNodeClipboardResult(const char* utf8)
{
    if (pendingClipboardController && utf8)
        pendingClipboardController->acceptBrowserPaste(QString::fromUtf8(utf8));
}
#endif

void ProviderProfilesController::requestBrowserPaste()
{
#ifdef __EMSCRIPTEN__
    pendingClipboardController = this;
    EM_ASM({
        if (!navigator.clipboard || !navigator.clipboard.readText) return;
        navigator.clipboard.readText().then(function(text) {
            if (!text) return;
            const length = lengthBytesUTF8(text) + 1;
            const ptr = _malloc(length);
            try {
                stringToUTF8(text, ptr, length);
                Module._genycasterNodeClipboardResult(ptr);
            } finally { _free(ptr); }
        }).catch(function() { /* Native paste shortcut is also available. */ });
    });
#else
    if (const auto* clipboard = QGuiApplication::clipboard())
        acceptBrowserPaste(clipboard->text());
#endif
}

QString ProviderProfilesController::activeReadProviderId() const
{
    return m_appliedReadId.isEmpty() ? QStringLiteral("hypersnap-v2") : m_appliedReadId;
}

QString ProviderProfilesController::activeReadEndpoint() const
{
    if (m_appliedReadId.isEmpty() || m_appliedReadId == QStringLiteral("hypersnap-v2"))
        return m_defaultIndexedEndpoint;
    for (const auto& entry : parseProfiles(m_appliedReadProfiles)) {
        const auto profile = entry.toObject();
        if (profile.value(QStringLiteral("id")).toString() == m_appliedReadId)
            return profile.value(QStringLiteral("endpoint")).toString();
    }
    return {};
}

void ProviderProfilesController::clearDraftProbe()
{
    if (m_probeProviderId != QStringLiteral("draft")) return;
    if (m_probeReply) {
        auto* pending = m_probeReply.data();
        m_probeReply = nullptr;
        pending->abort();
        pending->deleteLater();
    }
    m_probeProviderId.clear();
    m_probeState = QStringLiteral("idle");
    m_probeMessage.clear();
    m_probeLatencyMs = -1;
    m_probeHttpStatus = 0;
    emit probeChanged();
}
