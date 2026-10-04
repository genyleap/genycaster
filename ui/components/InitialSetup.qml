import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "initialSetup"
    focus: visible

    property bool runtimeReady: false
    property bool authenticationAvailable: false
    property bool authenticated: false
    property bool setupCompleted: false
    property bool legalConsentAccepted: false
    property string authenticationPhase: "signed-out"
    property string authenticationRoute: "openproof"
    property bool authenticationApprovalAvailable: false
    property string authenticationErrorText: ""
    property string accountName: ""
    property string accountUsername: ""
    property string accountAvatarUrl: ""
    property bool legalReadToEnd: false
    property bool legalTermsChecked: false
    property bool legalPrivacyChecked: false

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal openApproval()
    signal cancelAuthentication()
    signal acceptLegalConsent(bool termsAccepted, bool privacyAcknowledged, bool reviewCompleted)
    signal finishSetup()

    readonly property bool phone: width < 680
    readonly property bool compact: width < 980
    readonly property bool shortViewport: !phone && height < 720
    readonly property bool returningSignIn: setupCompleted && !authenticated
    readonly property bool authenticationStarting: authenticationPhase === "starting"
    readonly property bool awaitingApproval: authenticationPhase === "awaiting-approval"
    readonly property bool linkingFarcaster: authenticationPhase === "linking-farcaster"
    readonly property bool authenticationInProgress:
        authenticationStarting || awaitingApproval || linkingFarcaster
    readonly property var wizardSteps: returningSignIn ? [2] : [0, 1, 2, 3, 4, 5]
    readonly property int wizardStepCount: wizardSteps.length
    readonly property bool legalGateSatisfied:
        legalConsentAccepted || (legalReadToEnd && legalTermsChecked && legalPrivacyChecked)
    readonly property int preferredDesktopHeight:
        returningSignIn ? 620
                        : (currentStep === 1 || currentStep === 3 || currentStep === 4 ? 760 : 680)
    property int currentStep: returningSignIn ? 2 : 0
    property real stageReveal: 1.0
    property real shellReveal: 0.0

    function stepTitle(index) {
        switch (index) {
        case 0: return qsTrId("genycaster.setup.welcome")
        case 1: return qsTrId("genycaster.setup.privacy_terms")
        case 2: return qsTrId("genycaster.header.account")
        case 3: return qsTrId("genycaster.settings.appearance")
        case 4: return qsTrId("genycaster.settings.language_region")
        case 5: return qsTrId("genycaster.status.ready")
        default: return ""
        }
    }

    function stepIcon(index) {
        switch (index) {
        case 0: return Theme.iconHome
        case 1: return Theme.iconDocument
        case 2: return Theme.iconUser
        case 3: return Theme.iconPalette
        case 4: return Theme.iconLanguage
        case 5: return Theme.iconCheck
        default: return Theme.iconCheck
        }
    }

    function indexFor(model, selectedId) {
        for (let index = 0; index < model.length; ++index) {
            if (String(model[index].id) === String(selectedId))
                return index
        }
        return 0
    }

    function switchStep(index) {
        if (returningSignIn)
            index = 2
        index = Math.max(0, Math.min(5, index))
        if (!legalConsentAccepted && index > 1)
            index = 1
        if (!authenticated && index > 2)
            index = 2
        if (index === currentStep) {
            stageReveal = 1.0
            return
        }
        currentStep = index
        if (Theme.reduceMotion) {
            stageReveal = 1.0
        } else {
            stageReveal = 0.0
            stageEntrance.restart()
        }
    }

    function synchronizeFlow() {
        if (returningSignIn) {
            switchStep(2)
            return
        }
        if (!legalConsentAccepted && currentStep > 1) {
            switchStep(1)
            return
        }
        if (!authenticated && currentStep > 2)
            switchStep(2)
    }

    function nextStep() {
        if (returningSignIn)
            return

        if (currentStep === 0) {
            switchStep(legalConsentAccepted ? 2 : 1)
            return
        }
        if (currentStep === 1) {
            if (!legalGateSatisfied)
                return
            if (!legalConsentAccepted)
                acceptLegalConsent(legalTermsChecked, legalPrivacyChecked, legalReadToEnd)
            Qt.callLater(() => switchStep(2))
            return
        }
        if (currentStep === 2) {
            if (authenticated)
                switchStep(3)
            return
        }
        if (currentStep < 5) {
            switchStep(currentStep + 1)
            return
        }
        finishSetup()
    }

    function previousStep() {
        if (returningSignIn || currentStep <= 0)
            return
        switchStep(currentStep - 1)
    }

    function primaryActionText() {
        if (currentStep === 0)
            return qsTrId("genycaster.setup.continue")
        if (currentStep === 1)
            return qsTrId("genycaster.setup.agree_continue")
        if (currentStep === 2)
            return authenticated
                    ? qsTrId("genycaster.setup.continue")
                    : qsTrId("genycaster.setup.sign_in_required")
        if (currentStep === 5)
            return qsTrId("genycaster.setup.finish")
        return qsTrId("genycaster.setup.continue")
    }

    function primaryActionEnabled() {
        if (authenticationInProgress)
            return false
        if (currentStep === 1)
            return legalGateSatisfied
        if (currentStep === 2)
            return authenticated
        return true
    }

    function phaseTitle() {
        if (authenticated)
            return qsTrId("genycaster.setup.account_verified")
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
        if (authenticated)
            return qsTrId("genycaster.setup.account_verified_body")
        if (linkingFarcaster)
            return qsTrId("genycaster.shell.genyleap_verified_link_farcaster_to_continue")
        if (awaitingApproval) {
            return authenticationRoute === "farcaster"
                    ? qsTrId("genycaster.shell.approve_the_request_in_a_farcaster_compatible_client_t")
                    : qsTrId("genycaster.shell.approve_genyleap_sign_in_in_your_browser")
        }
        return qsTrId("genycaster.setup.subtitle")
    }

    onAuthenticatedChanged: Qt.callLater(synchronizeFlow)
    onSetupCompletedChanged: Qt.callLater(synchronizeFlow)
    onLegalConsentAcceptedChanged: {
        if (legalConsentAccepted) {
            legalReadToEnd = true
            legalTermsChecked = true
            legalPrivacyChecked = true
        }
        Qt.callLater(synchronizeFlow)
    }
    onVisibleChanged: {
        if (!visible)
            return
        if (legalConsentAccepted) {
            legalReadToEnd = true
            legalTermsChecked = true
            legalPrivacyChecked = true
        }
        forceActiveFocus()
        synchronizeFlow()
        shellReveal = Theme.reduceMotion ? 1.0 : 0.0
        if (!Theme.reduceMotion)
            shellEntrance.restart()
    }

    Component.onCompleted: {
        synchronizeFlow()
        shellReveal = Theme.reduceMotion ? 1.0 : 0.0
        if (visible && !Theme.reduceMotion)
            shellEntrance.start()
    }

    Keys.onEscapePressed: event => {
        if (authenticationInProgress)
            cancelAuthentication()
        event.accepted = true
    }

    NumberAnimation {
        id: shellEntrance
        target: root
        property: "shellReveal"
        from: 0.0
        to: 1.0
        duration: 520
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: stageEntrance
        target: root
        property: "stageReveal"
        from: 0.0
        to: 1.0
        duration: 300
        easing.type: Easing.OutCubic
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    Superellipse {
        id: ambientTop
        width: Math.max(root.width * 0.42, 420)
        height: width
        x: root.width - width * 0.64
        y: -height * 0.52
        curveDepth: width / 2
        fillColor: Theme.primarySurface
        borderWidth: 0
        opacity: Theme.dark ? 0.34 : 0.58

        SequentialAnimation on x {
            running: root.visible && !Theme.reduceMotion
            loops: Animation.Infinite
            NumberAnimation {
                to: root.width - ambientTop.width * 0.69
                duration: 9000
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                to: root.width - ambientTop.width * 0.64
                duration: 9000
                easing.type: Easing.InOutSine
            }
        }
    }

    Superellipse {
        width: Math.max(root.width * 0.30, 340)
        height: width
        x: -width * 0.48
        y: root.height - height * 0.40
        curveDepth: width / 2
        fillColor: Theme.primarySurface
        borderWidth: 0
        opacity: Theme.dark ? 0.20 : 0.34
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    Superellipse {
        id: setupShell
        anchors.centerIn: parent
        width: root.phone ? root.width : Math.min(root.width - 64, 1320)
        height: root.phone ? root.height : Math.min(root.height - 64, root.preferredDesktopHeight)
        Behavior on height {
            NumberAnimation {
                duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                easing.type: Easing.OutCubic
            }
        }
        curveDepth: root.phone ? 0 : Theme.curveModal
        fillColor: Theme.surfaceRaised
        borderWidth: root.phone ? 0 : Theme.strokeHairline
        borderColor: Theme.border
        elevation: root.phone ? 0 : Theme.elevationModal
        shadowEnabled: !root.phone
        clip: true
        opacity: root.shellReveal
        scale: Theme.reduceMotion ? 1.0 : 0.985 + 0.015 * root.shellReveal

        RowLayout {
            anchors.fill: parent
            layoutDirection: Theme.uiDirection
            spacing: 0

            Item {
                id: rail
                visible: !root.phone
                Layout.preferredWidth: root.compact ? 228 : 252
                Layout.fillHeight: true

                Superellipse {
                    anchors.fill: parent
                    curveDepth: Theme.curveModal
                    fillColor: Theme.surfaceMuted
                    borderWidth: 0
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: Theme.rtl ? undefined : parent.right
                    anchors.left: Theme.rtl ? parent.left : undefined
                    width: Math.min(Theme.curveModal * 2, parent.width * 0.24)
                    color: Theme.surfaceMuted
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.shortViewport
                                     ? Theme.space3
                                     : root.compact ? Theme.space4 : Theme.space5
                    spacing: root.shortViewport ? Theme.space3 : Theme.space5

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space3

                        Superellipse {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            curveDepth: Theme.curveControl
                            fillColor: Theme.primaryFill
                            borderWidth: 0

                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconLeaf
                                color: Theme.accentInk
                                preferredSize: 22
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.app.genycaster")
                                color: Theme.ink
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightBold
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.verified_by_openproof")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.space1

                        Repeater {
                            model: root.wizardSteps

                            delegate: Superellipse {
                                id: railStep
                                required property int index
                                required property var modelData
                                readonly property int stepNumber: Number(modelData)
                                readonly property bool selected: root.currentStep === stepNumber
                                readonly property bool completed: stepNumber < root.currentStep
                                readonly property bool accessibleStep:
                                    root.returningSignIn || stepNumber <= root.currentStep
                                Layout.fillWidth: true
                                implicitHeight: root.shortViewport ? 46 : 54
                                curveDepth: Theme.curveControl
                                fillColor: selected
                                           ? Theme.surfaceSelected
                                           : navHover.hovered ? Theme.surfaceHover : "transparent"
                                borderWidth: selected ? Theme.strokeHairline : 0
                                borderColor: selected ? Theme.accent : Theme.border

                                Behavior on fillColor {
                                    ColorAnimation { duration: Theme.motionFast }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.space3
                                    anchors.rightMargin: Theme.space3
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space3

                                    Superellipse {
                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        curveDepth: 16
                                        fillColor: railStep.selected
                                                   ? Theme.primaryFill
                                                   : railStep.completed
                                                     ? Theme.primarySurface
                                                     : Theme.surfaceRaised
                                        borderWidth: Theme.strokeHairline
                                        borderColor: railStep.selected || railStep.completed
                                                     ? Theme.accent
                                                     : Theme.borderMuted

                                        Behavior on fillColor {
                                            ColorAnimation { duration: Theme.motionFast }
                                        }

                                        Avatar {
                                            anchors.centerIn: parent
                                            visible: railStep.stepNumber === 2 && root.authenticated
                                            avatarSize: 28
                                            sourceUrl: root.accountAvatarUrl
                                            displayName: root.accountName.length > 0
                                                         ? root.accountName
                                                         : root.accountUsername
                                            avatarCurveDepth: 9
                                        }

                                        Icon {
                                            anchors.centerIn: parent
                                            visible: !(railStep.stepNumber === 2 && root.authenticated)
                                            text: railStep.completed
                                                  ? Theme.iconCheck
                                                  : root.stepIcon(railStep.stepNumber)
                                            color: railStep.selected
                                                   ? Theme.accentInk
                                                   : railStep.completed
                                                     ? Theme.accent
                                                     : Theme.inkSubtle
                                            preferredSize: Theme.iconSizeSmall
                                        }
                                    }

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: root.stepTitle(railStep.stepNumber)
                                        color: railStep.selected ? Theme.ink : Theme.inkMuted
                                        font.pixelSize: Theme.typeMetaSize
                                        font.weight: railStep.selected
                                                     ? Theme.weightDemiBold : Theme.weightMedium
                                    }
                                }

                                HoverHandler {
                                    id: navHover
                                    enabled: railStep.accessibleStep
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                }
                                TapHandler {
                                    enabled: railStep.accessibleStep
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: root.switchStep(railStep.stepNumber)
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.title")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.returningSignIn
                                  ? qsTrId("genycaster.shell.sign_in_to_interact")
                                  : root.stepTitle(root.currentStep)
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            Rectangle {
                visible: !root.phone
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.divider
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        id: mobileHeader
                        visible: root.phone
                        Layout.fillWidth: true
                        implicitHeight: 118

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space4
                            anchors.rightMargin: Theme.space4
                            anchors.topMargin: Theme.space3
                            anchors.bottomMargin: Theme.space2
                            spacing: Theme.space3

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2

                                Superellipse {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    curveDepth: Theme.curveControl
                                    fillColor: Theme.primaryFill
                                    borderWidth: 0
                                    Icon {
                                        anchors.centerIn: parent
                                        text: Theme.iconLeaf
                                        color: Theme.accentInk
                                        preferredSize: 19
                                    }
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.app.genycaster")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeTitle3Size
                                    font.weight: Theme.weightBold
                                }
                                DirectionAwareLabel {
                                    text: root.stepTitle(root.currentStep)
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightDemiBold
                                }
                            }

                            RowLayout {
                                visible: !root.returningSignIn
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2

                                Repeater {
                                    model: 6
                                    delegate: Superellipse {
                                        id: mobileStep
                                        required property int index
                                        Layout.fillWidth: true
                                        implicitHeight: 7
                                        curveDepth: 4
                                        fillColor: index <= root.currentStep
                                                   ? Theme.accent : Theme.borderMuted
                                        borderWidth: 0

                                        Behavior on fillColor {
                                            ColorAnimation { duration: Theme.motionFast }
                                        }

                                        TapHandler {
                                            enabled: index <= root.currentStep
                                            onTapped: root.switchStep(mobileStep.index)
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Theme.divider
                        }
                    }

                    ScrollView {
                        id: stageScroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                        ScrollBar.vertical: GenyScrollBar {
                            policy: stageScroll.contentHeight > stageScroll.availableHeight
                                    ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        }

                        Item {
                            width: stageScroll.availableWidth
                            height: Math.max(
                                stageScroll.height,
                                stageColumn.implicitHeight + (root.phone ? Theme.space4 : Theme.space7) * 2)

                            ColumnLayout {
                                id: stageColumn
                                width: Math.min(
                                    root.phone
                                            ? parent.width - Theme.space4 * 2
                                            : root.compact ? 720 : 820,
                                    parent.width - (root.compact ? Theme.space5 : Theme.space8) * 2)
                                x: (parent.width - width) / 2
                                y: root.phone
                                   ? Theme.space4
                                   : Math.max(Theme.space5, (parent.height - implicitHeight) / 2)
                                spacing: Theme.space5
                                opacity: root.stageReveal
                                transform: Translate {
                                    y: Theme.reduceMotion ? 0 : (1.0 - root.stageReveal) * 18
                                }

                                // Welcome
                                ColumnLayout {
                                    visible: root.currentStep === 0
                                    Layout.fillWidth: true
                                    spacing: Theme.space5

                                    GridLayout {
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        columns: root.phone ? 1 : 2
                                        columnSpacing: Theme.space6
                                        rowSpacing: Theme.space4

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: Theme.space3

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.setup.welcome_eyebrow")
                                                color: Theme.accent
                                                font.pixelSize: Theme.typeMetaSize
                                                font.weight: Theme.weightBold
                                            }

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.setup.welcome_title")
                                                color: Theme.ink
                                                font.pixelSize: root.phone
                                                                ? Theme.typeTitle1Size
                                                                : Math.round(Theme.typeDisplaySize * 1.18)
                                                font.weight: Theme.weightBold
                                                lineHeight: 0.98
                                                wrapMode: Text.Wrap
                                            }

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.setup.welcome_body")
                                                color: Theme.inkMuted
                                                font.pixelSize: root.phone
                                                                ? Theme.typeBodySize
                                                                : Theme.typeTitle3Size
                                                lineHeight: 1.42
                                                wrapMode: Text.Wrap
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: root.phone
                                            Layout.minimumWidth: root.phone ? 0 : 176
                                            Layout.preferredWidth: root.phone ? 0 : 196
                                            Layout.preferredHeight: root.phone ? 138 : 172

                                            Superellipse {
                                                id: welcomeOrbit
                                                anchors.centerIn: parent
                                                width: root.phone ? 118 : 136
                                                height: width
                                                curveDepth: width / 2
                                                fillColor: Theme.primarySurface
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.accent

                                                SequentialAnimation on scale {
                                                    running: root.visible
                                                             && root.currentStep === 0
                                                             && !Theme.reduceMotion
                                                    loops: Animation.Infinite
                                                    NumberAnimation {
                                                        to: 1.045
                                                        duration: 1400
                                                        easing.type: Easing.InOutSine
                                                    }
                                                    NumberAnimation {
                                                        to: 1.0
                                                        duration: 1400
                                                        easing.type: Easing.InOutSine
                                                    }
                                                }

                                                Superellipse {
                                                    anchors.centerIn: parent
                                                    width: parent.width - 28
                                                    height: width
                                                    curveDepth: width / 2
                                                    fillColor: Theme.primaryFill
                                                    borderWidth: 0

                                                    Icon {
                                                        anchors.centerIn: parent
                                                        text: Theme.iconLeaf
                                                        color: Theme.accentInk
                                                        preferredSize: root.phone ? 40 : 48
                                                    }
                                                }
                                            }

                                            Superellipse {
                                                width: 34
                                                height: 34
                                                x: welcomeOrbit.x + welcomeOrbit.width - 12
                                                y: welcomeOrbit.y + 10
                                                curveDepth: 17
                                                fillColor: Theme.surfaceRaised
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.border
                                                Icon {
                                                    anchors.centerIn: parent
                                                    text: Theme.iconCheck
                                                    color: Theme.accent
                                                    preferredSize: 15
                                                }
                                            }
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        columns: root.phone ? 1 : 3
                                        columnSpacing: Theme.space2
                                        rowSpacing: Theme.space2

                                        Repeater {
                                            model: [
                                                {
                                                    "icon": Theme.iconUser,
                                                    "title": qsTrId("genycaster.setup.welcome_identity_title"),
                                                    "body": qsTrId("genycaster.setup.welcome_identity_body")
                                                },
                                                {
                                                    "icon": Theme.iconPalette,
                                                    "title": qsTrId("genycaster.setup.welcome_personal_title"),
                                                    "body": qsTrId("genycaster.setup.welcome_personal_body")
                                                },
                                                {
                                                    "icon": Theme.iconSettings,
                                                    "title": qsTrId("genycaster.setup.welcome_control_title"),
                                                    "body": qsTrId("genycaster.setup.welcome_control_body")
                                                }
                                            ]

                                            delegate: Superellipse {
                                                id: welcomeFeature
                                                required property var modelData
                                                Layout.fillWidth: true
                                                implicitHeight: root.phone ? 92 : 126
                                                curveDepth: Theme.curveControl
                                                fillColor: featureHover.hovered
                                                           ? Theme.surfaceHover : Theme.surface
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.borderMuted
                                                scale: featureHover.hovered && !Theme.reduceMotion ? 1.012 : 1.0

                                                Behavior on scale {
                                                    NumberAnimation {
                                                        duration: Theme.motionFast
                                                        easing.type: Easing.OutCubic
                                                    }
                                                }

                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: Theme.space3
                                                    spacing: Theme.space1

                                                    Icon {
                                                        text: welcomeFeature.modelData.icon
                                                        color: Theme.accent
                                                        preferredSize: 20
                                                    }

                                                    DirectionAwareLabel {
                                                        Layout.fillWidth: true
                                                        text: welcomeFeature.modelData.title
                                                        color: Theme.ink
                                                        font.pixelSize: Theme.typeBodySize
                                                        font.weight: Theme.weightDemiBold
                                                        wrapMode: Text.Wrap
                                                    }

                                                    DirectionAwareLabel {
                                                        Layout.fillWidth: true
                                                        text: welcomeFeature.modelData.body
                                                        color: Theme.inkMuted
                                                        font.pixelSize: Theme.typeCaptionSize
                                                        lineHeight: 1.3
                                                        wrapMode: Text.Wrap
                                                    }
                                                }

                                                HoverHandler {
                                                    id: featureHover
                                                    cursorShape: Qt.ArrowCursor
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        spacing: Theme.space2

                                        Icon {
                                            text: Theme.iconInfo
                                            color: Theme.inkSubtle
                                            preferredSize: 15
                                        }

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.setup.welcome_note")
                                            color: Theme.inkSubtle
                                            font.pixelSize: Theme.typeCaptionSize
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }

                                // Privacy and terms
                                ColumnLayout {
                                    visible: root.currentStep === 1
                                    Layout.fillWidth: true
                                    spacing: Theme.space4

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.setup.legal_title")
                                            color: Theme.ink
                                            font.pixelSize: root.phone
                                                            ? Theme.typeTitle1Size
                                                            : Math.round(Theme.typeDisplaySize * 1.06)
                                            font.weight: Theme.weightBold
                                            wrapMode: Text.Wrap
                                        }

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.setup.legal_intro")
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeBodySize
                                            lineHeight: 1.38
                                            wrapMode: Text.Wrap
                                        }
                                    }

                                    Superellipse {
                                        Layout.fillWidth: true
                                        implicitHeight: root.phone ? 330 : 310
                                        curveDepth: Theme.curveLarge
                                        fillColor: Theme.surface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: root.legalReadToEnd
                                                     ? Theme.accent : Theme.borderMuted

                                        Flickable {
                                            id: legalFlick
                                            anchors.fill: parent
                                            anchors.margins: Theme.space4
                                            contentWidth: width
                                            contentHeight: legalCopy.implicitHeight
                                            clip: true
                                            boundsBehavior: Flickable.StopAtBounds
                                            flickableDirection: Flickable.VerticalFlick

                                            ScrollBar.vertical: GenyScrollBar {
                                                policy: ScrollBar.AsNeeded
                                            }

                                            onContentYChanged: {
                                                if (atYEnd)
                                                    root.legalReadToEnd = true
                                            }

                                            onContentHeightChanged: {
                                                if (contentHeight > 0 && contentHeight <= height)
                                                    root.legalReadToEnd = true
                                            }
                                            onHeightChanged: {
                                                if (contentHeight > 0 && contentHeight <= height)
                                                    root.legalReadToEnd = true
                                            }

                                            ColumnLayout {
                                                id: legalCopy
                                                width: legalFlick.width
                                                spacing: Theme.space4

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    layoutDirection: Theme.uiDirection
                                                    spacing: Theme.space2

                                                    Superellipse {
                                                        Layout.preferredWidth: 38
                                                        Layout.preferredHeight: 38
                                                        curveDepth: Theme.curveControl
                                                        fillColor: Theme.primarySurface
                                                        borderWidth: 0
                                                        Icon {
                                                            anchors.centerIn: parent
                                                            text: Theme.iconDocument
                                                            color: Theme.accent
                                                            preferredSize: 17
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 0
                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: qsTrId("genycaster.setup.terms_title")
                                                            color: Theme.ink
                                                            font.pixelSize: Theme.typeTitle3Size
                                                            font.weight: Theme.weightBold
                                                        }
                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: qsTrId("genycaster.setup.terms_updated")
                                                            color: Theme.inkSubtle
                                                            font.pixelSize: Theme.typeCaptionSize
                                                        }
                                                    }
                                                }

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.setup.terms_body")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeMetaSize
                                                    lineHeight: 1.42
                                                    wrapMode: Text.Wrap
                                                }

                                                ActionButton {
                                                    text: qsTrId("genycaster.setup.open_terms")
                                                    quiet: true
                                                    iconGlyph: Theme.iconArrowUpRight
                                                    onClicked: Qt.openUrlExternally(
                                                        "https://genyleap.com/terms-of-use")
                                                }

                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    height: 1
                                                    color: Theme.divider
                                                }

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    layoutDirection: Theme.uiDirection
                                                    spacing: Theme.space2

                                                    Superellipse {
                                                        Layout.preferredWidth: 38
                                                        Layout.preferredHeight: 38
                                                        curveDepth: Theme.curveControl
                                                        fillColor: Theme.primarySurface
                                                        borderWidth: 0
                                                        Icon {
                                                            anchors.centerIn: parent
                                                            text: Theme.iconInfo
                                                            color: Theme.accent
                                                            preferredSize: 17
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 0
                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: qsTrId("genycaster.setup.privacy_title")
                                                            color: Theme.ink
                                                            font.pixelSize: Theme.typeTitle3Size
                                                            font.weight: Theme.weightBold
                                                        }
                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: qsTrId("genycaster.setup.privacy_updated")
                                                            color: Theme.inkSubtle
                                                            font.pixelSize: Theme.typeCaptionSize
                                                        }
                                                    }
                                                }

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.setup.privacy_body")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeMetaSize
                                                    lineHeight: 1.42
                                                    wrapMode: Text.Wrap
                                                }

                                                ActionButton {
                                                    text: qsTrId("genycaster.setup.open_privacy")
                                                    quiet: true
                                                    iconGlyph: Theme.iconArrowUpRight
                                                    onClicked: Qt.openUrlExternally(
                                                        "https://genyleap.com/privacy")
                                                }

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.setup.legal_governing_note")
                                                    color: Theme.inkSubtle
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    lineHeight: 1.34
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        spacing: Theme.space2

                                        Icon {
                                            text: root.legalReadToEnd
                                                  ? Theme.iconCheck : Theme.iconChevronDown
                                            color: root.legalReadToEnd ? Theme.accent : Theme.inkSubtle
                                            preferredSize: 15
                                        }

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: root.legalReadToEnd
                                                  ? qsTrId("genycaster.setup.legal_read_complete")
                                                  : qsTrId("genycaster.setup.scroll_to_continue")
                                            color: root.legalReadToEnd ? Theme.accent : Theme.inkSubtle
                                            font.pixelSize: Theme.typeCaptionSize
                                            font.weight: Theme.weightDemiBold
                                            wrapMode: Text.Wrap
                                        }
                                    }

                                    Superellipse {
                                        id: termsConsentRow
                                        Layout.fillWidth: true
                                        implicitHeight: 54
                                        curveDepth: Theme.curveControl
                                        fillColor: termsConsentHover.hovered
                                                   ? Theme.surfaceHover : Theme.surface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: root.legalTermsChecked
                                                     ? Theme.accent : Theme.borderMuted
                                        opacity: root.legalReadToEnd || root.legalConsentAccepted ? 1.0 : 0.55

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: Theme.space3
                                            anchors.rightMargin: Theme.space3
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space3

                                            Superellipse {
                                                Layout.preferredWidth: 24
                                                Layout.preferredHeight: 24
                                                curveDepth: 7
                                                fillColor: root.legalTermsChecked
                                                           ? Theme.primaryFill : Theme.surfaceRaised
                                                borderWidth: Theme.strokeHairline
                                                borderColor: root.legalTermsChecked
                                                             ? Theme.accent : Theme.borderStrong
                                                Icon {
                                                    anchors.centerIn: parent
                                                    visible: root.legalTermsChecked
                                                    text: Theme.iconCheck
                                                    color: Theme.accentInk
                                                    preferredSize: 13
                                                }
                                            }

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.setup.accept_terms")
                                                color: Theme.ink
                                                font.pixelSize: Theme.typeMetaSize
                                                font.weight: Theme.weightDemiBold
                                                wrapMode: Text.Wrap
                                            }
                                        }

                                        HoverHandler {
                                            id: termsConsentHover
                                            enabled: root.legalReadToEnd && !root.legalConsentAccepted
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        }
                                        TapHandler {
                                            enabled: root.legalReadToEnd && !root.legalConsentAccepted
                                            onTapped: root.legalTermsChecked = !root.legalTermsChecked
                                        }
                                    }

                                    Superellipse {
                                        id: privacyConsentRow
                                        Layout.fillWidth: true
                                        implicitHeight: 54
                                        curveDepth: Theme.curveControl
                                        fillColor: privacyConsentHover.hovered
                                                   ? Theme.surfaceHover : Theme.surface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: root.legalPrivacyChecked
                                                     ? Theme.accent : Theme.borderMuted
                                        opacity: root.legalReadToEnd || root.legalConsentAccepted ? 1.0 : 0.55

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: Theme.space3
                                            anchors.rightMargin: Theme.space3
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space3

                                            Superellipse {
                                                Layout.preferredWidth: 24
                                                Layout.preferredHeight: 24
                                                curveDepth: 7
                                                fillColor: root.legalPrivacyChecked
                                                           ? Theme.primaryFill : Theme.surfaceRaised
                                                borderWidth: Theme.strokeHairline
                                                borderColor: root.legalPrivacyChecked
                                                             ? Theme.accent : Theme.borderStrong
                                                Icon {
                                                    anchors.centerIn: parent
                                                    visible: root.legalPrivacyChecked
                                                    text: Theme.iconCheck
                                                    color: Theme.accentInk
                                                    preferredSize: 13
                                                }
                                            }

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.setup.acknowledge_privacy")
                                                color: Theme.ink
                                                font.pixelSize: Theme.typeMetaSize
                                                font.weight: Theme.weightDemiBold
                                                wrapMode: Text.Wrap
                                            }
                                        }

                                        HoverHandler {
                                            id: privacyConsentHover
                                            enabled: root.legalReadToEnd && !root.legalConsentAccepted
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        }
                                        TapHandler {
                                            enabled: root.legalReadToEnd && !root.legalConsentAccepted
                                            onTapped: root.legalPrivacyChecked = !root.legalPrivacyChecked
                                        }
                                    }
                                }

                                // Account / authentication
                                ColumnLayout {
                                    visible: root.currentStep === 2
                                    Layout.fillWidth: true
                                    spacing: Theme.space5

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: root.returningSignIn
                                                  ? qsTrId("genycaster.shell.sign_in_to_interact")
                                                  : qsTrId("genycaster.setup.title")
                                            color: Theme.ink
                                            font.pixelSize: root.phone
                                                            ? Theme.typeTitle1Size
                                                            : Math.round(Theme.typeDisplaySize * 1.10)
                                            font.weight: Theme.weightBold
                                            lineHeight: 1.02
                                            wrapMode: Text.Wrap
                                        }

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: root.phaseBody()
                                            color: Theme.inkMuted
                                            font.pixelSize: root.phone
                                                            ? Theme.typeBodySize
                                                            : Theme.typeTitle3Size
                                            lineHeight: 1.42
                                            wrapMode: Text.Wrap
                                        }
                                    }

                                    Superellipse {
                                        Layout.fillWidth: true
                                        implicitHeight: identityVisual.implicitHeight + Theme.space5 * 2
                                        curveDepth: Theme.curveLarge
                                        fillColor: Theme.primarySurface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.accent

                                        RowLayout {
                                            id: identityVisual
                                            anchors.fill: parent
                                            anchors.margins: root.phone ? Theme.space4 : Theme.space5
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space4

                                            Item {
                                                Layout.preferredWidth: root.phone ? 56 : 68
                                                Layout.preferredHeight: Layout.preferredWidth

                                                Superellipse {
                                                    id: accountIdentityHalo
                                                    anchors.centerIn: parent
                                                    width: parent.width
                                                    height: parent.height
                                                    curveDepth: Theme.curveControl
                                                    fillColor: root.authenticated
                                                               ? Theme.primarySurface
                                                               : Theme.primaryFill
                                                    borderWidth: root.authenticated
                                                                 ? Theme.strokeStandard
                                                                 : 0
                                                    borderColor: Theme.accent
                                                    opacity: root.authenticationInProgress ? 0.78 : 1.0

                                                    SequentialAnimation on scale {
                                                        running: (root.authenticationInProgress
                                                                  || root.authenticated)
                                                                 && !Theme.reduceMotion
                                                        loops: root.authenticationInProgress
                                                               ? Animation.Infinite : 1
                                                        NumberAnimation {
                                                            from: root.authenticated ? 0.94 : 1.0
                                                            to: 1.055
                                                            duration: root.authenticated ? 260 : 720
                                                            easing.type: Easing.OutCubic
                                                        }
                                                        NumberAnimation {
                                                            to: 1.0
                                                            duration: root.authenticated ? 180 : 720
                                                            easing.type: Easing.InOutSine
                                                        }
                                                    }
                                                }

                                                Avatar {
                                                    anchors.centerIn: parent
                                                    visible: root.authenticated
                                                    avatarSize: root.phone ? 48 : 58
                                                    sourceUrl: root.accountAvatarUrl
                                                    displayName: root.accountName.length > 0
                                                                 ? root.accountName
                                                                 : root.accountUsername
                                                    avatarCurveDepth: Theme.curveControl
                                                }

                                                Superellipse {
                                                    anchors.centerIn: parent
                                                    visible: !root.authenticated
                                                    width: root.phone ? 48 : 58
                                                    height: width
                                                    curveDepth: Theme.curveControl
                                                    fillColor: Theme.primaryFill
                                                    borderWidth: 0

                                                    Icon {
                                                        anchors.centerIn: parent
                                                        text: root.authenticationInProgress
                                                              ? Theme.iconEnergy : Theme.iconUser
                                                        color: Theme.accentInk
                                                        preferredSize: Theme.iconSizeLarge
                                                    }
                                                }

                                                Superellipse {
                                                    visible: root.authenticated
                                                    width: 20
                                                    height: 20
                                                    anchors.right: parent.right
                                                    anchors.bottom: parent.bottom
                                                    curveDepth: 10
                                                    fillColor: Theme.positive
                                                    borderWidth: 2
                                                    borderColor: Theme.surfaceRaised

                                                    Icon {
                                                        anchors.centerIn: parent
                                                        text: Theme.iconCheck
                                                        color: Theme.accentInk
                                                        preferredSize: 11
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
                                                    text: qsTrId("genycaster.shell.openproof_account_farcaster_protocol_identity")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeMetaSize
                                                    wrapMode: Text.Wrap
                                                }
                                            }

                                            Superellipse {
                                                visible: !root.phone
                                                Layout.preferredWidth: 38
                                                Layout.preferredHeight: 38
                                                curveDepth: 19
                                                fillColor: Theme.surfaceRaised
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.border
                                                Icon {
                                                    anchors.centerIn: parent
                                                    text: Theme.iconLink
                                                    color: Theme.accent
                                                    preferredSize: 17
                                                }
                                            }
                                        }
                                    }

                                    Superellipse {
                                        Layout.fillWidth: true
                                        implicitHeight: accountActions.implicitHeight
                                                        + (root.phone ? Theme.space4 : Theme.space5) * 2
                                        curveDepth: Theme.curveLarge
                                        fillColor: Theme.surface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.borderMuted

                                        ColumnLayout {
                                            id: accountActions
                                            anchors.fill: parent
                                            anchors.margins: root.phone ? Theme.space4 : Theme.space5
                                            spacing: Theme.space3

                                            Superellipse {
                                                visible: root.authenticationErrorText.length > 0
                                                Layout.fillWidth: true
                                                implicitHeight: authErrorText.implicitHeight + Theme.space3 * 2
                                                curveDepth: Theme.curveCompact
                                                fillColor: Theme.warningSurface
                                                borderWidth: 0

                                                DirectionAwareLabel {
                                                    id: authErrorText
                                                    anchors.fill: parent
                                                    anchors.margins: Theme.space3
                                                    text: root.authenticationErrorText
                                                    color: Theme.warning
                                                    font.pixelSize: Theme.typeMetaSize
                                                    lineHeight: 1.35
                                                    wrapMode: Text.Wrap
                                                }
                                            }

                                            Superellipse {
                                                visible: root.runtimeReady && !root.authenticationAvailable
                                                Layout.fillWidth: true
                                                implicitHeight: unavailableText.implicitHeight + Theme.space3 * 2
                                                curveDepth: Theme.curveCompact
                                                fillColor: Theme.warningSurface
                                                borderWidth: 0

                                                DirectionAwareLabel {
                                                    id: unavailableText
                                                    anchors.fill: parent
                                                    anchors.margins: Theme.space3
                                                    text: qsTrId("genycaster.runtime.sign_in_could_not_be_completed")
                                                    color: Theme.warning
                                                    font.pixelSize: Theme.typeMetaSize
                                                    wrapMode: Text.Wrap
                                                }
                                            }

                                            RowLayout {
                                                visible: root.authenticationStarting
                                                Layout.fillWidth: true
                                                layoutDirection: Theme.uiDirection
                                                spacing: Theme.space3
                                                GenySpinner {
                                                    Layout.preferredWidth: 24
                                                    Layout.preferredHeight: 24
                                                }
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: root.authenticationRoute === "farcaster"
                                                          ? qsTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request")
                                                          : qsTrId("genycaster.shell.connecting_securely_to_genyleap")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeBodySize
                                                    wrapMode: Text.Wrap
                                                }
                                            }

                                            ColumnLayout {
                                                visible: root.awaitingApproval || root.linkingFarcaster
                                                Layout.fillWidth: true
                                                spacing: Theme.space2

                                                ActionButton {
                                                    Layout.fillWidth: true
                                                    implicitHeight: Theme.controlHeight + 6
                                                    text: root.linkingFarcaster
                                                          || root.authenticationRoute === "farcaster"
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
                                                visible: !root.authenticationInProgress && !root.authenticated
                                                Layout.fillWidth: true
                                                spacing: Theme.space2

                                                ActionButton {
                                                    objectName: "initialSetupGenyleapButton"
                                                    Layout.fillWidth: true
                                                    implicitHeight: Theme.controlHeight + 8
                                                    text: qsTrId("genycaster.shell.continue_with_genyleap")
                                                    prominent: true
                                                    iconGlyph: Theme.iconSignIn
                                                    enabled: root.runtimeReady && root.authenticationAvailable
                                                    onClicked: root.continueWithGenyleap()
                                                }

                                                ActionButton {
                                                    objectName: "initialSetupFarcasterButton"
                                                    Layout.fillWidth: true
                                                    implicitHeight: Theme.controlHeight + 8
                                                    text: qsTrId("genycaster.shell.sign_in_with_farcaster")
                                                    iconGlyph: Theme.iconLink
                                                    enabled: root.runtimeReady && root.authenticationAvailable
                                                    onClicked: root.continueWithFarcaster()
                                                }
                                            }

                                            Superellipse {
                                                visible: root.authenticated
                                                Layout.fillWidth: true
                                                implicitHeight: verifiedAccountRow.implicitHeight + Theme.space3 * 2
                                                curveDepth: Theme.curveControl
                                                fillColor: Theme.primarySurface
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.accent

                                                RowLayout {
                                                    id: verifiedAccountRow
                                                    anchors.fill: parent
                                                    anchors.margins: Theme.space3
                                                    layoutDirection: Theme.uiDirection
                                                    spacing: Theme.space3

                                                    Superellipse {
                                                        Layout.preferredWidth: 34
                                                        Layout.preferredHeight: 34
                                                        curveDepth: 17
                                                        fillColor: Theme.primaryFill
                                                        borderWidth: 0

                                                        Icon {
                                                            anchors.centerIn: parent
                                                            text: Theme.iconCheck
                                                            color: Theme.accentInk
                                                            preferredSize: 16
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 0

                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: root.accountName.length > 0
                                                                  ? root.accountName
                                                                  : root.accountUsername
                                                            color: Theme.ink
                                                            font.pixelSize: Theme.typeBodySize
                                                            font.weight: Theme.weightDemiBold
                                                            elide: Text.ElideRight
                                                        }

                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: qsTrId("genycaster.setup.verified_account_ready")
                                                            color: Theme.inkMuted
                                                            font.pixelSize: Theme.typeCaptionSize
                                                            wrapMode: Text.Wrap
                                                        }
                                                    }
                                                }
                                            }

                                            RowLayout {
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
                                                    text: qsTrId("genycaster.shell.verified_by_openproof")
                                                    color: Theme.inkSubtle
                                                    font.pixelSize: Theme.typeCaptionSize
                                                }
                                            }

                                            DirectionAwareLabel {
                                                visible: !root.runtimeReady
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.bridge.starting_local_read_runtime")
                                                color: Theme.inkSubtle
                                                font.pixelSize: Theme.typeCaptionSize
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                    }
                                }

                                // Appearance
                                ColumnLayout {
                                    visible: root.currentStep === 3
                                    Layout.fillWidth: true
                                    spacing: Theme.space5

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.appearance")
                                            color: Theme.ink
                                            font.pixelSize: root.phone
                                                            ? Theme.typeTitle1Size
                                                            : Math.round(Theme.typeDisplaySize * 1.06)
                                            font.weight: Theme.weightBold
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.appearance_description")
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeBodySize
                                            wrapMode: Text.Wrap
                                        }
                                    }

                                    Superellipse {
                                        Layout.fillWidth: true
                                        implicitHeight: 132
                                        curveDepth: Theme.curveLarge
                                        fillColor: Theme.primarySurface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.border

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.space4
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space4

                                            Superellipse {
                                                Layout.preferredWidth: 72
                                                Layout.preferredHeight: 72
                                                curveDepth: Theme.curveControl
                                                fillColor: Theme.primaryFill
                                                borderWidth: 0
                                                Icon {
                                                    anchors.centerIn: parent
                                                    text: Theme.iconPalette
                                                    color: Theme.accentInk
                                                    preferredSize: 30
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.space1
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.app.genycaster")
                                                    color: Theme.ink
                                                    font.pixelSize: Theme.typeTitle2Size
                                                    font.weight: Theme.weightBold
                                                }
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.settings.description")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeMetaSize
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.mode")
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeLabelSize
                                            font.weight: Theme.weightDemiBold
                                        }

                                        GridLayout {
                                            Layout.fillWidth: true
                                            layoutDirection: Theme.uiDirection
                                            columns: root.phone ? 1 : 3
                                            rowSpacing: Theme.space2
                                            columnSpacing: Theme.space2

                                            Repeater {
                                                model: [
                                                    { "id": "system", "label": qsTrId("genycaster.settings.mode_system"), "icon": Theme.iconSettings },
                                                    { "id": "dark", "label": qsTrId("genycaster.settings.mode_dark"), "icon": Theme.iconMoon },
                                                    { "id": "light", "label": qsTrId("genycaster.settings.mode_light"), "icon": Theme.iconSun }
                                                ]

                                                delegate: Superellipse {
                                                    id: modeCard
                                                    required property var modelData
                                                    readonly property bool selected:
                                                        appearanceController.themeMode === String(modelData.id)
                                                    Layout.fillWidth: true
                                                    implicitHeight: 58
                                                    curveDepth: Theme.curveControl
                                                    fillColor: selected
                                                               ? Theme.surfaceSelected
                                                               : modeHover.hovered
                                                                 ? Theme.surfaceHover
                                                                 : Theme.surface
                                                    borderWidth: selected ? 2 : Theme.strokeHairline
                                                    borderColor: selected ? Theme.accent : Theme.borderMuted

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: Theme.space3
                                                        anchors.rightMargin: Theme.space3
                                                        spacing: Theme.space2
                                                        layoutDirection: Theme.uiDirection
                                                        Icon {
                                                            text: modeCard.modelData.icon
                                                            color: modeCard.selected ? Theme.accent : Theme.inkMuted
                                                            preferredSize: 17
                                                        }
                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: modeCard.modelData.label
                                                            color: Theme.ink
                                                            font.pixelSize: Theme.typeBodySize
                                                            font.weight: modeCard.selected
                                                                         ? Theme.weightDemiBold
                                                                         : Theme.weightMedium
                                                        }
                                                        Icon {
                                                            visible: modeCard.selected
                                                            text: Theme.iconCheck
                                                            color: Theme.accent
                                                            preferredSize: 15
                                                        }
                                                    }
                                                    HoverHandler {
                                                        id: modeHover
                                                        cursorShape: Qt.PointingHandCursor
                                                    }
                                                    TapHandler {
                                                        onTapped: appearanceController.setThemeMode(
                                                            String(modeCard.modelData.id))
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.color_theme")
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeLabelSize
                                            font.weight: Theme.weightDemiBold
                                        }

                                        GridLayout {
                                            Layout.fillWidth: true
                                            layoutDirection: Theme.uiDirection
                                            columns: root.phone ? 2 : root.compact ? 4 : 7
                                            rowSpacing: Theme.space2
                                            columnSpacing: Theme.space2

                                            Repeater {
                                                model: [
                                                    { "id": "geny", "label": qsTrId("genycaster.settings.theme_geny"), "color": Theme.swatchGeny },
                                                    { "id": "ocean", "label": qsTrId("genycaster.settings.theme_ocean"), "color": Theme.swatchOcean },
                                                    { "id": "violet", "label": qsTrId("genycaster.settings.theme_violet"), "color": Theme.swatchViolet },
                                                    { "id": "ember", "label": qsTrId("genycaster.settings.theme_ember"), "color": Theme.swatchEmber },
                                                    { "id": "rose", "label": qsTrId("genycaster.settings.theme_rose"), "color": Theme.swatchRose },
                                                    { "id": "mint", "label": qsTrId("genycaster.settings.theme_mint"), "color": Theme.swatchMint },
                                                    { "id": "mono", "label": qsTrId("genycaster.settings.theme_mono"), "color": Theme.swatchMono }
                                                ]

                                                delegate: Superellipse {
                                                    id: themeCard
                                                    required property var modelData
                                                    readonly property bool selected:
                                                        appearanceController.colorThemeId === String(modelData.id)
                                                    Layout.fillWidth: true
                                                    implicitHeight: 72
                                                    curveDepth: Theme.curveControl
                                                    fillColor: selected
                                                               ? Theme.surfaceSelected
                                                               : themeHover.hovered
                                                                 ? Theme.surfaceHover
                                                                 : Theme.surface
                                                    borderWidth: selected ? 2 : Theme.strokeHairline
                                                    borderColor: selected ? Theme.accent : Theme.borderMuted

                                                    ColumnLayout {
                                                        anchors.centerIn: parent
                                                        width: parent.width - Theme.space2 * 2
                                                        spacing: Theme.space1

                                                        Superellipse {
                                                            Layout.alignment: Qt.AlignHCenter
                                                            Layout.preferredWidth: 26
                                                            Layout.preferredHeight: 26
                                                            curveDepth: 8
                                                            fillColor: themeCard.modelData.color
                                                            borderWidth: Theme.strokeHairline
                                                            borderColor: Theme.borderStrong
                                                        }
                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: themeCard.modelData.label
                                                            color: Theme.ink
                                                            font.pixelSize: Theme.typeCaptionSize
                                                            font.weight: themeCard.selected
                                                                         ? Theme.weightDemiBold
                                                                         : Theme.weightMedium
                                                            horizontalAlignment: Text.AlignHCenter
                                                            elide: Text.ElideRight
                                                        }
                                                    }
                                                    HoverHandler {
                                                        id: themeHover
                                                        cursorShape: Qt.PointingHandCursor
                                                    }
                                                    TapHandler {
                                                        onTapped: appearanceController.setColorThemeId(
                                                            String(themeCard.modelData.id))
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Superellipse {
                                        Layout.fillWidth: true
                                        implicitHeight: motionRow.implicitHeight + Theme.space3 * 2
                                        curveDepth: Theme.curveControl
                                        fillColor: Theme.surface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.borderMuted

                                        RowLayout {
                                            id: motionRow
                                            anchors.fill: parent
                                            anchors.margins: Theme.space3
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space3

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.settings.reduce_motion")
                                                    color: Theme.ink
                                                    font.pixelSize: Theme.typeBodySize
                                                    font.weight: Theme.weightDemiBold
                                                }
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: qsTrId("genycaster.settings.reduce_motion_description")
                                                    color: Theme.inkSubtle
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    wrapMode: Text.Wrap
                                                }
                                            }

                                            GenySwitch {
                                                checked: appearanceController.reduceMotion
                                                onToggled: appearanceController.setReduceMotion(checked)
                                            }
                                        }
                                    }
                                }

                                // Language and region
                                ColumnLayout {
                                    visible: root.currentStep === 4
                                    Layout.fillWidth: true
                                    spacing: Theme.space5

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.language_region")
                                            color: Theme.ink
                                            font.pixelSize: root.phone
                                                            ? Theme.typeTitle1Size
                                                            : Math.round(Theme.typeDisplaySize * 1.06)
                                            font.weight: Theme.weightBold
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.locale.regional_format_description")
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeBodySize
                                            wrapMode: Text.Wrap
                                        }
                                    }

                                    Superellipse {
                                        Layout.fillWidth: true
                                        implicitHeight: localePreview.implicitHeight + Theme.space4 * 2
                                        curveDepth: Theme.curveLarge
                                        fillColor: Theme.primarySurface
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.border

                                        RowLayout {
                                            id: localePreview
                                            anchors.fill: parent
                                            anchors.margins: Theme.space4
                                            layoutDirection: Theme.uiDirection
                                            spacing: Theme.space3

                                            Superellipse {
                                                Layout.preferredWidth: 52
                                                Layout.preferredHeight: 52
                                                curveDepth: Theme.curveControl
                                                fillColor: Theme.surfaceRaised
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.border
                                                Icon {
                                                    anchors.centerIn: parent
                                                    text: Theme.iconLanguage
                                                    color: Theme.accent
                                                    preferredSize: 22
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.space1
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: localizationController.languageLabel
                                                    color: Theme.ink
                                                    font.pixelSize: Theme.typeTitle3Size
                                                    font.weight: Theme.weightBold
                                                }
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: localizationController.regionalLocaleLabel
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeMetaSize
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        columns: root.phone ? 1 : 2
                                        columnSpacing: Theme.space3
                                        rowSpacing: Theme.space3

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.space1
                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.locale.language")
                                                color: Theme.ink
                                                font.pixelSize: Theme.typeLabelSize
                                                font.weight: Theme.weightDemiBold
                                            }
                                            GenyComboBox {
                                                Layout.fillWidth: true
                                                model: localizationController.availableLanguages
                                                textRole: "label"
                                                currentIndex: root.indexFor(
                                                    model, localizationController.languageTag)
                                                onActivated: index =>
                                                    localizationController.setLanguageTag(
                                                        String(model[index].id))
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.space1
                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.locale.regional_format")
                                                color: Theme.ink
                                                font.pixelSize: Theme.typeLabelSize
                                                font.weight: Theme.weightDemiBold
                                            }
                                            GenyComboBox {
                                                Layout.fillWidth: true
                                                model: localizationController.availableRegionalLocales
                                                textRole: "label"
                                                currentIndex: root.indexFor(
                                                    model, localizationController.regionalLocaleTag)
                                                onActivated: index =>
                                                    localizationController.setRegionalLocaleTag(
                                                        String(model[index].id))
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.space1
                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.locale.calendar")
                                                color: Theme.ink
                                                font.pixelSize: Theme.typeLabelSize
                                                font.weight: Theme.weightDemiBold
                                            }
                                            GenyComboBox {
                                                Layout.fillWidth: true
                                                model: localizationController.availableCalendars
                                                textRole: "label"
                                                currentIndex: root.indexFor(
                                                    model, localizationController.calendarId)
                                                onActivated: index =>
                                                    localizationController.setCalendarId(
                                                        String(model[index].id))
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.space1
                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.locale.time_zone")
                                                color: Theme.ink
                                                font.pixelSize: Theme.typeLabelSize
                                                font.weight: Theme.weightDemiBold
                                            }
                                            GenyComboBox {
                                                Layout.fillWidth: true
                                                popupMaximumWidth: Math.max(640, width * 1.5)
                                                model: localizationController.availableTimeZones
                                                textRole: "label"
                                                currentIndex: root.indexFor(
                                                    model, localizationController.timeZoneId)
                                                onActivated: index =>
                                                    localizationController.setTimeZoneId(
                                                        String(model[index].id))
                                            }
                                        }
                                    }
                                }

                                // Ready
                                ColumnLayout {
                                    visible: root.currentStep === 5
                                    Layout.fillWidth: true
                                    spacing: Theme.space5

                                    Item {
                                        Layout.fillWidth: true
                                        implicitHeight: root.phone ? 136 : 156

                                        Superellipse {
                                            anchors.centerIn: parent
                                            width: root.phone ? 104 : 116
                                            height: width
                                            curveDepth: width / 2
                                            fillColor: Theme.primarySurface
                                            borderWidth: Theme.strokeStandard
                                            borderColor: Theme.accent

                                            Superellipse {
                                                anchors.centerIn: parent
                                                width: parent.width - 22
                                                height: width
                                                curveDepth: width / 2
                                                fillColor: Theme.primaryFill
                                                borderWidth: 0
                                                Icon {
                                                    anchors.centerIn: parent
                                                    text: Theme.iconCheck
                                                    color: Theme.accentInk
                                                    preferredSize: root.phone ? 36 : 42
                                                }
                                            }

                                            SequentialAnimation on scale {
                                                running: root.visible
                                                         && root.currentStep === 5
                                                         && !Theme.reduceMotion
                                                loops: 1
                                                NumberAnimation {
                                                    from: 0.90
                                                    to: 1.06
                                                    duration: 260
                                                    easing.type: Easing.OutCubic
                                                }
                                                NumberAnimation {
                                                    to: 1.0
                                                    duration: 180
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.status.ready")
                                            color: Theme.ink
                                            font.pixelSize: root.phone
                                                            ? Theme.typeTitle1Size
                                                            : Math.round(Theme.typeDisplaySize * 1.06)
                                            font.weight: Theme.weightBold
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.description")
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeBodySize
                                            horizontalAlignment: Text.AlignHCenter
                                            wrapMode: Text.Wrap
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        columns: root.phone ? 1 : 2
                                        columnSpacing: Theme.space2
                                        rowSpacing: Theme.space2

                                        Repeater {
                                            model: [
                                                {
                                                    "icon": Theme.iconUser,
                                                    "avatar": true,
                                                    "title": qsTrId("genycaster.header.account"),
                                                    "value": root.accountName.length > 0
                                                             ? root.accountName
                                                             : root.accountUsername
                                                },
                                                {
                                                    "icon": Theme.iconDocument,
                                                    "title": qsTrId("genycaster.setup.privacy_terms"),
                                                    "value": qsTrId("genycaster.setup.consent_recorded")
                                                },
                                                {
                                                    "icon": Theme.iconPalette,
                                                    "title": qsTrId("genycaster.settings.appearance"),
                                                    "value": appearanceController.colorThemeId
                                                },
                                                {
                                                    "icon": Theme.iconLanguage,
                                                    "title": qsTrId("genycaster.settings.language_region"),
                                                    "value": localizationController.languageLabel
                                                }
                                            ]

                                            delegate: Superellipse {
                                                id: summaryCard
                                                required property var modelData
                                                Layout.fillWidth: true
                                                implicitHeight: 86
                                                curveDepth: Theme.curveControl
                                                fillColor: summaryHover.hovered
                                                           ? Theme.surfaceHover : Theme.surface
                                                borderWidth: Theme.strokeHairline
                                                borderColor: Theme.borderMuted

                                                Behavior on fillColor {
                                                    ColorAnimation { duration: Theme.motionFast }
                                                }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: Theme.space3
                                                    layoutDirection: Theme.uiDirection
                                                    spacing: Theme.space3

                                                    Superellipse {
                                                        Layout.preferredWidth: 38
                                                        Layout.preferredHeight: 38
                                                        curveDepth: Theme.curveControl
                                                        fillColor: Theme.primarySurface
                                                        borderWidth: Theme.strokeHairline
                                                        borderColor: Theme.borderSubtle

                                                        Avatar {
                                                            anchors.centerIn: parent
                                                            visible: Boolean(summaryCard.modelData.avatar)
                                                            avatarSize: 34
                                                            sourceUrl: root.accountAvatarUrl
                                                            displayName: root.accountName.length > 0
                                                                         ? root.accountName
                                                                         : root.accountUsername
                                                            avatarCurveDepth: 10
                                                        }

                                                        Icon {
                                                            anchors.centerIn: parent
                                                            visible: !Boolean(summaryCard.modelData.avatar)
                                                            text: summaryCard.modelData.icon
                                                            color: Theme.accent
                                                            preferredSize: Theme.iconSize
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        Layout.minimumWidth: 0
                                                        spacing: 1

                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: summaryCard.modelData.title
                                                            color: Theme.inkMuted
                                                            font.pixelSize: Theme.typeCaptionSize
                                                            font.weight: Theme.weightDemiBold
                                                            elide: Text.ElideRight
                                                        }

                                                        DirectionAwareLabel {
                                                            Layout.fillWidth: true
                                                            text: summaryCard.modelData.value
                                                            color: Theme.ink
                                                            font.pixelSize: Theme.typeBodySize
                                                            font.weight: Theme.weightDemiBold
                                                            elide: Text.ElideRight
                                                        }
                                                    }
                                                }

                                                HoverHandler {
                                                    id: summaryHover
                                                    cursorShape: Qt.ArrowCursor
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: setupFooter
                        visible: !root.returningSignIn && !root.setupCompleted
                        Layout.fillWidth: true
                        implicitHeight: root.phone ? 76 : 82

                        Superellipse {
                            anchors.fill: parent
                            curveDepth: root.phone ? 0 : Theme.curveModal
                            fillColor: Theme.surfaceRaised
                            borderWidth: 0
                        }

                        Rectangle {
                            visible: !root.phone
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: Theme.rtl ? undefined : parent.left
                            anchors.right: Theme.rtl ? parent.right : undefined
                            width: Math.min(Theme.curveModal * 2, parent.width * 0.18)
                            color: Theme.surfaceRaised
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 1
                            color: Theme.divider
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: root.phone ? Theme.space4 : Theme.space5
                            anchors.rightMargin: root.phone ? Theme.space4 : Theme.space5
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2

                            ActionButton {
                                visible: root.currentStep > 0
                                text: qsTrId("genycaster.setup.back")
                                quiet: true
                                iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                                enabled: !root.authenticationInProgress
                                onClicked: root.previousStep()
                            }

                            Item { Layout.fillWidth: true }

                            ColumnLayout {
                                visible: !root.phone
                                spacing: 0

                                DirectionAwareLabel {
                                    text: root.stepTitle(root.currentStep)
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightDemiBold
                                    horizontalAlignment: Text.AlignRight
                                }

                                DirectionAwareLabel {
                                    text: LocaleFormat.formatInteger(root.currentStep + 1)
                                          + " / " + LocaleFormat.formatInteger(6)
                                    color: Theme.inkSubtle
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            ActionButton {
                                implicitWidth: root.phone ? 0 : 220
                                Layout.fillWidth: root.phone
                                text: root.primaryActionText()
                                prominent: true
                                enabled: root.primaryActionEnabled()
                                iconGlyph: root.currentStep === 5
                                           ? Theme.iconCheck
                                           : (Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight)
                                onClicked: root.nextStep()
                            }
                        }
                    }
                }
            }
        }
    }
}
