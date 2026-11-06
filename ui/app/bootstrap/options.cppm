// Qt declarations are globally attached forward declarations. Importers that
// use them include the corresponding Qt headers in their own translation unit.
module;
class QString;
template<class T> class QList;
using QStringList = QList<QString>;
class QStringView;

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
