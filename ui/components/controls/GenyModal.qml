import QtQuick
import QtQuick.Controls
import Farcaster.Client

Popup {
    id: control

    // Explicit workspace source, never the Overlay itself (which contains
    // transient popups and would recurse through ShaderEffectSource).
    // Parent-owned dialogs automatically capture their safe parent surface.
    property Item blurSourceItem: parent && parent !== Overlay.overlay ? parent : null
    // Full-viewport inspectors use a flat, edge-to-edge modal surface.
    // Ordinary confirmation dialogs retain their inset rounded geometry.
    property bool fullScreen: false

    modal: true
    focus: true
    padding: Theme.space4
    topPadding: padding + (fullScreen && parent ? parent.SafeArea.margins.top : 0)
    bottomPadding: padding + (fullScreen && parent ? parent.SafeArea.margins.bottom : 0)
    x: control.fullScreen ? 0
       : parent ? Math.round(Math.max(0, (parent.width - width) / 2)) : 0
    y: control.fullScreen ? 0
       : parent ? Math.round(Math.max(0, (parent.height - height) / 2)) : 0
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

    background: PopupSurface {
        // No expensive glow, outer shadow, or rounded corners when the
        // popup fills the whole application viewport.
        neonAmbient: !control.fullScreen
        neonInteractive: !control.fullScreen
        // Keep the shared modal's previous shadow behavior unchanged.
        shadowEnabled: false
        borderWidth: control.fullScreen ? 0 : 1
        curveDepth: control.fullScreen ? 0 : Theme.curveModal
        elevation: control.fullScreen ? Theme.elevationNone : Theme.elevationModal
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: control.fullScreen ? 0 : Theme.motionStandard
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: control.fullScreen ? 0 : Theme.motionFast
            easing.type: Easing.InCubic
        }
    }
}
