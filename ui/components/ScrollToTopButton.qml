import QtQuick
import QtQuick.Controls

// Viewport overlay: stays fixed above the scroll content and is only offered
// after the reader has traveled farther than a screenful.
ActionButton {
    id: control
    objectName: "scrollToTopButton"

    required property var flickable
    readonly property real distanceFromTop: flickable
                                                 ? Math.max(0, flickable.contentY - flickable.originY)
                                                 : 0
    readonly property real revealDistance: flickable
                                                   ? Math.max(640, flickable.height * 1.25)
                                                   : 640

    compact: true
    iconOnly: true
    quiet: false
    prominent: true
    iconGlyph: Theme.iconChevronUp
    text: qsTr("Back to top")
    z: 40
    visible: flickable !== null && distanceFromTop > revealDistance
    enabled: visible
    opacity: visible ? 1 : 0

    onClicked: {
        if (!flickable) return
        flickable.cancelFlick()
        scrollAnimation.stop()
        const startingY = flickable.contentY
        // ListView's originY can shift as asynchronous delegates acquire their
        // final heights. Ask Qt for the actual beginning, then restore the old
        // offset within this same frame before starting the smooth transition.
        flickable.positionViewAtBeginning()
        const beginning = flickable.contentY
        flickable.contentY = startingY
        scrollAnimation.from = startingY
        scrollAnimation.to = beginning
        scrollAnimation.duration = Math.max(220,
            Math.min(520, Math.abs(scrollAnimation.from - beginning) * 0.12))
        scrollAnimation.start()
    }

    Connections {
        target: control.flickable
        function onDraggingChanged() {
            // Programmatic contentY animations can emit movementStarted;
            // only a real finger/mouse drag should interrupt the transition.
            if (control.flickable && control.flickable.dragging)
                scrollAnimation.stop()
        }
    }

    NumberAnimation {
        id: scrollAnimation
        target: control.flickable
        property: "contentY"
        easing.type: Easing.OutCubic
        onFinished: {
            if (control.flickable)
                control.flickable.positionViewAtBeginning()
        }
    }
}
