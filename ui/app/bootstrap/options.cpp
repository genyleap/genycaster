// Qt is a header-based framework; keep its declarations in the global
// module fragment so its types retain their original linkage.
module;

#include "rendering_controller.hpp"
#include <QSettings>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <string>

module genycaster.ui.bootstrap;

namespace genycaster::ui::bootstrap {

[[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix)
{
    for (const QString& argument : arguments) {
        if (argument.startsWith(prefix)) {
            return argument.sliced(prefix.size()).trimmed();
        }
    }
    return {};
}


[[nodiscard]] QString startupRenderingProfile(const int argc, char* argv[])
{
    QString explicitProfile{};
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        constexpr auto prefix = "--render-profile=";
        if (argument.startsWith(QString::fromLatin1(prefix))) {
            explicitProfile = argument.mid(static_cast<qsizetype>(std::char_traits<char>::length(prefix)));
            break;
        }
    }
    if (explicitProfile.isEmpty()) {
        explicitProfile = qEnvironmentVariable("GENYCASTER_RENDER_PROFILE").trimmed();
    }
    if (!explicitProfile.isEmpty()) {
        const QString canonical = RenderingController::canonicalProfile(explicitProfile);
        return canonical.isEmpty() ? QStringLiteral("adaptive") : canonical;
    }

    QSettings startupSettings{
        QSettings::NativeFormat,
        QSettings::UserScope,
        QStringLiteral("Genycaster"),
        QStringLiteral("Genycaster")};
    return RenderingController::canonicalProfile(
        startupSettings.value(QStringLiteral("rendering/profile"), QStringLiteral("adaptive")).toString());
}

[[nodiscard]] QString normalizeConfigurationToken(QString value)
{
    value = value.trimmed();
    for (int pass = 0; pass < 2 && value.size() >= 2; ++pass) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\"') && last == QLatin1Char('\"')) ||
            (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            value = value.sliced(1, value.size() - 2).trimmed();
            continue;
        }
        if (value.startsWith(QStringLiteral("\\\"")) &&
            value.endsWith(QStringLiteral("\\\"")) && value.size() >= 4) {
            value = value.sliced(2, value.size() - 4).trimmed();
            continue;
        }
        break;
    }
    return value;
}

[[nodiscard]] bool hasUnsafeConfigurationCharacters(const QStringView value)
{
    for (const QChar ch : value) {
        if (ch.isSpace() || ch == QLatin1Char('\\') || ch == QLatin1Char('\"') ||
            ch == QLatin1Char('\'')) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isValidHttpServiceUrl(
    const QString& value,
    const bool allowQuery)
{
    if (value.isEmpty() || hasUnsafeConfigurationCharacters(value)) {
        return false;
    }

    const QUrl url{value, QUrl::StrictMode};
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() ||
        (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) ||
        url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) {
        return false;
    }
    return allowQuery || !url.hasQuery();
}

[[nodiscard]] bool isValidSiwfUri(const QString& value)
{
    return isValidHttpServiceUrl(value, true);
}

[[nodiscard]] bool isValidSiwfDomain(const QString& value)
{
    if (value.isEmpty() || hasUnsafeConfigurationCharacters(value) ||
        value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('?')) ||
        value.contains(QLatin1Char('#'))) {
        return false;
    }
    const QUrl authority{QStringLiteral("http://") + value, QUrl::StrictMode};
    return authority.isValid() && !authority.host().isEmpty() && authority.userInfo().isEmpty();
}

[[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri)
{
    const QUrl authority{QStringLiteral("http://") + domain, QUrl::StrictMode};
    const QUrl relyingParty{uri, QUrl::StrictMode};
    if (!authority.isValid() || !relyingParty.isValid()) {
        return false;
    }
    if (authority.host().compare(relyingParty.host(), Qt::CaseInsensitive) != 0) {
        return false;
    }
    return authority.port(-1) == relyingParty.port(-1);
}

} // namespace genycaster::ui::bootstrap
