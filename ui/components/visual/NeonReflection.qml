import QtQuick
import Farcaster.Client
import "NeonGeometry.js" as NeonGeometry

// Chromatic material layer: the shared source is projected onto the adjacent
// receiver. Source-facing edges get color; neutral/no-source stays neutral.
ShaderEffect {
    id: reflection
    property NeonReflectionSource reflectionSource: null
    property rect sampleRect: NeonGeometry.translatedRect(reflection,
        reflectionSource ? reflectionSource.sourceItem : null)
    property real strength: Theme.reflectionStrength
    property real reach: Theme.reflectionReach
    property real cornerRadius: Theme.curveContainer
    property real cornerPower: Theme.superellipseExponent
    property real borderInset: 1
    property real edgeStrength: 0
    property real edgeWidth: 1
    property real edgeInset: edgeWidth / 2
    property bool bottomEdgeOnly: false
    readonly property bool sourceIsSafe: {
        if (!reflectionSource || !reflectionSource.capturing) return false
        let ancestor = reflection
        while (ancestor) {
            if (ancestor === reflectionSource.sourceItem) return false
            ancestor = ancestor.parent
        }
        return true
    }
    visible: sourceIsSafe && (strength > 0 || edgeStrength > 0)
        && width > 2 * borderInset && height > 2 * borderInset
    blending: true

    // An optional ShaderEffect sampler must always have a valid texture
    // provider, including when reflection strength is zero.
    Image {
        id: emptyEnvironment
        source: "qrc:/genycaster/effects/transparent.png"
        width: 1
        height: 1
        visible: false
        asynchronous: false
        cache: true
    }
    property variant source: sourceIsSafe ? reflectionSource.texture : emptyEnvironment
    property vector2d sourceSize: Qt.vector2d(
        reflectionSource ? Math.max(1, reflectionSource.width) : 1,
        reflectionSource ? Math.max(1, reflectionSource.height) : 1)
    property rect samplingRect: sampleRect
    property vector4d surfaceParameters: Qt.vector4d(
        width, height, cornerRadius, cornerPower)
    property vector4d materialParameters: Qt.vector4d(
        // The previous 0.25 ceiling clipped the restored header strength
        // and made chromatic reflections barely visible over light chrome.
        Math.max(0, Math.min(0.42, strength)) * Theme.reflectionGain,
        Math.max(0, borderInset), Math.max(1, reach),
        reflectionSource ? 2 * reflectionSource.effectiveBlur : 0)
    property vector4d edgeParameters: Qt.vector4d(
        Math.max(0, Math.min(1, edgeStrength)) * Theme.reflectionGain,
        Math.max(0, edgeWidth), Math.max(edgeWidth / 2, edgeInset),
        Theme.dark ? 1 : 0)
    property real edgeMode: bottomEdgeOnly ? 1 : 0
    fragmentShader: "qrc:/genycaster/effects/reflection.frag.qsb"
}
