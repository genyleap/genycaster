import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property string iconGlyph: ""
    property string metricText: ""
    property bool active: false
    // Each reaction keeps its semantic color: likes red, reposts green.
    property color activeColor: Theme.accent
    property bool iconFilled: false
    property real iconSize: Theme.iconSizeSmall
    // A local, one-shot micro-interaction belongs to the individual action,
    // never to the shared/recycled reaction data. Network hydration and
    // delegate reuse must not replay or inherit animations.
    property bool heartFillHeld: false
    function resetReactionMotion() {
        heartHop.stop()
        subtlePulse.stop()
        heartFillHeld = false
        reactionGlyph.scale = 1
        reactionGlyph.rotation = 0
        iconLift.y = 0
    }
    function playReactionMotion(kind, activating) {
        resetReactionMotion()
        if (Theme.reduceMotion || !control.visible) return
        if (kind === "like" && activating) {
            heartFillHeld = true
            heartHop.start()
        } else {
            subtlePulse.start()
        }
    }

    implicitWidth: Math.max(Theme.touchTarget, content.implicitWidth + Theme.space2 * 2)
    implicitHeight: Theme.touchTarget
    padding: 0
    // No engagement visual depends on hover. Tracking hover for three
    // controls in every recycled post only makes stationary desktop pointers
    // generate state churn while the timeline moves underneath them.
    hoverEnabled: false
    // A shaped containmentMask rejects pointer points near the edge of an
    // already-small button in Qt WebAssembly on touch devices. The visible
    // superellipse is decorative; the full 44px target owns the interaction.
    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    scale: control.down ? 0.985 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: Item {
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight

        // Intrinsic content size must not include layout-assigned spacer
        // widths: feeding those back into the button made it grow on reflow.
        RowLayout {
            id: content
            anchors.centerIn: parent
            layoutDirection: Theme.uiDirection
            spacing: control.metricText.length > 0 ? Theme.space1 : 0

            // Give every contextual reaction a stable square viewport. Some SVGs
            // have non-square intrinsic viewBoxes; never stretch the image to fill
            // the metric row or allow its geometry to alter the action height.
            Icon {
                id: reactionGlyph
                Layout.preferredWidth: Math.max(22, control.iconSize + 4)
                Layout.preferredHeight: Math.max(22, control.iconSize + 4)
                Layout.alignment: Qt.AlignVCenter
                // The jump must not be cropped by the icon viewport.
                clip: false
                text: control.iconGlyph
                filled: control.iconFilled && !control.heartFillHeld
                color: control.active ? control.activeColor : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
                preferredSize: control.iconSize
                transform: Translate {
                    id: iconLift
                    y: 0
                }
            }

            Label {
                visible: control.metricText.length > 0
                text: control.metricText
                color: control.active ? control.activeColor : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.typeCaptionSize
                font.weight: control.active ? Theme.weightDemiBold : Theme.weightMedium
            }

        }
    }

    // The heart makes a quick upward leap, fills at its apex, then settles
    // with a gentle spring. Deliberately animate the icon only: no changes
    // to row metrics, scroll geometry, or pointer hit targets.
    SequentialAnimation {
        id: heartHop
        ParallelAnimation {
            NumberAnimation { target: reactionGlyph; property: "scale"; from: 1; to: 1.32
                              duration: 145; easing.type: Easing.OutCubic }
            NumberAnimation { target: iconLift; property: "y"; from: 0; to: -7
                              duration: 145; easing.type: Easing.OutCubic }
        }
        ScriptAction { script: control.heartFillHeld = false }
        ParallelAnimation {
            NumberAnimation { target: reactionGlyph; property: "scale"; to: 0.90
                              duration: 115; easing.type: Easing.InOutCubic }
            NumberAnimation { target: iconLift; property: "y"; to: 1
                              duration: 115; easing.type: Easing.InOutCubic }
        }
        ParallelAnimation {
            NumberAnimation { target: reactionGlyph; property: "scale"; to: 1.09
                              duration: 90; easing.type: Easing.OutCubic }
            NumberAnimation { target: iconLift; property: "y"; to: 0
                              duration: 90; easing.type: Easing.OutCubic }
        }
        NumberAnimation { target: reactionGlyph; property: "scale"; to: 1
                          duration: 75; easing.type: Easing.OutCubic }
        onStopped: {
            control.heartFillHeld = false
            reactionGlyph.scale = 1
            iconLift.y = 0
        }
    }

    // Recast, reply, quote, bookmark and Unlike get one much softer pulse.
    SequentialAnimation {
        id: subtlePulse
        ParallelAnimation {
            NumberAnimation { target: reactionGlyph; property: "scale"; from: 1; to: 1.12
                              duration: 100; easing.type: Easing.OutCubic }
            NumberAnimation { target: reactionGlyph; property: "rotation"; from: 0; to: 7
                              duration: 100; easing.type: Easing.OutCubic }
        }
        ParallelAnimation {
            NumberAnimation { target: reactionGlyph; property: "scale"; to: 1
                              duration: 150; easing.type: Easing.OutCubic }
            NumberAnimation { target: reactionGlyph; property: "rotation"; to: 0
                              duration: 150; easing.type: Easing.OutCubic }
        }
        onStopped: {
            reactionGlyph.scale = 1
            reactionGlyph.rotation = 0
        }
    }

    // No selected/pressed tile: selection is conveyed only by the
    // semantic hue of the icon and its count. Keep the full touch target and
    // the accessible keyboard-focus outline on both native Qt and WASM.
    background: Superellipse {
        anchors.fill: parent
        anchors.margins: 1
        fillColor: "transparent"
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : 0
        borderColor: Theme.focus
        curveDepth: Theme.curveControl
        shadowEnabled: false
    }
}
