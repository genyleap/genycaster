// Render-profile startup reads only the pre-application preferences.
module;

#include "rendering_controller.hpp"
#include <QSettings>
#include <QString>
#include <string>

module genycaster.ui.bootstrap;

namespace genycaster::ui::bootstrap {
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
} // namespace genycaster::ui::bootstrap
