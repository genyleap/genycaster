import QtQuick
import QtQuick.VectorImage

VectorImage {
    id: root
    property bool animationActive: false
    property int playCount: 1
    // The Qt 6.12 Lottie generator exposes this timeline on generatedItem.
    // Its `NumberAnimation on frameCounter` is a property value source, not
    // an Item.data entry. Walking Item.data never found the playback owner.
    readonly property var timeline: root.generatedItem
    readonly property bool hasTimeline: root.timeline !== null
        && typeof root.timeline.frameCounter === "number"
        && typeof root.timeline.endFrame === "number"
    readonly property bool playbackCompleted: root.hasTimeline
        && root.timeline.frameCounter >= root.timeline.endFrame
    readonly property bool renderFailed: root.status === VectorImage.Error
        || (root.status === VectorImage.Ready && !root.hasTimeline)
    // At the exclusive endFrame Qt hides the generated root, but leaves its
    // children and implicit size intact. Observe the actual scene so Unicode
    // returns in the same frame, without a delayed completion callback.
    readonly property bool renderReady: root.status === VectorImage.Ready
        && root.hasTimeline && root.timeline.visible
        && root.implicitWidth > 0 && root.implicitHeight > 0
        && !root.playbackCompleted
    assumeTrustedSource: true
    fillMode: VectorImage.PreserveAspectFit
    preferredRendererType: VectorImage.CurveRenderer
    animations.loops: root.playCount < 0 ? Animation.Infinite : root.playCount
    // A completed NumberAnimation cannot be paused. Retaining its renderer
    // during scroll/focus changes must not attempt to restart or pause it.
    animations.paused: !root.animationActive && !root.playbackCompleted
    opacity: root.renderReady ? 1 : 0
}
