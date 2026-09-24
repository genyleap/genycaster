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
        if (!root.renderReady)
            return
        // Restart first because Qt rejects setPaused(true) for an animation
        // that has already reached its finite end state.
        root.animations.restart()
        Qt.callLater(() => {
            if (root.renderReady && !root.animationActive)
                root.animations.paused = true
        })
    }

    onAnimationActiveChanged: {
        if (root.animationActive) {
            if (root.animations.paused)
                root.animations.paused = false
            if (root.renderReady)
                root.animations.restart()
        } else {
            root.pauseSafely()
        }
    }
    onRenderReadyChanged: {
        if (root.renderReady && !root.animationActive)
            root.pauseSafely()
    }
}
