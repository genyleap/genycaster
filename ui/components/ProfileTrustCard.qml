import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root

    property bool authenticated: false
    property double fid: 0
    property string displayName: ""
    property string username: ""
    property string avatarUrl: ""
    property string bio: ""
    property string location: ""
    property bool verified: false
    property int following: -1
    property int followers: -1
    property var streakSnapshot: ({})
    readonly property bool streakKnown: streakSnapshot.time_zone === "UTC"
    readonly property int streakCurrent: streakKnown
        ? Math.max(0,Number(streakSnapshot.activity?.current || 0)) : 0
    readonly property int streakRecord: streakKnown
        ? Math.max(0,Number(streakSnapshot.activity?.longest || 0)) : 0
    readonly property int streakNext: streakKnown
        ? Math.max(0,Number(streakSnapshot.activity?.next_milestone || 0)) : 0
    readonly property bool streakActive: streakKnown && streakCurrent > 0
    property bool compact: false
    property bool loading: false
    property bool streakDetailsExpanded: false
    property real flipAngle: streakDetailsExpanded ? 180 : 0

    signal profileRequested()
    signal accountRequested()

    readonly property string shownName: root.displayName.length > 0
                                        ? root.displayName : qsTrId("genycaster.side_nav.no_identity_open")
    readonly property string shownUsername: root.username.length > 0 ? root.username : "genycaster"
    readonly property real contentPadding: root.compact ? Theme.space3 : Theme.space4
    readonly property real faceHeight: Math.max(frontContent.implicitHeight, backContent.implicitHeight, root.compact ? 194 : 218)

    function requestProfile() {
        if (root.authenticated)
            root.profileRequested()
        else
            root.accountRequested()
    }

    function toggleFace() {
        root.streakDetailsExpanded = !root.streakDetailsExpanded
    }

    fillColor: root.compact ? Theme.surfaceMuted : Theme.contextRailCard
    borderWidth: Theme.strokeHairline
    borderColor: Theme.borderSubtle
    curveDepth: Theme.curveModal
    implicitHeight: root.faceHeight + root.contentPadding * 2
    clip: true

    Behavior on flipAngle {
        NumberAnimation {
            duration: Theme.reduceMotion ? 0 : Math.max(Theme.motionStandard, 360)
            easing.type: Easing.InOutCubic
        }
    }

    component Metric: ColumnLayout {
        id: metric
        property string valueText: "—"
        property string labelText: ""
        property string iconGlyph: ""
        property color valueColor: Theme.textPrimary
        spacing: 0
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.space1
            Icon {
                visible: metric.iconGlyph.length > 0
                text: metric.iconGlyph
                preferredSize: Theme.scaleUi(14)
                color: metric.valueColor
            }
            DirectionAwareLabel {
                text: metric.valueText
                color: metric.valueColor
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightBold
                horizontalAlignment: Text.AlignHCenter
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: parent.labelText
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    Item {
        id: frontFace
        anchors.fill: parent
        anchors.margins: root.contentPadding
        visible: root.flipAngle <= 90
        opacity: Math.max(0, 1 - root.flipAngle / 95)
        layer.enabled: !Theme.reduceMotion && root.flipAngle > 0 && root.flipAngle < 180
        layer.smooth: true
        layer.samples: renderingController.layerSampleCount
        layer.mipmap: false
        transform: Rotation {
            origin.x: frontFace.width / 2
            origin.y: frontFace.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: root.flipAngle
        }

        ColumnLayout {
            id: frontContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: root.compact ? Theme.space2 : Theme.space3

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Avatar {
                    avatarSize: root.compact ? 46 : 54
                    sourceUrl: root.avatarUrl
                    displayName: root.shownName
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space1

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.shownName
                            color: Theme.textPrimary
                            font.pixelSize: root.compact ? Theme.typeLabelSize : Theme.typeTitle3Size
                            font.weight: Theme.weightBold
                            elide: Text.ElideRight
                        }
                        Icon {
                            visible: root.verified
                            text: Theme.iconCheck
                            color: Theme.verified
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        LayoutMirroring.enabled: false
                        text: "@" + root.shownUsername
                        color: Theme.textTertiary
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Theme.rtl ? Text.AlignRight : Text.AlignLeft
                        elide: Text.ElideRight
                    }

                    DirectionAwareLabel {
                        visible: root.location.length > 0 && !root.compact
                        Layout.fillWidth: true
                        text: root.location
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }

                Item {
                    id: streakRing
                    objectName: "sidebarStreakRing"
                    Layout.preferredWidth: root.compact ? 46 : 54
                    Layout.preferredHeight: Layout.preferredWidth

                    ProgressRing {
                        anchors.fill: parent
                        progress: root.streakActive && root.streakNext > 0
                            ? Math.min(1, root.streakCurrent/root.streakNext) : 0
                        strokeWidth: root.compact ? 3.5 : 4
                        inset: 3
                        trackColor: Theme.borderMuted
                        progressColor: root.streakActive ? Theme.streak : Theme.inkMuted
                    }
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconStreakFlame
                        preferredSize: Theme.scaleUi(22)
                        color: root.streakActive ? Theme.streak : Theme.inkMuted
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.toggleFace() }
                }
            }

            DirectionAwareLabel {
                visible: root.bio.length > 0
                Layout.fillWidth: true
                text: root.bio
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
                maximumLineCount: root.compact ? 2 : 3
                elide: Text.ElideRight
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: root.compact ? 54 : 58
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveControl

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space2
                    anchors.rightMargin: Theme.space2
                    layoutDirection: Theme.uiDirection
                    spacing: 0

                    Metric {
                        Layout.fillWidth: true
                        valueText: root.following >= 0 ? LocaleFormat.formatCompactNumber(root.following) : "—"
                        labelText: qsTrId("genycaster.shell.following")
                    }
                    Rectangle { Layout.preferredWidth: Theme.strokeHairline; Layout.preferredHeight: 28; color: Theme.divider }
                    Metric {
                        Layout.fillWidth: true
                        valueText: root.followers >= 0 ? LocaleFormat.formatCompactNumber(root.followers) : "—"
                        labelText: qsTrId("genycaster.shell.followers")
                    }
                    Rectangle { Layout.preferredWidth: Theme.strokeHairline; Layout.preferredHeight: 28; color: Theme.divider }
                    Metric {
                        objectName: "sidebarStreakMetric"
                        Layout.fillWidth: true
                        valueText: root.streakKnown
                            ? LocaleFormat.formatInteger(root.streakCurrent) : "—"
                        labelText: qsTrId("genycaster.streak.title")
                        iconGlyph: Theme.iconStreakFlame
                        valueColor: root.streakActive ? Theme.streak : Theme.inkMuted
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconStreakFlame
                    balancedIcon: true
                    text: qsTrId("genycaster.streak.title")
                    onClicked: root.toggleFace()
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    iconGlyph: Theme.iconProfile
                    balancedIcon: true
                    text: qsTrId("genycaster.side_nav.profile")
                    onClicked: root.requestProfile()
                }
            }

            ActionButton {
                objectName: "profileTrustCardAccountButton"
                Layout.fillWidth: true
                compact: true
                quiet: true
                text: qsTrId("genycaster.header.account")
                onClicked: root.accountRequested()
            }
        }
    }

    Item {
        id: backFace
        anchors.fill: parent
        anchors.margins: root.contentPadding
        visible: root.flipAngle >= 90
        opacity: Math.max(0, (root.flipAngle - 85) / 95)
        layer.enabled: !Theme.reduceMotion && root.flipAngle > 0 && root.flipAngle < 180
        layer.smooth: true
        layer.samples: renderingController.layerSampleCount
        layer.mipmap: false
        transform: Rotation {
            origin.x: backFace.width / 2
            origin.y: backFace.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: root.flipAngle - 180
        }

        ColumnLayout {
            id: backContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: root.compact ? Theme.space2 : Theme.space3

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Superellipse {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    fillColor: Theme.primarySurface
                    curveDepth: Theme.curveControl
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconStreakFlame
                        color: root.streakActive ? Theme.streak : Theme.inkMuted
                        preferredSize: Theme.iconSizeLarge
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.streak.title")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightBold
                    }
                    Label {
                        Layout.fillWidth: true
                        LayoutMirroring.enabled: false
                        text: "@" + root.shownUsername
                        color: Theme.textTertiary
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }

                DirectionAwareLabel {
                    text: root.streakKnown
                        ? LocaleFormat.formatInteger(root.streakCurrent) : "—"
                    color: root.streakActive ? Theme.streak : Theme.textTertiary
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: streakDetails.implicitHeight + Theme.space3 * 2
                fillColor: Theme.primarySurface
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveCompact

                ColumnLayout {
                    id: streakDetails
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.streak.activity_rule")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.streak.best")
                            .arg(root.streakKnown
                                ? LocaleFormat.formatInteger(root.streakRecord) : "—")
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Metric {
                    Layout.fillWidth: true
                    valueText: root.followers >= 0 ? LocaleFormat.formatCompactNumber(root.followers) : "—"
                    labelText: qsTrId("genycaster.shell.followers")
                }
                Metric {
                    Layout.fillWidth: true
                    valueText: root.following >= 0 ? LocaleFormat.formatCompactNumber(root.following) : "—"
                    labelText: qsTrId("genycaster.shell.following")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconBack
                    text: qsTrId("genycaster.profile.back_back_to_feed")
                    onClicked: root.toggleFace()
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    iconGlyph: Theme.iconProfile
                    balancedIcon: true
                    text: qsTrId("genycaster.side_nav.profile")
                    onClicked: root.requestProfile()
                }
            }

            ActionButton {
                Layout.fillWidth: true
                compact: true
                quiet: true
                text: qsTrId("genycaster.header.account")
                onClicked: root.accountRequested()
            }
        }
    }
}
