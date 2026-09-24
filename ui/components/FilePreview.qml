import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Superellipse {
    id: root

    property var embed: ({})
    readonly property string targetUrl: String(root.embed.url || root.embed.value || "")
    readonly property string fileName: {
        const provided = String(root.embed.fileName || "")
        if (provided.length > 0)
            return provided
        const clean = root.targetUrl.split("?")[0].split("#")[0]
        const pieces = clean.split("/")
        const leaf = pieces.length > 0 ? pieces[pieces.length - 1] : ""
        return leaf.length > 0 ? decodeURIComponent(leaf) : String(root.embed.host || root.embed.domain || root.targetUrl)
    }
    readonly property string hostText: String(root.embed.host || root.embed.domain || "")
    readonly property string extensionText: {
        const match = /\.([a-z0-9]{1,8})$/i.exec(root.fileName)
        return match ? match[1].toUpperCase() : ""
    }

    signal openRequested(string url)

    implicitHeight: 74
    fillColor: hover.hovered ? Theme.surfaceHover : Theme.surfaceSecondary
    borderWidth: 1
    borderColor: hover.hovered ? Theme.borderDefault : Theme.borderSubtle
    curveDepth: Theme.shapeSurface

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        layoutDirection: Theme.uiDirection
        spacing: Theme.space3

        Superellipse {
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            fillColor: Theme.primarySurface
            borderWidth: 0
            curveDepth: Theme.curveControl

            Icon {
                anchors.centerIn: parent
                text: Theme.iconFile
                color: Theme.accent
                preferredSize: Theme.typeBodySize
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.fileName
                color: Theme.textPrimary
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideMiddle
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Qt.LeftToRight
                spacing: Theme.space2

                Label {
                    visible: root.extensionText.length > 0
                    text: root.extensionText
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                }

                Label {
                    Layout.fillWidth: true
                    text: root.hostText
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideMiddle
                }
            }
        }

        Icon {
            text: Theme.iconArrowUpRight
            color: Theme.textTertiary
            preferredSize: Theme.typeMetaSize
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler {
        enabled: root.targetUrl.length > 0
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.openRequested(root.targetUrl)
    }

    Accessible.role: Accessible.Link
    Accessible.name: root.fileName
    Accessible.description: qsTrId("genycaster.embed.open_safely_in_your_default_app_external")
}
