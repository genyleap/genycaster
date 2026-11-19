import QtQuick
import Farcaster.Client

// One low-resolution GPU capture per neighboring content region. Never place
// this object under sourceItem: a reflected surface must not capture itself.
Item {
    id: root
    property Item sourceItem: null
    property bool active: true
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
    readonly property real captureScale: Math.min(0.125,
        512 / Math.max(1, width, height))
    readonly property size textureExtent: Qt.size(
        Math.max(1, Math.ceil(width * captureScale)),
        Math.max(1, Math.ceil(height * captureScale)))
    readonly property real effectiveBlur: Math.max(24, Math.min(160, blurRadius))

    width: sourceItem ? sourceItem.width : 0
    height: sourceItem ? sourceItem.height : 0
    visible: false

    // A single-threaded WebAssembly render loop must not recapture the entire
    // adjacent scene on every frame. Refresh the shared low-resolution color
    // field at a bounded rate; interactive neon geometry remains frame-paced.
    Timer {
        interval: 160
        repeat: true
        triggeredOnStart: true
        running: root.browserRenderer && root.capturing
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
        live: root.capturing && !root.browserRenderer
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
        live: root.capturing
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
        live: root.capturing
        smooth: true
        wrapMode: ShaderEffectSource.ClampToEdge
    }
}
