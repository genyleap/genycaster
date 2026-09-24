import QtQuick
import QtQuick.Controls

Drawer {
    id: control

    property bool bottomSheet: false
    property bool dimBackground: false
    property int contentPadding: 0
    property Item hostItem: null
    property Item blurSourceItem: null
    property bool showHandle: bottomSheet
    readonly property int sheetTopInset: showHandle ? Theme.space5 : 0

    parent: hostItem ? hostItem : Overlay.overlay
    modal: true
    focus: true
    interactive: true
    dim: control.blurSourceItem !== null || control.dimBackground
    edge: bottomSheet ? Qt.BottomEdge : (Theme.rtl ? Qt.LeftEdge : Qt.RightEdge)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    leftPadding: contentPadding
    rightPadding: contentPadding
    topPadding: contentPadding + sheetTopInset
    bottomPadding: contentPadding

    // Blur belongs to the environment behind the drawer, not to the drawer
    // surface itself. Qt creates these components underneath visible popups.
    Overlay.modal: DrawerBackdrop {
        sourceItem: control.blurSourceItem
        active: control.opened
        addScrim: control.dimBackground
        blockInput: true
        onBackgroundTapped: control.close()
    }

    Overlay.modeless: DrawerBackdrop {
        sourceItem: control.blurSourceItem
        active: control.opened
        addScrim: control.dimBackground
        blockInput: false
    }

    background: PopupSurface {
        fillColor: Theme.surfaceElevated
        curveDepth: control.bottomSheet ? Theme.curveSheet : Theme.curveDrawer
        elevation: Theme.elevationNone
        curveTopLeft: control.bottomSheet || control.edge !== Qt.LeftEdge
        curveTopRight: control.bottomSheet || control.edge !== Qt.RightEdge
        curveBottomLeft: !control.bottomSheet && control.edge !== Qt.LeftEdge
        curveBottomRight: !control.bottomSheet && control.edge !== Qt.RightEdge

        Superellipse {
            visible: control.showHandle
            anchors.top: parent.top
            anchors.topMargin: Theme.space2
            anchors.horizontalCenter: parent.horizontalCenter
            width: 40
            height: 4
            fillColor: Theme.textTertiary
            opacity: 0.5
            borderWidth: 0
            curveDepth: 2
        }
    }
}
