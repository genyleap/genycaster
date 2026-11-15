import QtQuick
import QtQuick.Controls
import Farcaster.Client

Popup {
    id: control

    // Explicit workspace source, never the Overlay itself (which contains
    // transient popups and would recurse through ShaderEffectSource).
    // Parent-owned dialogs automatically capture their safe parent surface.
    property Item blurSourceItem: parent && parent !== Overlay.overlay ? parent : null

    modal: true
    focus: true
    padding: Theme.space4
    x: parent ? Math.round(Math.max(0, (parent.width - width) / 2)) : 0
    y: parent ? Math.round(Math.max(0, (parent.height - height) / 2)) : 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Same frosted, input-blocking backdrop as GenyDrawer. Respect a caller's
    // NoAutoClose policy: wallet approval must never be rejected by tapping
    // its dimmed background.
    Overlay.modal: DrawerBackdrop {
        sourceItem: control.blurSourceItem
        snapshotBlurOnBrowser: true
        active: control.visible
        addScrim: true
        blockInput: true
        onBackgroundTapped: {
            if (control.closePolicy & Popup.CloseOnPressOutside)
                control.close()
        }
    }

    background: PopupSurface {}

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: Theme.motionStandard
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: Theme.motionFast
            easing.type: Easing.InCubic
        }
    }
}
