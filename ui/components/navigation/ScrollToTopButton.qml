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
    text: qsTrId("genycaster.navigation.back_to_top")
    z: 40
    visible: flickable !== null && distanceFromTop > revealDistance
    enabled: visible
    opacity: visible ? 1 : 0

    onClicked: {
        if (!flickable) return
        flickable.cancelFlick()
        // A ListView only instantiates delegates near its viewport. Animating
        // contentY through an entire long feed outruns that incubation and
        // displays an empty screen for several seconds. Its indexed positioning
        // API jumps directly to the real header without traversing unloaded
        // rows or temporarily restoring an obsolete contentY.
        flickable.positionViewAtBeginning()
    }
}
