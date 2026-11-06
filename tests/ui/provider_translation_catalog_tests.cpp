#include <QCoreApplication>
#include <QTranslator>
#include <QtTranslation>
#include <QString>
#include <cassert>

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    for (const QString& file : {
             QStringLiteral(GENYCASTER_QM_EN),
             QStringLiteral(GENYCASTER_QM_FA),
         }) {
        QTranslator translator{};
        assert(translator.load(file));
        assert(app.installTranslator(&translator));
        // Brand-specific metadata moved out of the generic catalog but the
        // shipped IDs/translations must remain available in every locale.
        for (const char* id : {
                 "genycaster.setup.farcaster_not_configured",
                 "genycaster.setup.farcaster_missing_key",
                 "genycaster.setup.connection_mode_help",
                 "genycaster.setup.hypersnap_explainer",
                 "genycaster.setup.farcaster_explainer",
                 "genycaster.setup.network_settings_explainer",
             }) {
            const QString translated = qtTrId(id);
            assert(!translated.isEmpty());
            assert(translated != QString::fromLatin1(id));
        }
        app.removeTranslator(&translator);
    }
    return 0;
}
