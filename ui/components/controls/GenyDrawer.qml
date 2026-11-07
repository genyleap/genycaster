import QtQuick
import QtQuick.Controls
import Farcaster.Client

Drawer {
    id: control

    property bool bottomSheet: false
    property bool fullScreen: false
    property bool floatingSheet: false
    property bool dimBackground: false
    property int contentPadding: 0
    property Item hostItem: null
    property Item blurSourceItem: null
    // Full-screen inspectors have their own Cancel/Close navigation. Only
    // short, gesture-dismissable bottom sheets expose a pull handle.
    property bool showHandle: bottomSheet && !fullScreen
    readonly property int sheetTopInset: showHandle ? Theme.space5 : 0
    // A predictable, less abrupt motion contract for every shared inspector.
    // Reduced-motion requests bypass both translations and fades.
    readonly property int sheetMotionDuration: Theme.reduceMotion ? 0 : 275

    parent: hostItem ? hostItem : Overlay.overlay
    modal: true
    focus: true
    interactive: true
    dim: control.blurSourceItem !== null || control.dimBackground
    // Side inspectors mirror with the active interface language: RTL opens
    // from the physical LEFT; LTR opens from the physical RIGHT.
    // Phone-sized bottom sheets keep their BottomEdge in both directions.
    // Qt Drawer owns x/y and interactive drag geometry through this edge.
    edge: bottomSheet ? Qt.BottomEdge : (Theme.rtl ? Qt.LeftEdge : Qt.RightEdge)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    leftInset: floatingSheet ? Theme.space3 : 0
    rightInset: floatingSheet ? Theme.space3 : 0
    bottomInset: floatingSheet ? Theme.space3 : 0
    leftPadding: contentPadding + (floatingSheet ? Theme.space3 : 0)
    rightPadding: contentPadding + (floatingSheet ? Theme.space3 : 0)
    topPadding: contentPadding + sheetTopInset
    bottomPadding: contentPadding + (floatingSheet ? Theme.space3 : 0)

    // Qt's Drawer can lose focus to embedded editors, WebAssembly canvases,
    // or an app-wide shortcut handler. Desktop dismissal must remain available
    // even when the close glyph is off-screen. Mobile sheets retain their own
    // navigation/back contract.
    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        enabled: control.visible && !control.bottomSheet
        onActivated: control.close()
    }

    // One motion contract for side drawers and bottom sheets. Animate the
    // Drawer.position property so interactive edge gestures stay in sync;
    // never animate x/y, which Qt owns for an edge-anchored Drawer.
    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "position"
                from: 0.0
                to: 1.0
                duration: control.sheetMotionDuration
                easing.type: Easing.OutQuart
            }
            NumberAnimation {
                property: "opacity"
                from: 0.96
                to: 1.0
                duration: control.sheetMotionDuration
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "position"
                to: 0.0
                duration: control.sheetMotionDuration
                easing.type: Easing.InOutCubic
            }
            NumberAnimation {
                property: "opacity"
                to: 0.96
                duration: control.sheetMotionDuration
            }
        }
    }

    // Blur belongs to the environment behind the drawer, not to the drawer
    // surface itself. Qt creates these components underneath visible popups.
    Overlay.modal: DrawerBackdrop {
        sourceItem: control.blurSourceItem
        active: control.position > 0
        opacity: control.position
        addScrim: control.dimBackground
        blockInput: true
        onBackgroundTapped: control.close()

        // Native desktop needs a deterministic outside-press path even when
        // the Drawer gesture recognizer consumes the modal backdrop event.
        // Keep the existing mobile bottom-sheet behavior unchanged.
        MouseArea {
            anchors.fill: parent
            z: 200
            // Desktop browsers have platform.os === wasm. The viewport
            // presentation (not the runtime OS) determines outside dismissal.
            visible: !control.bottomSheet
            enabled: visible
            acceptedButtons: Qt.AllButtons
            onPressed: mouse => {
                mouse.accepted = true
                control.close()
            }
            onReleased: mouse => mouse.accepted = true
        }
    }

    Overlay.modeless: DrawerBackdrop {
        sourceItem: control.blurSourceItem
        active: control.position > 0
        opacity: control.position
        addScrim: control.dimBackground
        blockInput: false
    }

    background: PopupSurface {
        fillColor: Theme.surfaceElevated
        curveDepth: control.fullScreen ? 0
                    : control.floatingSheet ? Theme.curveSheetFloating
                    : control.bottomSheet ? Theme.curveSheet : Theme.curveDrawer
        elevation: Theme.elevationNone
        curveTopLeft: !control.fullScreen && (control.bottomSheet || control.edge !== Qt.LeftEdge)
        curveTopRight: !control.fullScreen && (control.bottomSheet || control.edge !== Qt.RightEdge)
        curveBottomLeft: !control.fullScreen && (control.floatingSheet || (!control.bottomSheet && control.edge !== Qt.LeftEdge))
        curveBottomRight: !control.fullScreen && (control.floatingSheet || (!control.bottomSheet && control.edge !== Qt.RightEdge))

        // Form sheets disable Drawer.interactive so scroll gestures belong to
        // their Flickable. The isolated grabber remains swipe-dismissable,
        // without stealing vertical drags anywhere in the editor body.
        Item {
            visible: control.bottomSheet && control.showHandle
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: 96
            height: 28
            z: 2

            Superellipse {
                anchors.top: parent.top
                anchors.topMargin: Theme.space2
                anchors.horizontalCenter: parent.horizontalCenter
                width: 40
                height: 4
                fillColor: Theme.textTertiary
                opacity: 0.55
                borderWidth: 0
                curveDepth: 2
            }

            MouseArea {
                anchors.fill: parent
                enabled: !control.interactive
                property real startY: 0
                onPressed: mouse => startY = mouse.y
                onReleased: mouse => {
                    if (mouse.y - startY > 32) control.close()
                }
                Accessible.name: qsTrId("genycaster.shell.close")
                Accessible.role: Accessible.Button
                Accessible.onPressAction: control.close()
            }
        }
    }
}
