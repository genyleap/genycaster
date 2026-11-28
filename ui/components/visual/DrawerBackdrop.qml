import QtQuick
import QtQuick.Effects
import Farcaster.Client

Item {
    id: root

    property Item sourceItem: null
    property bool active: false
    property bool addScrim: true
    property bool blockInput: false
    property bool snapshotBlurOnBrowser: false
    property bool efficientNativeBlur: false
    // Pointer presses and synthesized taps may both be delivered on WASM.
    // Dismiss exactly once per open, even if both handlers receive the gesture.
    property bool outsideDismissConsumed: false
    readonly property bool useBrowserSnapshot: root.browserRenderer && root.snapshotBlurOnBrowser
    // Non-settings native dialogs retain the reference two-pass Gaussian.
    // Appearance settings use an optimized downsampled MultiEffect so one
    // theme change cannot saturate the compositor with two full-res passes.
    // WebAssembly keeps its proven reduced-resolution snapshot fallback.
    readonly property bool nativeBlurActive: !root.browserRenderer && root.active
                                              && root.sourceItem !== null
    readonly property bool nativeGaussian: root.nativeBlurActive && !root.efficientNativeBlur
    readonly property bool optimizedNativeBlur: root.nativeBlurActive && root.efficientNativeBlur
    readonly property real gaussianAmount: 1.0
    property bool captureUpdateQueued: false
    function performSnapshotUpdate() {
        root.captureUpdateQueued = false
        if (root.active && root.sourceItem)
            capture.scheduleUpdate()
    }
    function refreshSnapshot() {
        if (!root.active || !root.sourceItem || root.captureUpdateQueued)
            return
        root.captureUpdateQueued = true
        // All triggers share one scheduled update, even during rapid resizing.
        Qt.callLater(root.performSnapshotUpdate)
    }
    onActiveChanged: {
        if (root.active)
            root.outsideDismissConsumed = false
        root.refreshSnapshot()
    }
    onSourceItemChanged: root.refreshSnapshot()
    onWidthChanged: root.refreshSnapshot()
    onHeightChanged: root.refreshSnapshot()
    // The backdrop is deliberately frozen between changes. Refresh once
    // AFTER Qt has propagated the new palette, not on each animated color
    // frame of dozens of live cards beneath the settings inspector.
    Timer {
        id: paletteSnapshotRefresh
        // Coalesce palette changes and wait until the visible scene settles;
        // capturing during its paint animations creates a GPU synchronization stall.
        interval: Math.max(360, Theme.materialSurfaceDuration + 220)
        repeat: false
        onTriggered: root.refreshSnapshot()
    }
    Connections {
        target: Theme
        enabled: root.efficientNativeBlur
        function onDarkChanged() {
            if (root.optimizedNativeBlur)
                paletteSnapshotRefresh.restart()
        }
        function onColorThemeIdChanged() {
            if (root.optimizedNativeBlur)
                paletteSnapshotRefresh.restart()
        }
        function onCustomAccentChanged() {
            if (root.optimizedNativeBlur)
                paletteSnapshotRefresh.restart()
        }
    }
    signal backgroundTapped()
    function requestOutsideDismiss() {
        if (!root.active || !root.blockInput || root.outsideDismissConsumed)
            return
        root.outsideDismissConsumed = true
        root.backgroundTapped()
    }
    // WebAssembly's Qt Quick render loop runs on the browser main thread.
    // Sampling a live full-screen feed into a blurred texture every frame
    // duplicates GPU work and can cause scroll hitching in browser renderers.
    readonly property bool browserRenderer: Qt.platform.os === "wasm"
    // Telemetry must never resize a live GPU capture or change blur strength
    // during an interaction. Only an explicit profile choice sets the budget.
    readonly property real captureScale: renderingController.profile === "fidelity" ? 1.0
                                            : renderingController.profile === "efficiency" ? 0.72
                                            : 0.90
    readonly property real blurStrength: renderingController.profile === "efficiency" ? 0.46 : 0.58
    // Original panels retain their HiDPI snapshot. The theme inspector
    // intentionally samples at <= 48% viewport resolution: detail behind a
    // frosted sheet is intentionally obscured, so Retina-scale copies only
    // waste memory bandwidth and induce expensive GPU synchronization.
    readonly property real nativeCaptureScale: root.efficientNativeBlur
        ? Math.min(0.48, 1440 / Math.max(1, root.width, root.height))
        : Math.min(Math.max(1, Screen.devicePixelRatio) * root.captureScale,
                   4096 / Math.max(1, root.width, root.height))

    anchors.fill: parent
    visible: root.active

    ShaderEffectSource {
        id: capture
        objectName: "drawerBackdropCapture"
        visible: false
        // One texture snapshot on browser search open, never a live feed
        // repaint during each WebGL scroll frame.
        // Freeze background after opening: a modal should not re-render the
        // entire animated feed through two Gaussian passes every frame.
        live: false
        recursive: false
        hideSource: root.nativeBlurActive
        // A closed drawer must not retain a full-window capture target.
        sourceItem: root.active && (!root.browserRenderer || root.useBrowserSnapshot)
                    ? root.sourceItem : null
        textureSize: Qt.size(
            !root.active || (root.browserRenderer && !root.useBrowserSnapshot) ? 1
                : Math.max(1, Math.round(root.width * (root.browserRenderer
                                  ? 0.48 : root.nativeCaptureScale))),
            !root.active || (root.browserRenderer && !root.useBrowserSnapshot) ? 1
                : Math.max(1, Math.round(root.height * (root.browserRenderer
                                  ? 0.48 : root.nativeCaptureScale))))
        sourceRect: {
            if (!root.sourceItem || root.width <= 0 || root.height <= 0)
                return Qt.rect(0, 0, 1, 1)
            const origin = root.sourceItem.mapFromItem(root, 0, 0)
            return Qt.rect(origin.x, origin.y, root.width, root.height)
        }
    }

    // Separable Gaussian from the supplied UI Kit. Hidden horizontal pass is
    // captured into a single intermediate texture; only the vertical output
    // is visible. Never capture Window.contentItem/Overlay here.
    ShaderEffect {
        id: horizontalPass
        width: root.width
        height: root.height
        visible: false
        property var source: capture
        property vector2d sampleStep: Qt.vector2d(
            root.gaussianAmount * 0.5 / Math.max(1, width), 0)
        fragmentShader: "qrc:/genycaster/effects/gaussian.frag.qsb"
    }
    ShaderEffectSource {
        id: intermediate
        sourceItem: root.nativeGaussian ? horizontalPass : null
        visible: false
        live: root.nativeGaussian
        // Skip the second full-screen offscreen render target for the
        // optimized frosted Settings backdrop.
        textureSize: root.nativeGaussian ? capture.textureSize : Qt.size(1, 1)
        smooth: true
    }
    ShaderEffect {
        anchors.fill: parent
        visible: root.nativeGaussian
        property var source: intermediate
        property vector2d sampleStep: Qt.vector2d(
            0, root.gaussianAmount * 0.5 / Math.max(1, height))
        fragmentShader: "qrc:/genycaster/effects/gaussian.frag.qsb"
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.active && (root.useBrowserSnapshot || root.optimizedNativeBlur)
                 && root.sourceItem !== null
        source: capture
        // Qt Quick MultiEffect implements a downsampled blur pyramid.
        // A moderate radius approximates the reference's tight Gaussian
        // instead of turning the entire desktop into an oversized blur.
        // A frozen, low-res compositor effect avoids two 47-tap passes.
        blurEnabled: true
        blur: root.optimizedNativeBlur ? 0.66 : root.blurStrength
        blurMax: root.optimizedNativeBlur ? 20 : 30
        saturation: 0.92
        autoPaddingEnabled: false
        antialiasing: true
    }

    Rectangle {
        anchors.fill: parent
        color: root.addScrim
               ? (root.snapshotBlurOnBrowser
                  ? (Theme.dark ? Qt.rgba(0.02, 0.025, 0.035, 0.68)
                                : Qt.rgba(0.94, 0.95, 0.97, 0.62))
                  : (Theme.dark ? Qt.rgba(0.02, 0.025, 0.035, 0.42)
                                : Qt.rgba(0.94, 0.95, 0.97, 0.38)))
               : "transparent"
    }

    // Qt/WASM may deliver touch as a pointer gesture without a synthesized
    // MouseArea click. Keep outside-tap dismissal reliable on mobile browsers.
    TapHandler {
        enabled: root.blockInput
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.requestOutsideDismiss()
    }

    MouseArea {
        anchors.fill: parent
        z: 100
        enabled: root.blockInput
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
        propagateComposedEvents: false
        onPressed: mouse => {
            mouse.accepted = true
            // Browsers may skip a synthesized click after focus/keyboard
            // transitions; dismiss the search drawer on outside *press*.
            root.requestOutsideDismiss()
        }
        onReleased: mouse => mouse.accepted = true
        onDoubleClicked: mouse => mouse.accepted = true
        onClicked: mouse => {
            mouse.accepted = true
            // The press (or touch TapHandler) already dismissed the popup.
        }
        onWheel: wheel => wheel.accepted = true
    }
}
