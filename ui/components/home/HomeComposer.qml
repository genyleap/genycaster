import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

NeonSurface {
    id: root
    // GenyHub NeonCard material: a quiet rim until pointer approaches.
    interactive: true
    ambientNeon: true
    neonStrength: Theme.neonCardStrength
    externalHovered: composerHover.hovered
    externalPointerX: composerHover.point.position.x
    externalPointerY: composerHover.point.position.y

    property string avatarUrl: ""
    property string displayName: ""
    property bool interactionsAvailable: false
    property bool compact: false

    signal composeRequested()
    signal photoRequested()
    signal spaceRequested()
    signal authenticationRequested()

    implicitHeight: root.compact ? 56 : 62
    fillColor: composerHover.hovered ? Theme.surfaceRaised : Theme.surfacePrimary
    borderWidth: 1
    borderColor: composerHover.hovered ? Theme.borderStrong : Theme.borderMuted
    curveDepth: Theme.shapeSurface
    elevation: Theme.elevationNone

    function invokeCompose() {
        if (!root.interactionsAvailable) {
            root.authenticationRequested()
            return
        }
        root.composeRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        layoutDirection: Theme.uiDirection
        spacing: Theme.space2

        Icon {
            text: Theme.iconCompose
            color: composerHover.hovered ? Theme.accent : Theme.textSecondary
            preferredSize: Theme.iconSize
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.shell.whats_happening")
            alignToUi: true
            color: Theme.textPrimary
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
            elide: Text.ElideRight
        }
    }

    HoverHandler { id: composerHover; cursorShape: Qt.PointingHandCursor }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.invokeCompose()
    }

    // The home prompt is a launcher, not an editable TextArea. Right-clicking
    // it enters the real themed composer instead of a browser-native menu.
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.invokeCompose()
    }
}
