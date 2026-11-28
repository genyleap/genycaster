#!/usr/bin/env python3
"""Run the actual shell route admission binding in Qt Quick."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
QML = """import QtQuick
import QtTest
Item {
    id: root
    width: 320; height: 240
    property bool shuttingDown: false
    // ROUTE_COMPONENT
    RouteSurfaceLoader {
        id: current
        routeRequested: false
        retainLoaded: true
        sourceComponent: Component { Item { width: 100; height: 100 } }
    }
    RouteSurfaceLoader {
        id: closed
        routeRequested: false
        sourceComponent: Component { Item { width: 100; height: 100 } }
    }
    RouteSurfaceLoader {
        id: transient
        routeRequested: false
        sourceComponent: Component { Item { width: 100; height: 100 } }
    }
    TestCase {
        name: "RouteLoaderShutdown"
        when: windowShown
        function test_admissionFreeze() {
            verify(!current.active && !closed.active && !transient.active)
            current.routeRequested = true
            tryCompare(current, "status", Loader.Ready)
            const original = current.item
            current.routeRequested = false
            verify(current.active)
            compare(current.item, original)
            transient.routeRequested = true
            tryCompare(transient, "status", Loader.Ready)
            transient.routeRequested = false
            tryCompare(transient, "active", false)
            transient.routeRequested = true
            verify(transient.active)
            compare(transient.status, Loader.Loading)
            root.shuttingDown = true
            transient.routeRequested = false
            closed.routeRequested = true
            wait(80)
            verify(transient.active, "already admitted route must not cancel during shutdown")
            verify(current.active)
            compare(current.item, original)
            verify(!closed.active, "shutdown cannot admit a cold route")
            tryCompare(transient, "status", Loader.Ready)
        }
    }
}
"""

def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER")
    shell = (ROOT / "ui/components/shell/AppShell.qml").read_text()
    first = shell.index("    component RouteSurfaceLoader:")
    last = shell.index("    component UtilitySurface:", first)
    component = shell[first:last].replace("GenySpinner {", "Item {")
    with tempfile.TemporaryDirectory(prefix="genycaster-route-shutdown-") as tmp:
        Path(tmp, "tst_route.qml").write_text(QML.replace("    // ROUTE_COMPONENT", component))
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
        result = subprocess.run([runner, "-input", tmp], env=env,
                                capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        print(output)
        if result.returncode or any(w in output for w in (
            "Binding loop detected", "ReferenceError:", "TypeError:",
            "during incubation", "items in the process", "QWARN")):
            raise SystemExit(1)

if __name__ == "__main__":
    main()
