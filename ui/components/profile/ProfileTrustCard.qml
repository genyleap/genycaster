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
    // The compact PRO label denotes a verified membership, not merely
    // a shortcut into the Pro explainer.
    property bool proEligible: false
    property string proStatus: "unavailable"
    property bool proChecking: false
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
    signal proRequested()

    readonly property string shownName: root.displayName.length > 0
                                        ? root.displayName : qsTrId("genycaster.side_nav.no_identity_open")
    readonly property string shownUsername: root.username.length > 0 ? root.username : "genycaster"
    readonly property real contentPadding: root.compact ? Theme.space2 : Theme.space4
    // Window resizing changes line breaking while Qt is still negotiating
    // the sidebar width. Commit a measured height after layout settles,
    // instead of bouncing the bottom account card every frame.
    property real settledCompactFaceHeight: 148
    readonly property real faceHeight: root.compact
        ? settledCompactFaceHeight
        : Math.max(frontContent.implicitHeight, backContent.implicitHeight, 200)
    onWidthChanged: compactHeightSettler.restart()
    onCompactChanged: compactHeightSettler.restart()
    onStreakDetailsExpandedChanged: compactHeightSettler.restart()
    Timer {
        id: compactHeightSettler
        interval: 180
        repeat: false
        // Compact faces are measured independently. The taller streak
        // details side must not leave an empty band under the profile actions.
        onTriggered: root.settledCompactFaceHeight = Math.max(0,
            root.streakDetailsExpanded
                ? backContent.implicitHeight : frontContent.implicitHeight)
    }
    Connections {
        target: frontContent
        function onImplicitHeightChanged() { compactHeightSettler.restart() }
    }
    Connections {
        target: backContent
        function onImplicitHeightChanged() { compactHeightSettler.restart() }
    }

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
        property bool interactive: false
        signal activated()
        Layout.minimumWidth: 0
        spacing: 0
        Accessible.role: metric.interactive ? Accessible.Button : Accessible.StaticText
        Accessible.name: metric.labelText + " " + metric.valueText
        HoverHandler {
            enabled: metric.interactive
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            enabled: metric.interactive
            onTapped: metric.activated()
        }
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
                font.pixelSize: root.compact ? Math.max(10, Theme.typeMetaSize - 1)
                                             : Theme.typeMetaSize
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
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideNone
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
            spacing: root.compact ? Theme.space1 : Theme.space3

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
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideNone
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
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 2
                        elide: Text.ElideNone
                    }

                    DirectionAwareLabel {
                        id: sidebarFidValue
                        objectName: "sidebarFidMetric"
                        visible: Number(root.fid) > 0
                        Layout.fillWidth: true
                        text: "FID " + LocaleFormat.formatInteger(root.fid)
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                        Accessible.role: Accessible.Button
                        Accessible.name: text
                        Accessible.onPressAction: root.requestProfile()
                        TapHandler { onTapped: root.requestProfile() }
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
                // The sidebar is a numeric account snapshot, not a truncated
                // biography. Full biographies remain in the profile surface.
                visible: !root.compact && root.bio.length > 0
                Layout.fillWidth: true
                text: root.bio
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
                maximumLineCount: root.compact ? 1 : 3
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

            // Account controls are full-width in the narrow sidebar. The
            // old 50/50 row ellipsized both "GenyCaster Pro" and "Profile".
            // Membership is never inferred from FID or signing readiness.
            Superellipse {
                id: proMembershipCard
                objectName: "sidebarProMembershipCard"
                Layout.fillWidth: true
                Layout.preferredHeight: root.compact ? 54 : 60
                fillColor: root.proEligible ? Theme.proSurface : Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: root.proEligible ? Theme.proBorder : Theme.borderMuted
                curveDepth: Theme.curveControl
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTrId("genycaster.pro.title")
                Accessible.description: root.proEligible
                    ? qsTrId("genycaster.pro.short")
                    : root.proStatus !== "verified"
                        ? qsTrId("genycaster.pro.verification_unavailable")
                        : qsTrId("genycaster.pro.get_pro")
                Accessible.onPressAction: root.proRequested()
                Keys.onReturnPressed: root.proRequested()
                Keys.onSpacePressed: root.proRequested()
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Icon {
                        text: Theme.iconSpark
                        color: root.proEligible ? Theme.proAccent : Theme.textSecondary
                        preferredSize: Theme.iconSize
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.title")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.proEligible ? qsTrId("genycaster.pro.short")
                                : root.proChecking ? qsTrId("genycaster.pro.check_status")
                                : root.proStatus === "verified" ? qsTrId("genycaster.pro.get_pro")
                                : qsTrId("genycaster.pro.check_status")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }
                    }
                    Icon {
                        text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                        color: root.proEligible ? Theme.proAccent : Theme.textSecondary
                        preferredSize: Theme.iconSizeSmall
                    }
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.proRequested() }
            }
            ActionButton {
                Layout.fillWidth: true
                compact: true
                quiet: true
                iconGlyph: Theme.iconProfile
                text: qsTrId("genycaster.side_nav.profile")
                onClicked: root.requestProfile()
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
            spacing: root.compact ? Theme.space1 : Theme.space3

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
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 2
                        elide: Text.ElideNone
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
                        text: qsTrId("genycaster.streak.best").arg(root.streakKnown
                            ? LocaleFormat.formatInteger(root.streakRecord) : "—")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.streak.next").arg(root.streakKnown && root.streakNext > 0
                            ? LocaleFormat.formatInteger(root.streakNext) : "—")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
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
                    iconOnly: root.compact
                    iconGlyph: Theme.iconBack
                    text: qsTrId("genycaster.profile.back_back_to_feed")
                    onClicked: root.toggleFace()
                }
                ActionButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: root.compact ? 84 : 0
                    compact: true
                    iconGlyph: Theme.iconProfile
                    balancedIcon: true
                    text: qsTrId("genycaster.side_nav.profile")
                    onClicked: root.requestProfile()
                }

            }
        }
    }
}
