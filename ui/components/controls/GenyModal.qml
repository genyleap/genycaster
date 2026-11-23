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
    // Opt-in full-viewport sheet motion for narrow layouts. Ordinary
    // desktop dialogs retain their centered fade transition.
    property bool slideFromBottom: false

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
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: control.slideFromBottom ? 1.0 : 0.0
                to: 1.0
                duration: control.slideFromBottom ? 0
                          : control.fullScreen ? 0 : Theme.motionStandard
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "y"
                from: control.slideFromBottom && control.fullScreen
                      ? control.height : control.y
                to: control.slideFromBottom && control.fullScreen
                    ? 0 : control.y
                duration: control.slideFromBottom && control.fullScreen
                          ? Theme.motionStandard : 0
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: control.slideFromBottom ? 1.0 : 0.0
                duration: control.slideFromBottom ? 0
                          : control.fullScreen ? 0 : Theme.motionFast
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "y"
                from: control.y
                to: control.slideFromBottom && control.fullScreen
                    ? control.height : control.y
                duration: control.slideFromBottom && control.fullScreen
                          ? Theme.motionStandard : 0
                easing.type: Easing.InCubic
            }
        }
    }
}
