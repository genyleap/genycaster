import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string displayName: ""
    property string username: ""
    property string avatarUrl: ""
    property string storyState: "viewed"
    property string activityKind: "cast"
    property bool addStory: false
    property bool compact: false

    signal activated()

    readonly property color ringColor: root.storyState === "live" ? Theme.live
                                       : root.storyState === "special" ? Theme.positive
                                       : root.storyState === "viewed" ? Theme.borderDefault
                                       : Theme.story
    readonly property real ringOpacity: root.storyState === "viewed" ? 0.55
                                        : root.storyState === "partial" ? 0.72 : 1.0

    readonly property int frameSize: root.compact ? 52 : 58
    readonly property int avatarSize: root.compact ? 44 : 50
    readonly property real frameCurveDepth: Math.round(root.frameSize * 0.30)
    readonly property real avatarCurveDepth: Math.round(root.avatarSize * 0.30)

    implicitWidth: root.compact ? 62 : 68
    implicitHeight: root.compact ? 80 : 86

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space1

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.frameSize
            Layout.preferredHeight: root.frameSize

            Superellipse {
                anchors.fill: parent
                fillColor: root.addStory ? Theme.primarySurface : root.ringColor
                borderWidth: root.addStory ? 1 : 0
                borderColor: Theme.primaryFill
                opacity: root.addStory ? 1.0 : root.ringOpacity
                exponent: 3.8
                curveDepth: root.frameCurveDepth
            }

            Superellipse {
                visible: !root.addStory
                anchors.fill: parent
                anchors.margins: 2
                fillColor: Theme.backgroundPrimary
                borderWidth: 0
                exponent: 3.8
                curveDepth: Math.max(1, root.frameCurveDepth - 2)
            }

            Avatar {
                anchors.centerIn: parent
                avatarSize: root.avatarSize
                avatarCurveDepth: root.avatarCurveDepth
                avatarExponent: 3.8
                sourceUrl: root.avatarUrl
                displayName: root.displayName
            }

            Superellipse {
                visible: root.addStory
                width: 20
                height: 20
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: -1
                anchors.bottomMargin: -1
                fillColor: Theme.primaryFill
                borderWidth: 2
                borderColor: Theme.backgroundPrimary
                exponent: 2.01
                curveDepth: width / 2
                elevation: Theme.elevationControl
                shadowOpacityScale: 0.62

                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconPlus
                    color: Theme.accentInk
                    preferredSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }
            }

            Superellipse {
                visible: !root.addStory && root.storyState !== "live"
                width: 20
                height: 20
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: -2
                anchors.bottomMargin: -2
                fillColor: Theme.backgroundPrimary
                borderWidth: 2
                borderColor: Theme.backgroundPrimary
                curveDepth: 10
                elevation: Theme.elevationControl

                Superellipse {
                    anchors.fill: parent
                    anchors.margins: 2
                    fillColor: root.activityKind === "nft" ? Theme.primarySurface : Theme.surfaceRaised
                    curveDepth: 8

                    Icon {
                        anchors.centerIn: parent
                        text: root.activityKind === "reply" ? Theme.iconReply
                              : root.activityKind === "nft" ? Theme.iconPalette
                              : root.activityKind === "media" ? Theme.iconImage
                              : Theme.iconCompose
                        color: root.activityKind === "nft" ? Theme.accent : Theme.ink
                        preferredSize: 10
                    }
                }
            }

            Superellipse {
                visible: !root.addStory && root.storyState === "live"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -3
                implicitWidth: liveText.implicitWidth + Theme.space2
                implicitHeight: 17
                fillColor: Theme.live
                borderWidth: 2
                borderColor: Theme.backgroundPrimary
                curveDepth: 8

                Label {
                    id: liveText
                    anchors.centerIn: parent
                    text: qsTrId("genycaster.status.live")
                    color: Theme.inkInverse
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightBold
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space1
            Layout.rightMargin: Theme.space1
            text: root.addStory ? qsTrId("genycaster.home.add_story") : root.displayName
            color: root.storyState === "viewed" && !root.addStory ? Theme.textTertiary : Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            font.weight: root.addStory ? Theme.weightMedium : Theme.weightRegular
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    HoverHandler { id: hover }
    TapHandler {
        id: tap
        onTapped: root.activated()
    }

    scale: hover.hovered ? 1.018 : 1.0
    opacity: tap.pressed ? 0.88 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.motionFast } }
    Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
}
