#pragma once

class QQuickWindow;

namespace genycaster::ui::web {

// Platform-neutral entrypoint; the implementation is active only in WASM.
void installWheelBridge(QQuickWindow* window);

} // namespace genycaster::ui::web
