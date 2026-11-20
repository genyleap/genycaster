// Endpoint precedence and credential checks are isolated from UI startup.
module;

#include <QDebug>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>
#include <utility>

module genycaster.ui.bootstrap.providers;

import genycaster.ui.bootstrap;

namespace genycaster::ui::bootstrap {

struct ProviderEndpoints::Impl {
    QString proxyMode{};
    QString hypersnapBaseUrl{};
    QString readBaseUrl{};
    QString readApiKey{};
    bool isValid{true};
    bool isDirect{false};

    void load(QSettings& settings, const QStringList& arguments)
    {
        proxyMode = optionValue(arguments, QStringLiteral("--network-proxy="));
        if (proxyMode.isEmpty())
            proxyMode = qEnvironmentVariable("GENYCASTER_NETWORK_PROXY").trimmed();
        if (proxyMode.isEmpty())
            proxyMode = settings.value(
                QStringLiteral("network/proxyMode"), QStringLiteral("system")).toString();
        proxyMode = proxyMode.toLower();
        if (proxyMode == QStringLiteral("direct")) {
            isDirect = true;
        } else if (proxyMode != QStringLiteral("system")) {
            qCritical().noquote()
                << "GENYCASTER_NETWORK_PROXY/--network-proxy must be 'system' or 'direct'.";
            isValid = false;
            return;
        }

        hypersnapBaseUrl = optionValue(arguments, QStringLiteral("--hypersnap-base-url="));
        if (hypersnapBaseUrl.isEmpty())
            hypersnapBaseUrl = qEnvironmentVariable("GENYCASTER_HYPERSNAP_BASE_URL").trimmed();
        if (hypersnapBaseUrl.isEmpty())
            hypersnapBaseUrl = settings.value(
                QStringLiteral("network/hypersnapBaseUrl")).toString().trimmed();
        hypersnapBaseUrl = normalizeConfigurationToken(std::move(hypersnapBaseUrl));
        if (!hypersnapBaseUrl.isEmpty() && !isValidHttpServiceUrl(hypersnapBaseUrl)) {
            qWarning().noquote()
                << "Ignoring invalid Hypersnap base URL override; using the built-in public read source.";
            hypersnapBaseUrl.clear();
        }

        // An indexed Farcaster provider is optional. Never load a credential
        // from persisted QSettings, or send it to an unapproved API origin.
        readBaseUrl = optionValue(arguments, QStringLiteral("--farcaster-read-base-url="));
        if (readBaseUrl.isEmpty())
            readBaseUrl = qEnvironmentVariable("GENYCASTER_FARCASTER_READ_BASE_URL").trimmed();
        readApiKey = qEnvironmentVariable("GENYCASTER_FARCASTER_READ_API_KEY").trimmed();
        if (readBaseUrl.isEmpty()) {
            // The public index at the built-in read origin exposes a
            // Neynar-compatible /v2/farcaster/user for Farcaster FIDs with
            // aggregate social counts. Register it as a separate Farcaster
            // provider: never reuse the HyperSnap identity namespace.
            // Explicit Neynar credentials still select their canonical host.
            readBaseUrl = readApiKey.isEmpty()
                ? QStringLiteral("https://haatz.quilibrium.com")
                : QStringLiteral("https://api.neynar.com");
        }
        readBaseUrl = normalizeConfigurationToken(std::move(readBaseUrl));
        if (!readBaseUrl.isEmpty() && !isValidHttpServiceUrl(readBaseUrl)) {
            qWarning().noquote()
                << "Farcaster read endpoint is invalid; hosted provider will not be registered.";
            readBaseUrl.clear();
        }
        if (readApiKey.size() > 256
            || readApiKey.contains(QLatin1Char('\r'))
            || readApiKey.contains(QLatin1Char('\n'))) {
            qCritical().noquote() << "Invalid Farcaster read credential format.";
            isValid = false;
            return;
        }
        if (!readApiKey.isEmpty()
            && QUrl(readBaseUrl).host().compare(
                QStringLiteral("api.neynar.com"), Qt::CaseInsensitive) != 0) {
            qCritical().noquote()
                << "Neynar API key may only be sent to the canonical api.neynar.com host.";
            isValid = false;
        }
    }
};

ProviderEndpoints::ProviderEndpoints(QSettings& settings, const QStringList& arguments)
    : m_impl{std::make_unique<Impl>()}
{
    m_impl->load(settings, arguments);
}

ProviderEndpoints::~ProviderEndpoints() = default;
bool ProviderEndpoints::valid() const noexcept { return m_impl->isValid; }
bool ProviderEndpoints::proxyDirect() const noexcept { return m_impl->isDirect; }
const QString& ProviderEndpoints::proxyMode() const noexcept { return m_impl->proxyMode; }
const QString& ProviderEndpoints::hypersnapBaseUrl() const noexcept
{
    return m_impl->hypersnapBaseUrl;
}
const QString& ProviderEndpoints::farcasterReadBaseUrl() const noexcept
{
    return m_impl->readBaseUrl;
}
const QString& ProviderEndpoints::farcasterReadApiKey() const noexcept
{
    return m_impl->readApiKey;
}
} // namespace genycaster::ui::bootstrap
