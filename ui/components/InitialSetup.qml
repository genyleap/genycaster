import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "initialSetup"
    focus: visible

    property bool runtimeReady: false
    property bool authenticationAvailable: false
    property string authenticationPhase: "signed-out"
    property string authenticationRoute: "openproof"
    property bool authenticationApprovalAvailable: false
    property string authenticationErrorText: ""

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal openApproval()
    signal cancelAuthentication()

    readonly property bool phone: width < 620
    readonly property bool compact: width < 860
    readonly property bool authenticationStarting: authenticationPhase === "starting"
    readonly property bool awaitingApproval: authenticationPhase === "awaiting-approval"
    readonly property bool linkingFarcaster: authenticationPhase === "linking-farcaster"
    readonly property bool authenticationInProgress:
        authenticationStarting || awaitingApproval || linkingFarcaster
    readonly property int activeStep:
        linkingFarcaster || authenticationRoute === "farcaster" ? 1 : 0
    readonly property real contentReveal: Theme.reduceMotion ? 1.0 : revealProgress
    property real revealProgress: 0.0

    function phaseTitle() {
        if (linkingFarcaster)
            return qsTrId("genycaster.shell.link_your_farcaster_account")
        if (awaitingApproval)
            return qsTrId("genycaster.shell.approve_sign_in")
        if (authenticationStarting) {
            return authenticationRoute === "farcaster"
                    ? qsTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request")
                    : qsTrId("genycaster.shell.connecting_securely_to_genyleap")
        }
        return qsTrId("genycaster.shell.sign_in_to_interact")
    }

    function phaseBody() {
        if (linkingFarcaster)
            return qsTrId("genycaster.shell.genyleap_verified_link_farcaster_to_continue")
        if (awaitingApproval) {
            return authenticationRoute === "farcaster"
                    ? qsTrId("genycaster.shell.approve_the_request_in_a_farcaster_compatible_client_t")
                    : qsTrId("genycaster.shell.approve_genyleap_sign_in_in_your_browser")
        }
        return qsTrId("genycaster.shell.you_can_keep_reading_genycaster_as_a_guest_sign_in_is_")
    }

    function restartEntrance() {
        entrance.stop()
        revealProgress = Theme.reduceMotion ? 1.0 : 0.0
        if (!Theme.reduceMotion)
            entrance.start()
    }

    onVisibleChanged: {
        if (visible) {
            forceActiveFocus()
            restartEntrance()
        }
    }

    Component.onCompleted: {
        if (visible)
            restartEntrance()
    }

    Keys.onEscapePressed: event => {
        if (root.authenticationInProgress)
            root.cancelAuthentication()
        event.accepted = true
    }

    NumberAnimation {
        id: entrance
        target: root
        property: "revealProgress"
        from: 0.0
        to: 1.0
        duration: 620
        easing.type: Easing.OutCubic
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // Slow ambient shapes make the blocking setup screen feel alive without
    // competing with the authentication controls.
    Superellipse {
        id: ambientTop
        width: Math.max(root.width * (root.compact ? 0.92 : 0.58), root.compact ? 360 : 520)
        height: width
        curveDepth: width / 2
        x: root.width - width * (root.compact ? 0.66 : 0.52)
        y: -height * (root.compact ? 0.58 : 0.46)
        fillColor: Theme.primarySurface
        borderWidth: 0
        opacity: Theme.dark ? 0.54 : 0.76

        SequentialAnimation on x {
            running: root.visible && !Theme.reduceMotion
            loops: Animation.Infinite
            NumberAnimation {
                to: root.width - ambientTop.width * (root.compact ? 0.70 : 0.56)
                duration: 7600
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                to: root.width - ambientTop.width * (root.compact ? 0.66 : 0.52)
                duration: 7600
                easing.type: Easing.InOutSine
            }
        }
    }

    Superellipse {
        id: ambientBottom
        width: Math.max(root.width * (root.compact ? 0.78 : 0.38), root.compact ? 300 : 360)
        height: width
        curveDepth: width / 2
        x: -width * 0.46
        y: root.height - height * 0.48
        fillColor: Theme.primarySurface
        borderWidth: 0
        opacity: Theme.dark ? 0.32 : 0.48

        SequentialAnimation on y {
            running: root.visible && !Theme.reduceMotion
            loops: Animation.Infinite
            NumberAnimation {
                to: root.height - ambientBottom.height * 0.54
                duration: 8800
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                to: root.height - ambientBottom.height * 0.48
                duration: 8800
                easing.type: Easing.InOutSine
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    ScrollView {
        id: setupScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
        ScrollBar.vertical: GenyScrollBar {
            policy: setupScroll.contentHeight > setupScroll.availableHeight
                    ? ScrollBar.AsNeeded
                    : ScrollBar.AlwaysOff
        }

        Item {
            id: scrollSurface
            width: setupScroll.availableWidth
            implicitHeight: Math.max(
                setupScroll.availableHeight,
                setupColumn.implicitHeight + (root.phone ? Theme.space5 : Theme.space8) * 2)
            height: implicitHeight

            ColumnLayout {
                id: setupColumn
                width: Math.min(
                    root.compact ? 720 : 1120,
                    Math.max(300, scrollSurface.width - (root.phone ? Theme.space4 : Theme.space7) * 2))
                x: (scrollSurface.width - width) / 2
                y: Math.max(
                    root.phone ? Theme.space4 : Theme.space6,
                    (scrollSurface.height - implicitHeight) / 2)
                spacing: root.phone ? Theme.space5 : Theme.space6

                Item {
                    id: brandReveal
                    Layout.fillWidth: true
                    implicitHeight: brandRow.implicitHeight
                    opacity: Math.max(0, Math.min(1, root.contentReveal / 0.34))
                    transform: Translate {
                        y: (1.0 - brandReveal.opacity) * 14
                    }

                    RowLayout {
                        id: brandRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space3

                        Superellipse {
                            Layout.preferredWidth: root.phone ? 44 : 50
                            Layout.preferredHeight: Layout.preferredWidth
                            curveDepth: Theme.curveControl
                            fillColor: Theme.primaryFill
                            borderWidth: 0
                            elevation: Theme.elevationControl

                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconLeaf
                                color: Theme.accentInk
                                preferredSize: root.phone ? 22 : 25
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.app.genycaster")
                                color: Theme.ink
                                font.pixelSize: root.phone ? Theme.typeTitle3Size : Theme.typeTitle2Size
                                font.weight: Theme.weightBold
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.verified_by_openproof")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }
                    }
                }

                GridLayout {
                    id: setupGrid
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    columns: root.compact ? 1 : 2
                    columnSpacing: root.compact ? 0 : Theme.space8
                    rowSpacing: root.phone ? Theme.space5 : Theme.space6

                    ColumnLayout {
                        id: introColumn
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.maximumWidth: root.compact ? 100000 : 520
                        spacing: root.phone ? Theme.space4 : Theme.space5
                        opacity: Math.max(0, Math.min(1, (root.contentReveal - 0.10) / 0.42))
                        transform: Translate {
                            y: (1.0 - introColumn.opacity) * 18
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.setup.title")
                                color: Theme.ink
                                font.pixelSize: root.phone
                                                ? Theme.typeTitle1Size
                                                : root.compact
                                                  ? Theme.typeDisplaySize
                                                  : Math.round(Theme.typeDisplaySize * 1.18)
                                font.weight: Theme.weightBold
                                lineHeight: 0.98
                                wrapMode: Text.Wrap
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.setup.subtitle")
                                color: Theme.inkMuted
                                font.pixelSize: root.phone ? Theme.typeBodySize : Theme.typeTitle3Size
                                lineHeight: 1.42
                                wrapMode: Text.Wrap
                            }
                        }

                        Superellipse {
                            Layout.fillWidth: true
                            implicitHeight: stepsColumn.implicitHeight + Theme.space4 * 2
                            curveDepth: Theme.curveLarge
                            fillColor: Theme.surface
                            borderWidth: Theme.strokeHairline
                            borderColor: Theme.borderMuted
                            opacity: 0.96

                            ColumnLayout {
                                id: stepsColumn
                                anchors.fill: parent
                                anchors.margins: Theme.space4
                                spacing: 0

                                Repeater {
                                    model: [
                                        {
                                            "title": qsTrId("genycaster.shell.verified_by_openproof"),
                                            "body": qsTrId("genycaster.shell.openproof_account_farcaster_protocol_identity"),
                                            "icon": Theme.iconCheck
                                        },
                                        {
                                            "title": qsTrId("genycaster.shell.link_your_farcaster_account"),
                                            "body": qsTrId("genycaster.shell.genyleap_verified_link_farcaster_to_continue"),
                                            "icon": Theme.iconLink
                                        }
                                    ]

                                    delegate: Item {
                                        id: stepItem
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        implicitHeight: stepRow.implicitHeight + Theme.space3 * 2
                                        readonly property bool active: root.activeStep === index
                                        readonly property bool complete: root.activeStep > index

                                        RowLayout {
                                            id: stepRow
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space3

                                            Item {
                                                Layout.preferredWidth: 42
                                                Layout.preferredHeight: 42

                                                Superellipse {
                                                    id: stepPulse
                                                    anchors.centerIn: parent
                                                    width: 42
                                                    height: 42
                                                    curveDepth: 21
                                                    fillColor: stepItem.active
                                                               ? Theme.primarySurface
                                                               : Theme.surfaceMuted
                                                    borderWidth: stepItem.active ? Theme.strokeStandard : 0
                                                    borderColor: stepItem.active ? Theme.accent : Theme.borderMuted
                                                    Behavior on fillColor {
                                                        ColorAnimation { duration: Theme.motionStandard }
                                                    }
                                                    Behavior on borderColor {
                                                        ColorAnimation { duration: Theme.motionStandard }
                                                    }

                                                    SequentialAnimation on scale {
                                                        running: root.visible
                                                                 && root.authenticationInProgress
                                                                 && stepItem.active
                                                                 && !Theme.reduceMotion
                                                        loops: Animation.Infinite
                                                        NumberAnimation {
                                                            to: 1.06
                                                            duration: 780
                                                            easing.type: Easing.InOutSine
                                                        }
                                                        NumberAnimation {
                                                            to: 1.0
                                                            duration: 780
                                                            easing.type: Easing.InOutSine
                                                        }
                                                    }

                                                    Icon {
                                                        anchors.centerIn: parent
                                                        text: stepItem.complete ? Theme.iconCheck : stepItem.modelData.icon
                                                        color: stepItem.active || stepItem.complete
                                                               ? Theme.accent
                                                               : Theme.inkMuted
                                                        preferredSize: 18
                                                    }
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.space1

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: stepItem.modelData.title
                                                    color: stepItem.active ? Theme.ink : Theme.inkMuted
                                                    font.pixelSize: Theme.typeBodySize
                                                    font.weight: stepItem.active
                                                                 ? Theme.weightDemiBold
                                                                 : Theme.weightMedium
                                                    wrapMode: Text.Wrap
                                                }

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: stepItem.modelData.body
                                                    color: Theme.inkSubtle
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    lineHeight: 1.34
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }

                                        Rectangle {
                                            visible: index === 0
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            anchors.leftMargin: 54
                                            height: 1
                                            color: Theme.divider
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: cardReveal
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        implicitHeight: authCard.implicitHeight
                        opacity: Math.max(0, Math.min(1, (root.contentReveal - 0.24) / 0.44))
                        transform: Translate {
                            y: (1.0 - cardReveal.opacity) * 22
                        }

                        Superellipse {
                            id: authCard
                            width: parent.width
                            implicitHeight: cardContent.implicitHeight + (root.phone ? Theme.space4 : Theme.space5) * 2
                            Behavior on implicitHeight {
                                NumberAnimation {
                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                    easing.type: Easing.OutCubic
                                }
                            }
                            curveDepth: Theme.curveModal
                            fillColor: Theme.surfaceRaised
                            borderWidth: Theme.strokeHairline
                            borderColor: Theme.border
                            elevation: Theme.elevationModal
                            shadowEnabled: true

                            ColumnLayout {
                                id: cardContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: root.phone ? Theme.space4 : Theme.space5
                                spacing: root.phone ? Theme.space3 : Theme.space4

                                RowLayout {
                                    Layout.fillWidth: true
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space3

                                    Item {
                                        Layout.preferredWidth: 44
                                        Layout.preferredHeight: 44

                                        Superellipse {
                                            anchors.centerIn: parent
                                            width: 44
                                            height: 44
                                            curveDepth: Theme.curveControl
                                            fillColor: Theme.primarySurface
                                            borderWidth: 0

                                            Icon {
                                                anchors.centerIn: parent
                                                text: root.authenticationInProgress
                                                      ? Theme.iconEnergy
                                                      : Theme.iconSignIn
                                                color: Theme.accent
                                                preferredSize: 19
                                            }
                                        }

                                        Superellipse {
                                            id: statusHalo
                                            visible: root.authenticationInProgress
                                            anchors.centerIn: parent
                                            width: 52
                                            height: 52
                                            curveDepth: 26
                                            fillColor: "transparent"
                                            borderWidth: Theme.strokeHairline
                                            borderColor: Theme.accent
                                            opacity: 0.34

                                            SequentialAnimation on opacity {
                                                running: statusHalo.visible && !Theme.reduceMotion
                                                loops: Animation.Infinite
                                                NumberAnimation { to: 0.10; duration: 720; easing.type: Easing.InOutSine }
                                                NumberAnimation { to: 0.42; duration: 720; easing.type: Easing.InOutSine }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space1

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: root.phaseTitle()
                                            color: Theme.ink
                                            font.pixelSize: root.phone
                                                            ? Theme.typeTitle3Size
                                                            : Theme.typeTitle2Size
                                            font.weight: Theme.weightBold
                                            wrapMode: Text.Wrap
                                        }

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: root.authenticationInProgress
                                                  ? (root.authenticationRoute === "farcaster"
                                                     ? qsTrId("genycaster.shell.sign_in_with_farcaster")
                                                     : qsTrId("genycaster.shell.verified_by_openproof"))
                                                  : qsTrId("genycaster.shell.verified_by_openproof")
                                            color: Theme.inkSubtle
                                            font.pixelSize: Theme.typeCaptionSize
                                            font.weight: Theme.weightDemiBold
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: root.phaseBody()
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeBodySize
                                    lineHeight: 1.42
                                    wrapMode: Text.Wrap
                                }

                                Superellipse {
                                    visible: root.runtimeReady && !root.authenticationAvailable
                                    Layout.fillWidth: true
                                    implicitHeight: setupUnavailable.implicitHeight + Theme.space3 * 2
                                    curveDepth: Theme.curveCompact
                                    fillColor: Theme.warningSurface
                                    borderWidth: 0

                                    DirectionAwareLabel {
                                        id: setupUnavailable
                                        anchors.fill: parent
                                        anchors.margins: Theme.space3
                                        text: qsTrId("genycaster.shell.sign_in_is_not_configured_in_this_build_guest_reading_")
                                        color: Theme.warning
                                        font.pixelSize: Theme.typeCaptionSize
                                        lineHeight: 1.3
                                        wrapMode: Text.Wrap
                                    }
                                }

                                Superellipse {
                                    visible: root.authenticationErrorText.length > 0
                                    Layout.fillWidth: true
                                    implicitHeight: setupError.implicitHeight + Theme.space3 * 2
                                    curveDepth: Theme.curveCompact
                                    fillColor: Theme.warningSurface
                                    borderWidth: 0

                                    DirectionAwareLabel {
                                        id: setupError
                                        anchors.fill: parent
                                        anchors.margins: Theme.space3
                                        text: root.authenticationErrorText
                                        color: Theme.warning
                                        font.pixelSize: Theme.typeCaptionSize
                                        lineHeight: 1.3
                                        wrapMode: Text.Wrap
                                    }
                                }

                                RowLayout {
                                    visible: root.authenticationStarting
                                    Layout.fillWidth: true
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space2

                                    GenySpinner {
                                        Layout.preferredWidth: 22
                                        Layout.preferredHeight: 22
                                    }

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: root.authenticationRoute === "farcaster"
                                              ? qsTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request")
                                              : qsTrId("genycaster.shell.connecting_securely_to_genyleap")
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeMetaSize
                                        wrapMode: Text.Wrap
                                    }
                                }

                                ColumnLayout {
                                    visible: root.awaitingApproval || root.linkingFarcaster
                                    Layout.fillWidth: true
                                    spacing: Theme.space2

                                    ActionButton {
                                        Layout.fillWidth: true
                                        text: root.linkingFarcaster || root.authenticationRoute === "farcaster"
                                              ? qsTrId("genycaster.shell.open_farcaster")
                                              : qsTrId("genycaster.shell.open_genyleap")
                                        prominent: true
                                        iconGlyph: Theme.iconArrowUpRight
                                        enabled: root.authenticationApprovalAvailable
                                        onClicked: root.openApproval()
                                    }

                                    ActionButton {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.shell.cancel")
                                        quiet: true
                                        onClicked: root.cancelAuthentication()
                                    }
                                }

                                ColumnLayout {
                                    visible: !root.authenticationInProgress
                                    Layout.fillWidth: true
                                    spacing: Theme.space2

                                    ActionButton {
                                        id: genyleapButton
                                        objectName: "initialSetupGenyleapButton"
                                        Layout.fillWidth: true
                                        implicitHeight: root.phone
                                                        ? Theme.controlHeight
                                                        : Theme.controlHeight + 4
                                        text: qsTrId("genycaster.shell.continue_with_genyleap")
                                        prominent: true
                                        iconGlyph: Theme.iconSignIn
                                        enabled: root.runtimeReady && root.authenticationAvailable
                                        busy: root.authenticationStarting
                                        onClicked: root.continueWithGenyleap()
                                    }

                                    ActionButton {
                                        objectName: "initialSetupFarcasterButton"
                                        Layout.fillWidth: true
                                        implicitHeight: root.phone
                                                        ? Theme.controlHeight
                                                        : Theme.controlHeight + 4
                                        text: qsTrId("genycaster.shell.sign_in_with_farcaster")
                                        iconGlyph: Theme.iconLink
                                        enabled: root.runtimeReady && root.authenticationAvailable
                                        onClicked: root.continueWithFarcaster()
                                    }
                                }

                                Rectangle {
                                    visible: !root.authenticationInProgress
                                    Layout.fillWidth: true
                                    Layout.topMargin: Theme.space1
                                    height: 1
                                    color: Theme.divider
                                }

                                RowLayout {
                                    visible: !root.authenticationInProgress
                                    Layout.fillWidth: true
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space2

                                    Icon {
                                        text: Theme.iconCheck
                                        color: Theme.accent
                                        preferredSize: 15
                                    }

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.shell.openproof_account_farcaster_protocol_identity")
                                        color: Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                        lineHeight: 1.32
                                        wrapMode: Text.Wrap
                                    }
                                }

                                DirectionAwareLabel {
                                    visible: !root.runtimeReady
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.bridge.starting_local_read_runtime")
                                    color: Theme.inkSubtle
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                DirectionAwareLabel {
                    id: footerReveal
                    Layout.fillWidth: true
                    Layout.bottomMargin: root.phone ? Theme.space2 : Theme.space3
                    text: qsTrId("genycaster.app.genycaster")
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Text.AlignHCenter
                    opacity: Math.max(0, Math.min(1, (root.contentReveal - 0.58) / 0.42))
                }
            }
        }
    }
}
