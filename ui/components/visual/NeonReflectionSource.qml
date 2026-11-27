import QtQuick
import Farcaster.Client

// One low-resolution GPU capture per neighboring content region. Never place
// this object under sourceItem: a reflected surface must not capture itself.
Item {
    id: root
    property Item sourceItem: null
    property bool active: true
    property bool motionActive: false
    property real blurRadius: Theme.reflectionBlurRadius
    property alias texture: blurredTexture
    // WebGL1 does not guarantee half-float color-renderable attachments.
    // Keep native's wider color range; use portable RGBA8 for the browser.
    readonly property bool browserRenderer: Qt.platform.os === "wasm"
    readonly property bool sourceIsSafe: {
        if (!sourceItem) return false
        let ancestor = root
        while (ancestor) {
            if (ancestor === sourceItem) return false
            ancestor = ancestor.parent
        }
        return true
    }
    readonly property bool capturing: active && enabled && sourceIsSafe
        && Theme.reflectionGain > 0 && !Theme.paletteSwitchInProgress
        && width > 0 && height > 0
    readonly property real captureScale: root.browserRenderer
        ? Math.min(0.0625, 256 / Math.max(1, width, height))
        : Math.min(0.125, 512 / Math.max(1, width, height))
    readonly property size desiredTextureExtent: Qt.size(
        Math.max(1, Math.ceil(width * captureScale)),
        Math.max(1, Math.ceil(height * captureScale)))
    // Capture targets are GPU allocations, not layout dimensions. Retain
    // the last color field during a live resize, then refresh all three
    // passes together instead of reallocating their FBOs every frame.
    property size textureExtent: Qt.size(1, 1)
    property bool captureGeometryReady: false
    property bool resizingCapture: false
    function captureGeometryChanged() {
        if (!captureGeometryReady) return
        resizingCapture = true
        captureResizeSettler.restart()
    }
    onWidthChanged: captureGeometryChanged()
    onHeightChanged: captureGeometryChanged()
    Component.onCompleted: {
        textureExtent = desiredTextureExtent
        captureGeometryReady = true
    }
    Timer {
        id: captureResizeSettler
        interval: 180
        repeat: false
        onTriggered: {
            root.textureExtent = root.desiredTextureExtent
            root.resizingCapture = false
            if (root.capturing) {
                capture.scheduleUpdate()
                horizontalTexture.scheduleUpdate()
                blurredTexture.scheduleUpdate()
            }
        }
    }
    onMotionActiveChanged: {
        if (!motionActive && capturing && !resizingCapture) {
            capture.scheduleUpdate()
            horizontalTexture.scheduleUpdate()
            blurredTexture.scheduleUpdate()
        }
    }
    // Retain broad color fields without blurring saturated pixels into a
    // near-neutral average. Mobile keeps a shorter effective sampling reach.
    readonly property real effectiveBlur: Math.max(14, Math.min(160, blurRadius))

    width: sourceItem ? sourceItem.width : 0
    height: sourceItem ? sourceItem.height : 0
    visible: false

    // Qt/WASM runs scene-graph capture, texture upload and both blur passes
    // on the browser's graphics path. A 160ms full-pane capture at 1/8 scale
    // repeatedly stalled scrolling on mid-range GPUs. Keep one shared source
    // but capture a coarse *color* field at a measured cadence; text/edges
    // remain rendered by Qt at full resolution.
    Timer {
        interval: 240
        repeat: true
        triggeredOnStart: true
        running: root.browserRenderer && root.capturing && !root.resizingCapture && !root.motionActive
        onTriggered: capture.scheduleUpdate()
    }

    ShaderEffectSource {
        id: capture
        sourceItem: root.capturing ? root.sourceItem : null
        sourceRect: Qt.rect(0, 0, root.width, root.height)
        format: root.browserRenderer ? ShaderEffectSource.RGBA8 : ShaderEffectSource.RGBA16F
        textureSize: root.textureExtent
        hideSource: false
        recursive: false
        visible: false
        live: root.capturing && !root.browserRenderer && !root.resizingCapture && !root.motionActive
        smooth: true
        wrapMode: ShaderEffectSource.ClampToEdge
    }
    ShaderEffect {
        id: horizontalPass
        width: root.width
        height: root.height
        visible: false
        property variant source: capture
        property vector2d sampleStep: Qt.vector2d(
            root.effectiveBlur / (16.5 * Math.max(1, width)), 0)
        fragmentShader: "qrc:/genycaster/effects/reflection-gaussian.frag.qsb"
    }
    ShaderEffectSource {
        id: horizontalTexture
        sourceItem: root.capturing ? horizontalPass : null
        format: root.browserRenderer ? ShaderEffectSource.RGBA8 : ShaderEffectSource.RGBA16F
        textureSize: root.textureExtent
        visible: false
        live: root.capturing && !root.resizingCapture && !root.motionActive
        smooth: true
        wrapMode: ShaderEffectSource.ClampToEdge
    }
    ShaderEffect {
        id: verticalPass
        width: root.width
        height: root.height
        visible: false
        property variant source: horizontalTexture
        property vector2d sampleStep: Qt.vector2d(
            0, root.effectiveBlur / (16.5 * Math.max(1, height)))
        fragmentShader: "qrc:/genycaster/effects/gaussian.frag.qsb"
    }
    ShaderEffectSource {
        id: blurredTexture
        sourceItem: root.capturing ? verticalPass : null
        format: root.browserRenderer ? ShaderEffectSource.RGBA8 : ShaderEffectSource.RGBA16F
        textureSize: root.textureExtent
        visible: false
        live: root.capturing && !root.resizingCapture && !root.motionActive
        smooth: true
        wrapMode: ShaderEffectSource.ClampToEdge
    }
}
