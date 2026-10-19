import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property string iconGlyph: ""
    property string metricText: ""
    property bool active: false
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
            color: control.active ? Theme.accent : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
            preferredSize: control.iconSize
        }

        Label {
            visible: control.metricText.length > 0
            text: control.metricText
            color: control.active ? Theme.accent : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            font.weight: control.active ? Theme.weightDemiBold : Theme.weightMedium
        }
    }

    // Same touch-safe superellipse action chips in timelines and threads.
    // The canvas is intentionally opaque enough to separate adjacent actions
    // without expensive per-delegate shadows during WebAssembly scrolling.
    background: Superellipse {
        anchors.fill: parent
        anchors.margins: 1
        fillColor: control.active ? Theme.surfaceSelected
                   : control.down ? Theme.surfacePressed : Theme.surfaceRaised
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : Theme.strokeHairline
        borderColor: Theme.keyboardFocusVisible(control) ? Theme.focus
                     : control.active ? Theme.accent : Theme.borderSubtle
        curveDepth: Theme.curveControl
        shadowColor: Theme.keyboardFocusVisible(control) ? Theme.focusHalo : Theme.shadow
    }
}
