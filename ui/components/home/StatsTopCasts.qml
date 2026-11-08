import QtQuick
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root
    objectName: "statsTopCasts"
    property var casts: []
    signal castRequested(string hash)

    Layout.fillWidth: true
    Layout.preferredHeight: content.implicitHeight + Theme.space4 * 2
    fillColor: Theme.surface
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space4
        spacing: Theme.space1

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.space2
            text: qsTrId("genycaster.stats.top_casts")
            color: Theme.textPrimary
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
        }

        DirectionAwareLabel {
            visible: root.casts.length === 0
            text: qsTrId("genycaster.stats.no_posts")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
        }

        Repeater {
            model: root.casts
            delegate: Item {
                id: castRow
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(64, summary.implicitHeight + Theme.space3 * 2)
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: String(modelData.text || qsTrId("genycaster.stats.open_cast"))
                Accessible.onPressAction: root.castRequested(modelData.hash)
                Keys.onReturnPressed: root.castRequested(modelData.hash)
                Keys.onSpacePressed: root.castRequested(modelData.hash)

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    DirectionAwareLabel {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: Theme.space2
                        Layout.preferredWidth: 24
                        text: String(castRow.index + 1).padStart(2, "0")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.textTertiary
                    }
                    ColumnLayout {
                        id: summary
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(castRow.modelData.text || qsTrId("genycaster.stats.media_cast"))
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            color: castHover.hovered ? Theme.accent : Theme.textPrimary
                            font.pixelSize: Theme.typeMetaSize
                            Behavior on color {
                                ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            Repeater {
                                model: [
                                    { icon: Theme.iconHeart, amount: castRow.modelData.likes },
                                    { icon: Theme.iconRepost, amount: castRow.modelData.recasts },
                                    { icon: Theme.iconComment, amount: castRow.modelData.replies }
                                ]
                                delegate: RowLayout {
                                    required property var modelData
                                    spacing: Theme.space1
                                    Icon {
                                        text: modelData.icon
                                        color: Theme.textTertiary
                                        preferredSize: Theme.iconSizeSmall
                                    }
                                    DirectionAwareLabel {
                                        text: LocaleFormat.formatInteger(Number(modelData.amount))
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    Icon {
                        Layout.alignment: Qt.AlignVCenter
                        text: Theme.iconArrowRight
                        rotation: Theme.rtl ? 180 : 0
                        color: Theme.textTertiary
                        preferredSize: Theme.iconSizeSmall
                    }
                }
                HoverHandler { id: castHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.castRequested(castRow.modelData.hash) }
                Rectangle {
                    visible: castRow.index < root.casts.length - 1
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.borderMuted
                }
            }
        }
    }
}
