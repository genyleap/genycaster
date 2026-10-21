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

    readonly property int frameSize: root.compact ? 62 : 66
    readonly property int avatarSize: root.compact ? 54 : 58
    readonly property real frameCurveDepth: Math.round(root.frameSize * 0.30)
    readonly property real avatarCurveDepth: Math.round(root.avatarSize * 0.30)

    implicitWidth: root.compact ? 74 : 78
    implicitHeight: root.compact ? 94 : 98

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space1

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.frameSize
            Layout.preferredHeight: root.frameSize

            // The frame and the photo now share one squircle shape. Circular
            // tri-color Shape arcs previously leaked red/yellow behind PFPs.
            Superellipse {
                anchors.fill: parent
                fillColor: root.addStory ? Theme.primarySurface : Theme.backgroundPrimary
                borderWidth: root.addStory ? 1 : (root.storyState === "active" ? 2 : 1)
                borderColor: root.addStory || root.storyState === "active"
                             ? Theme.primaryFill : Theme.borderSubtle
                exponent: 3.8
                curveDepth: root.frameCurveDepth
            }

            Avatar {
                objectName: "storyPortrait"
                visible: !root.addStory || root.avatarUrl.length > 0
                anchors.centerIn: parent
                opacity: root.storyState === "viewed" && !root.addStory ? 0.74 : 1.0
                avatarSize: root.avatarSize
                avatarCurveDepth: root.avatarCurveDepth
                avatarExponent: 3.8
                sourceUrl: root.avatarUrl
                displayName: root.displayName
            }

            // The signed-in user's own photo is the first story portrait.
            // Reserve the large plus only when the account has no PFP.
            Icon {
                visible: root.addStory && root.avatarUrl.length === 0
                anchors.centerIn: parent
                text: Theme.iconPlus
                color: Theme.accent
                preferredSize: 32
            }

            Superellipse {
                objectName: "ownStoryAddBadge"
                visible: root.addStory && root.avatarUrl.length > 0
                width: 25
                height: 25
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: -1
                anchors.bottomMargin: -1
                fillColor: Theme.primaryFill
                borderWidth: 2
                borderColor: Theme.backgroundPrimary
                curveDepth: 10

                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconPlus
                    color: Theme.inkInverse
                    preferredSize: 13
                }
            }

            Superellipse {
                visible: !root.addStory && root.storyState !== "live"
                width: 24
                height: 24
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
                        preferredSize: 13
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
            text: root.addStory ? qsTrId("genycaster.home.add_short") : root.displayName
            color: root.storyState === "viewed" && !root.addStory ? Theme.textTertiary : Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            font.weight: root.addStory ? Theme.weightMedium : Theme.weightRegular
            horizontalAlignment: Text.AlignHCenter
            elide: root.addStory ? Text.ElideNone : Text.ElideRight
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
