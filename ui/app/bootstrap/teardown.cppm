// Native QML teardown is kept after background work is stopped and joined.
module;
#include "qt_fwd.hpp"

export module genycaster.ui.bootstrap.teardown;

export namespace genycaster::ui::bootstrap {
    // Caller must stop runtime producers and join bootstrap threads first.
    // Destroy root objects before QGuiApplication and borrowed controller owners.
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
