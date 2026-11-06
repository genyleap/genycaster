// Qt's ABI remains globally attached; full Qt headers are private to implementations.
module;
#include "qt_fwd.hpp"

export module genycaster.ui.bootstrap;

export namespace genycaster::ui::bootstrap {
    [[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix);
    [[nodiscard]] QString startupRenderingProfile(int argc, char* argv[]);
    [[nodiscard]] QString normalizeConfigurationToken(QString value);
    [[nodiscard]] bool hasUnsafeConfigurationCharacters(QStringView value);
    [[nodiscard]] bool isValidHttpServiceUrl(const QString& value, bool allowQuery = false);
    [[nodiscard]] bool isValidSiwfUri(const QString& value);
    [[nodiscard]] bool isValidSiwfDomain(const QString& value);
    [[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri);
}
