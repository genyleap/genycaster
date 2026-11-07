#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QByteArray>
#include <QDebug>

#include <array>
#include <utility>

import genycaster.ui.bootstrap.providers;

namespace {
struct CleanEnvironment final {
    struct Entry { const char* name; QByteArray previous; bool existed; };
    std::array<Entry, 4> entries{{
        {"GENYCASTER_NETWORK_PROXY", {}, false},
        {"GENYCASTER_HYPERSNAP_BASE_URL", {}, false},
        {"GENYCASTER_FARCASTER_READ_BASE_URL", {}, false},
        {"GENYCASTER_FARCASTER_READ_API_KEY", {}, false},
    }};

    CleanEnvironment()
    {
        for (auto& entry : entries) {
            entry.existed = qEnvironmentVariableIsSet(entry.name);
            entry.previous = qgetenv(entry.name);
            qunsetenv(entry.name);
        }
    }
    ~CleanEnvironment()
    {
        for (const auto& entry : entries) {
            if (entry.existed) qputenv(entry.name, entry.previous);
            else qunsetenv(entry.name);
        }
    }
};

bool verify(const bool ok, const char* label)
{
    if (!ok) qCritical().noquote() << "Provider endpoint contract:" << label;
    return ok;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    CleanEnvironment env;
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;
    QSettings settings(directory.filePath(QStringLiteral("policy.ini")), QSettings::IniFormat);
    bool ok = true;

    {
        genycaster::ui::bootstrap::ProviderEndpoints config(settings, {});
        ok &= verify(config.valid(), "default config is valid");
        ok &= verify(!config.proxyDirect(), "system proxy default");
        ok &= verify(config.hypersnapBaseUrl().isEmpty(), "default HyperSnap override absent");
        ok &= verify(config.farcasterReadBaseUrl().isEmpty(), "hosted read opt-in");
        ok &= verify(config.farcasterReadApiKey().isEmpty(), "hosted credentials not persisted");
    }
    settings.setValue(QStringLiteral("network/proxyMode"), QStringLiteral("direct"));
    settings.setValue(QStringLiteral("network/hypersnapBaseUrl"),
                      QStringLiteral("https://stored.example"));
    {
        genycaster::ui::bootstrap::ProviderEndpoints config(settings,
            {QStringLiteral("--hypersnap-base-url=https://selected.example")});
        ok &= verify(config.valid(), "CLI endpoint accepted");
        ok &= verify(config.proxyDirect(), "persisted direct proxy");
        ok &= verify(config.hypersnapBaseUrl() ==
                     QStringLiteral("https://selected.example"), "CLI overrides persisted endpoint");
    }
    {
        genycaster::ui::bootstrap::ProviderEndpoints config(settings,
            {QStringLiteral("--network-proxy=invalid")});
        ok &= verify(!config.valid(), "invalid proxy rejected");
    }
    {
        genycaster::ui::bootstrap::ProviderEndpoints config(settings,
            {QStringLiteral("--hypersnap-base-url=file:///etc/passwd")});
        ok &= verify(config.valid() && config.hypersnapBaseUrl().isEmpty(),
                     "unsafe HyperSnap override discarded");
    }
    qputenv("GENYCASTER_FARCASTER_READ_API_KEY", "test-credential-never-log");
    {
        genycaster::ui::bootstrap::ProviderEndpoints config(settings, {});
        ok &= verify(config.valid(), "default canonical Neynar host");
        ok &= verify(config.farcasterReadBaseUrl() == QStringLiteral("https://api.neynar.com"),
                     "credential auto-selects canonical host");
    }
    {
        genycaster::ui::bootstrap::ProviderEndpoints config(settings,
            {QStringLiteral("--farcaster-read-base-url=https://untrusted.example")});
        ok &= verify(!config.valid(), "credential refused at custom host");
    }
    return ok ? 0 : 1;
}
