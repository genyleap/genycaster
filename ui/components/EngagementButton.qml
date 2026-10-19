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

    implicitWidth: Math.max(Theme.touchTarget, content.implicitWidth + Theme.space2 * 2)
    implicitHeight: Theme.touchTarget
    padding: 0
    // No engagement visual depends on hover. Tracking hover for three
    // controls in every recycled post only makes stationary desktop pointers
    // generate state churn while the timeline moves underneath them.
    hoverEnabled: false
    // A shaped containmentMask rejects pointer points near the edge of an
    // already-small button in Qt WebAssembly mobile Safari. The visible
    // superellipse is decorative; the full 44px target owns the interaction.
    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    scale: control.down ? 0.985 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: RowLayout {
        id: content
        layoutDirection: Theme.uiDirection
        spacing: control.metricText.length > 0 ? Theme.space1 : 0

        // RowLayout stretches all six footer cells equally; centering each
        // icon/count cluster keeps the action-to-count gap identical in
        // RTL, LTR, detail views and nested replies.
        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }

        // Give every contextual reaction a stable square viewport. Some SVGs
        // have non-square intrinsic viewBoxes; never stretch the image to fill
        // the metric row or allow its geometry to alter the action height.
        Icon {
            Layout.preferredWidth: Math.max(22, control.iconSize + 4)
            Layout.preferredHeight: Math.max(22, control.iconSize + 4)
            Layout.alignment: Qt.AlignVCenter
            clip: true
            text: control.iconGlyph
            filled: control.iconFilled
            color: control.active ? control.activeColor : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
            preferredSize: control.iconSize
        }

        Label {
            visible: control.metricText.length > 0
            text: control.metricText
            color: control.active ? control.activeColor : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            font.weight: control.active ? Theme.weightDemiBold : Theme.weightMedium
        }

        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
    }

    // Keep the complete touch target, but do not draw a permanent box around
    // every reaction. A subtle selected tint is enough; focus remains visible.
    background: Superellipse {
        anchors.fill: parent
        anchors.margins: 1
        fillColor: control.down ? Theme.surfacePressed
                   : control.active ? Qt.rgba(control.activeColor.r, control.activeColor.g,
                                              control.activeColor.b, Theme.dark ? 0.16 : 0.075)
                   : "transparent"
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : 0
        borderColor: Theme.focus
        curveDepth: Theme.curveControl
        shadowEnabled: false
    }
}
