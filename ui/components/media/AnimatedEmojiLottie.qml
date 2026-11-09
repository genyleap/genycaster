import QtQuick
import QtQuick.VectorImage
import Farcaster.Client

VectorImage {
    id: root
    property bool animationActive: false
    property int playCount: 1
    // Qt's generated Lottie root hides at its exclusive endFrame. Freeze one
    // visible frame before that boundary instead of returning to Unicode.
    property bool cycleFinished: false
    function freezeLastFrame() {
        if (!root.hasTimeline || root.cycleFinished || root.playCount < 0)
            return
        const lastVisible = Math.max(0, root.timeline.endFrame - 1)
        if (root.timeline.frameCounter < lastVisible)
            return
        root.cycleFinished = true
        if (root.timeline.frameCounter >= root.timeline.endFrame)
            root.timeline.frameCounter = lastVisible
    }
    onSourceChanged: root.cycleFinished = false
    Connections {
        target: root.timeline
        ignoreUnknownSignals: true
        function onFrameCounterChanged() {
            if (root.animationActive)
                root.freezeLastFrame()
        }
    }
    // The Qt 6.12 Lottie generator exposes this timeline on generatedItem.
    // Its `NumberAnimation on frameCounter` is a property value source, not
    // an Item.data entry. Walking Item.data never found the playback owner.
    readonly property var timeline: root.generatedItem
    readonly property bool hasTimeline: root.timeline !== null
        && typeof root.timeline.frameCounter === "number"
        && typeof root.timeline.endFrame === "number"
    readonly property bool playbackCompleted: root.cycleFinished
        || (root.hasTimeline && root.timeline.frameCounter >= root.timeline.endFrame)
    readonly property bool renderFailed: root.status === VectorImage.Error
        || (root.status === VectorImage.Ready && !root.hasTimeline)
    // Qt hides the generated root at its exclusive endFrame. Observe the
    // actual scene, and freeze a visible frame before that boundary.
    readonly property bool renderReady: root.status === VectorImage.Ready
        && root.hasTimeline && root.timeline.visible
        && root.implicitWidth > 0 && root.implicitHeight > 0
    assumeTrustedSource: true
    fillMode: VectorImage.PreserveAspectFit
    preferredRendererType: VectorImage.CurveRenderer
    // Never allow a composed inline emoji to request an unbounded timeline.
    animations.loops: 1
    // A completed NumberAnimation cannot be paused. Retaining its renderer
    // during scroll/focus changes must not attempt to restart or pause it.
    animations.paused: !root.animationActive || root.playbackCompleted
    opacity: root.renderReady ? 1 : 0
}
