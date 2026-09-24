import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var modelData
    signal openRequested(double fid)

    function ltrHandle(value) {
        const isolate = String.fromCharCode(0x2066)
        const pop = String.fromCharCode(0x2069)
        return isolate + "@" + String(value || "").replace(/^@/, "") + pop
    }

    implicitHeight: 68
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    Superellipse {
        anchors.fill: parent
        anchors.margins: 2
        fillColor: hoverHandler.hovered ? Theme.surfaceHover : "transparent"
        curveDepth: Theme.curveControl
        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        spacing: Theme.space3
        layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

        Avatar {
            avatarSize: 42
            sourceUrl: root.modelData.avatarUrl || ""
            displayName: root.modelData.displayName || root.modelData.author || ""
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.modelData.displayName || root.modelData.author || qsTrId("genycaster.profile_list.unknown")
                color: Theme.ink
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                forceLeftToRight: true
                text: root.modelData.author ? root.ltrHandle(root.modelData.author) : ""
                font.family: Theme.latinFontFamily
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }
        }
    }

    HoverHandler { id: hoverHandler }

    TapHandler {
        enabled: Number(root.modelData.fid || 0) > 0
        onTapped: root.openRequested(Number(root.modelData.fid))
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Theme.rtl ? Theme.space4 : Theme.space4 + 42 + Theme.space3
        anchors.right: parent.right
        anchors.rightMargin: Theme.rtl ? Theme.space4 + 42 + Theme.space3 : Theme.space4
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.divider
    }
}
