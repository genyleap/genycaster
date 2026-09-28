import QtQuick
import QtQuick.VectorImage

VectorImage {
    id: root
    property bool animationActive: false
    property int playCount: 1
    readonly property bool renderReady: root.children.length > 0 && root.implicitWidth > 0
    assumeTrustedSource: true
    fillMode: VectorImage.PreserveAspectFit
    preferredRendererType: VectorImage.CurveRenderer
    animations.loops: root.playCount < 0 ? Animation.Infinite : root.playCount

    function pauseSafely() {
        if (root.renderReady && !root.animationActive)
            root.animations.paused = true
    }

    onAnimationActiveChanged: {
        // Qt 6.11 VectorImage defines paused as a true pause/resume control:
        // clearing it continues from the current frame. Never restart here;
        // restarting on every ListView flick caused visible jumps and repeated
        // scene-graph work for every animated emoji in the row.
        if (root.animationActive) {
            root.animations.paused = false
        } else {
            root.pauseSafely()
        }
    }
    onRenderReadyChanged: {
        if (root.renderReady && !root.animationActive)
            root.pauseSafely()
    }
}
