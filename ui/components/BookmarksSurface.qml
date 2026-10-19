import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root
    property var rows: []
    property bool compact: false
    property bool authenticated: false
    signal castRequested(string castId)
    signal authenticationRequested()

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: column.implicitHeight + Theme.space5 * 2

    ColumnLayout {
        id: column
        x: root.compact ? Theme.mobileGutter : Theme.contentGutter
        width: Math.max(0, root.width - 2 * x)
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space3
            spacing: Theme.space2
            Icon { text: Theme.iconBookmark; color: Theme.accent; preferredSize: Theme.typeTitle2Size }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.side_nav.bookmarks")
                color: Theme.ink
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
            }
            Label {
                text: LocaleFormat.formatInteger(root.rows.length)
                color: Theme.inkMuted
                font.pixelSize: Theme.typeMetaSize
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.side_nav.bookmarks")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeMetaSize
            wrapMode: Text.Wrap
        }

        DirectionAwareLabel {
            visible: !root.authenticated || root.rows.length === 0
            Layout.fillWidth: true
            Layout.topMargin: Theme.space6
            text: !root.authenticated
                  ? qsTrId("genycaster.notifications.sign_in_required")
                  : qsTrId("genycaster.empty.nothing_here_yet")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeBodySize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Repeater {
            model: root.rows
            delegate: Superellipse {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: savedContent.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceRaised
                borderColor: Theme.borderSubtle
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: savedContent
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Avatar {
                            avatarSize: 35
                            sourceUrl: String(modelData.avatarUrl || "")
                            displayName: String(modelData.displayName || modelData.author || "")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.displayName || modelData.author || "Farcaster")
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.author || modelData.username || "")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                        Icon {
                            text: Theme.iconArrowRight
                            color: Theme.inkMuted
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.text || "")
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
                        maximumLineCount: 5
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                    }
                }

                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: root.castRequested(String(modelData.castId || ""))
                }
            }
        }
    }
}
