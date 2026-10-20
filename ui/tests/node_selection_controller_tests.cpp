#include "../mobile/node_selection_controller.hpp"
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariantMap>
#include <cassert>

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    QTemporaryDir directory{};
    assert(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("node-selection.ini"));
    QSettings settings{settingsPath, QSettings::IniFormat};
    const QString publicEndpoint{QStringLiteral("https://haatz.quilibrium.com")};
    const QString independentEndpoint{QStringLiteral("https://node.example.org")};
    settings.setValue(QStringLiteral("network/hypersnapBaseUrl"), independentEndpoint);
    settings.setValue(QStringLiteral("network/nodeSelectionMode"), QStringLiteral("manual"));
    settings.setValue(QStringLiteral("network/proxyMode"), QStringLiteral("system"));
    settings.sync();

    {
        // Probing disabled: no actual provider network requests.
        NodeSelectionController controller{
            settings, publicEndpoint,
            {independentEndpoint + QLatin1Char('/'), independentEndpoint,
             QStringLiteral("not-a-url")},
            QStringLiteral("system"), false};
        const QVariantList nodes = controller.nodes();
        assert(nodes.size() == 2);
        assert(controller.selectedEndpoint() == independentEndpoint);
        assert(!controller.automatic() && !controller.probing());
        assert(controller.restartRequired());
        assert(!controller.selectNode(QStringLiteral("unknown-node")));
        int selectionChanges{};
        int nodeChanges{};
        QObject::connect(&controller, &NodeSelectionController::selectionChanged,
            &controller, [&] { ++selectionChanges; });
        QObject::connect(&controller, &NodeSelectionController::nodesChanged,
            &controller, [&] { ++nodeChanges; });

        controller.refresh();
        assert(!controller.probing() && nodeChanges == 1);
        for (const QVariant& value : controller.nodes()) {
            const QVariantMap node = value.toMap();
            assert(node.value(QStringLiteral("status")) == QStringLiteral("idle"));
            assert(!node.value(QStringLiteral("online")).toBool());
        }
        assert(controller.setProxyMode(QStringLiteral("direct")));
        assert(controller.proxyRestartRequired());
        assert(controller.proxyMode() == QStringLiteral("direct"));
        assert(settings.value(QStringLiteral("network/proxyMode")) == QStringLiteral("direct"));
        assert(!controller.setProxyMode(QStringLiteral("invalid")));
        assert(selectionChanges == 1);

        controller.useAutomaticSelection();
        assert(controller.automatic());
        assert(settings.value(QStringLiteral("network/nodeSelectionMode")) == QStringLiteral("auto"));
        assert(settings.value(QStringLiteral("network/hypersnapBaseUrl")) == independentEndpoint);
        assert(controller.selectNode(nodes.at(0).toMap().value(QStringLiteral("id")).toString()));
        assert(!controller.automatic());
        assert(controller.selectedEndpoint() == publicEndpoint);
        assert(!controller.restartRequired());
        assert(settings.value(QStringLiteral("network/hypersnapBaseUrl")) == publicEndpoint);
        assert(selectionChanges >= 3);
    }

    QSettings reopened{settingsPath, QSettings::IniFormat};
    NodeSelectionController again{
        reopened, publicEndpoint, {independentEndpoint},
        QStringLiteral("system"), false};
    assert(!again.automatic());
    assert(again.selectedEndpoint() == publicEndpoint);
    assert(again.proxyMode() == QStringLiteral("direct"));
    assert(again.proxyRestartRequired());
    return 0;
}
