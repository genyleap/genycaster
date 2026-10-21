import QtQuick
import QtQuick.Effects

Item {
    id: root

    property Item sourceItem: null
    property bool active: false
    property bool addScrim: true
    property bool blockInput: false
    property bool snapshotBlurOnBrowser: false
    // Pointer presses and synthesized taps may both be delivered on WASM.
    // Dismiss exactly once per open, even if both handlers receive the gesture.
    property bool outsideDismissConsumed: false
    readonly property bool useBrowserSnapshot: root.browserRenderer && root.snapshotBlurOnBrowser
    onActiveChanged: {
        if (root.active) {
            root.outsideDismissConsumed = false
            if (root.useBrowserSnapshot && root.sourceItem)
                Qt.callLater(() => capture.scheduleUpdate())
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
    // duplicates GPU work and causes scroll hitching on mobile Safari/WebGL.
    readonly property bool browserRenderer: Qt.platform.os === "wasm"
    readonly property real captureScale: renderingController.profile === "fidelity"
                                              || renderingController.effectiveQuality === "fidelity" ? 1.0
                                            : renderingController.profile === "efficiency"
                                              || renderingController.effectiveQuality === "efficiency" ? 0.72
                                            : 0.90
    readonly property real blurStrength: renderingController.profile === "efficiency"
                                         || renderingController.effectiveQuality === "efficiency" ? 0.46 : 0.58

    anchors.fill: parent
    visible: root.active

    ShaderEffectSource {
        id: capture
        visible: false
        // One texture snapshot on browser search open, never a live feed
        // repaint during each WebGL scroll frame.
        live: root.active && root.sourceItem !== null && !root.browserRenderer
        recursive: false
        hideSource: root.active && root.sourceItem !== null && !root.browserRenderer
        sourceItem: root.sourceItem
        textureSize: Qt.size(
            root.browserRenderer && !root.useBrowserSnapshot ? 1
                : Math.max(1, Math.round(root.width * (root.useBrowserSnapshot ? 0.48 : root.captureScale))),
            root.browserRenderer && !root.useBrowserSnapshot ? 1
                : Math.max(1, Math.round(root.height * (root.useBrowserSnapshot ? 0.48 : root.captureScale))))
        sourceRect: {
            if (!root.sourceItem || root.width <= 0 || root.height <= 0)
                return Qt.rect(0, 0, 1, 1)
            const origin = root.sourceItem.mapFromItem(root, 0, 0)
            return Qt.rect(origin.x, origin.y, root.width, root.height)
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.active && root.sourceItem !== null
                 && (!root.browserRenderer || root.useBrowserSnapshot)
        source: capture
        blurEnabled: true
        blur: root.blurStrength
        blurMax: 30
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
