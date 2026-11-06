#pragma once

#include <QString>
#include <QStringList>
#include <QStringView>

namespace genycaster::ui::bootstrap {

// Keep startup argument, rendering and service-URL validation independent of
// the QML engine and provider composition graph.
[[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix);
[[nodiscard]] QString startupRenderingProfile(int argc, char* argv[]);
[[nodiscard]] QString normalizeConfigurationToken(QString value);
[[nodiscard]] bool hasUnsafeConfigurationCharacters(QStringView value);
[[nodiscard]] bool isValidHttpServiceUrl(const QString& value, bool allowQuery = false);
[[nodiscard]] bool isValidSiwfUri(const QString& value);
[[nodiscard]] bool isValidSiwfDomain(const QString& value);
[[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri);

} // namespace genycaster::ui::bootstrap
