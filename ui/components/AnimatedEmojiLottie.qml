import QtQuick
import QtQuick.VectorImage

VectorImage {
    id: root
    property bool animationActive: false
    property int playCount: 1
    property var playbackAnimations: []
    readonly property bool renderReady: root.children.length > 0 && root.implicitWidth > 0
    assumeTrustedSource: true
    fillMode: VectorImage.PreserveAspectFit
    preferredRendererType: VectorImage.CurveRenderer
    animations.loops: root.playCount < 0 ? Animation.Infinite : root.playCount

    function synchronizePlayback() {
        for (const animation of root.playbackAnimations) {
            // VectorImage's aggregate paused property forwards to stopped
            // animations too. Only a running animation can pause or resume;
            // a completed one-shot must stay completed after a scroll.
            if (animation !== null && animation.running)
                animation.paused = !root.animationActive
        }
    }

    function discoverPlaybackAnimations() {
        const animations = []
        const pending = []
        for (const child of root.children)
            pending.push(child)
        // Item.data includes nonvisual animations. Keep traversal in this
        // invocation instead of retaining recursive QML method callbacks.
        while (pending.length > 0) {
            const item = pending.pop()
            if (item === null)
                continue
            for (const entry of item.data) {
                if (entry instanceof NumberAnimation)
                    animations.push(entry)
                else if (entry instanceof Item)
                    pending.push(entry)
            }
        }
        root.playbackAnimations = animations
        root.synchronizePlayback()
    }

    onAnimationActiveChanged: root.synchronizePlayback()
    onPlayCountChanged: playbackSync.restart()
    onSourceChanged: {
        root.playbackAnimations = []
        playbackSync.restart()
    }
    onRenderReadyChanged: {
        if (root.renderReady)
            playbackSync.restart()
    }
    // A Loader can destroy this delegate before a deferred global callback
    // runs. This timer is owned and cancelled with the renderer itself.
    Timer {
        id: playbackSync
        interval: 0
        repeat: false
        onTriggered: root.discoverPlaybackAnimations()
    }
    Component.onCompleted: playbackSync.restart()
    Component.onDestruction: playbackSync.stop()
}
