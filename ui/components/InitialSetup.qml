pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "initialSetup"
    focus: visible

    // GUI-* layout contract:
    // - one bounded task column (640 px compact / 720 px wide maximum)
    // - 64 px header and responsive safe footer
    // - sidebar appears only when the viewport can preserve the task width
    // - horizontal shell regions use Theme.uiDirection for deterministic RTL;
    //   mixed-language content such as the language carousel stays physical
    Accessible.name: qsTrId("genycaster.setup.title")

    property bool runtimeReady: false
    property bool authenticationAvailable: false
    property bool authenticated: false
    property bool setupCompleted: false
    property bool returningAfterLogout: false
    property bool legalConsentAccepted: false
    property string authenticationPhase: "signed-out"
    property string authenticationRoute: "openproof"
    property bool authenticationApprovalAvailable: false
    property string authenticationErrorText: ""
    property string signerStatusText: ""
    property string signerErrorText: ""
    property bool signerBusy: false
    property bool signerReady: false
    property string accountName: ""
    property string accountUsername: ""
    property string accountAvatarUrl: ""

    // The chosen transport preference is separate from the verified identity and signer.
    property string accountIntent: "existing"
    property bool alternateMethodsExpanded: false
    property bool showAdvancedNodes: false

    property bool legalTermsChecked: false
    property bool legalPrivacyChecked: false
    property bool legalTermsReviewed: false
    property bool legalPrivacyReviewed: false
    property string legalReaderKind: ""
    property bool legalReaderReachedEnd: false
    property int currentStep: 0
    property int highestReachedStep: 0
    property real stageReveal: 1.0

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal openRegistration()
    signal openApproval()
    signal cancelAuthentication()
    signal authorizeSigner()
    signal recoverAccount()
    signal acceptLegalConsent(bool termsAccepted, bool privacyAcknowledged, bool reviewCompleted)
    signal finishSetup()

    readonly property bool phone: width < 680
    readonly property bool compact: width < 1040
    readonly property bool browserReadOnly: wasmReadOnly

    // Setup design-system layer: every stage composes the same neutral surfaces,
    // typography scale and spacing rhythm. Accent is reserved for focus/action,
    // while semantic colors are used only for compact status feedback.
    readonly property int setupContentWidth: 600
    readonly property int setupPageTitleSize: Theme.typeTitle2Size
    readonly property int setupBodySize: Theme.typeLabelSize
    readonly property int setupSectionTitleSize: Theme.typeTitle3Size
    readonly property int setupCardPadding: Theme.space4
    readonly property int setupGap: Theme.space4
    readonly property color setupCard: Theme.surface
    readonly property color setupCardRaised: Theme.surfaceRaised
    readonly property color setupBorder: Theme.borderMuted
    readonly property color setupSelectedBorder: Theme.accent
    readonly property string genyBrandIcon: "brands/geny"
    readonly property string hypersnapDisplayName: "HyperSnap v2"

    readonly property bool authenticationStarting: authenticationPhase === "starting"
    readonly property bool awaitingApproval: authenticationPhase === "awaiting-approval"
    readonly property bool linkingFarcaster: authenticationPhase === "linking-farcaster"
    readonly property bool authenticationInProgress:
        authenticationStarting || awaitingApproval || linkingFarcaster
    // Sign-in is independent of the selected read network. Keep the active
    // action attached to the route that actually initiated the ceremony.
    readonly property string activeSignInRoute: authenticationInProgress
        ? authenticationRoute
        : (presentationBridge.preferredNetwork === "farcaster" ? "farcaster" : "openproof")
    readonly property var wizardSteps: [0, 1, 2, 3, 4, 5]
    readonly property var greetings: [
        { tag: "en", hello: "Hello", language: "English" },
        { tag: "fa", hello: "سلام", language: "فارسی" },
        { tag: "ar", hello: "مرحباً", language: "العربية" },
        { tag: "es", hello: "Hola", language: "Español" },
        { tag: "fr", hello: "Bonjour", language: "Français" },
        { tag: "de", hello: "Hallo", language: "Deutsch" },
        { tag: "pt", hello: "Olá", language: "Português" },
        { tag: "ru", hello: "Привет", language: "Русский" },
        { tag: "ja", hello: "こんにちは", language: "日本語" },
        { tag: "ko", hello: "안녕하세요", language: "한국어" },
        { tag: "zh", hello: "你好", language: "中文" },
        { tag: "hi", hello: "नमस्ते", language: "हिन्दी" }
    ]

    function stepTitle(index) {
        switch (index) {
        case 0: return qsTrId("genycaster.setup.welcome")
        case 1: return qsTrId("genycaster.setup.welcome_personal_title")
        case 2: return qsTrId("genycaster.setup.privacy_terms")
        case 3: return qsTrId("genycaster.setup.network")
        case 4: return qsTrId("genycaster.header.account")
        case 5: return qsTrId("genycaster.status.ready")
        default: return ""
        }
    }

    function stepIcon(index) {
        switch (index) {
        case 0: return Theme.iconLanguage
        case 1: return Theme.iconPalette
        case 2: return Theme.iconDocument
        case 3: return Theme.iconWifi
        case 4: return Theme.iconUser
        case 5: return root.genyBrandIcon
        default: return Theme.iconInfo
        }
    }

    function indexFor(model, selectedId) {
        for (let index = 0; index < model.length; ++index) {
            if (String(model[index].id) === String(selectedId))
                return index
        }
        return 0
    }

    function greetingIndexForTag(tag) {
        const normalized = String(tag || "").toLowerCase().split(/[-_]/)[0]
        for (let index = 0; index < greetings.length; ++index) {
            if (greetings[index].tag === normalized)
                return index
        }
        return 0
    }

    function accentCode() {
        return String(Theme.accent).toUpperCase()
    }

    function stepComplete(index) {
        if (index === 0)
            return highestReachedStep > 0
        if (index === 1)
            return highestReachedStep > 1
        if (index === 2)
            return legalConsentAccepted
        if (index === 3)
            return highestReachedStep > 3
        if (index === 4)
            return authenticated
        return setupCompleted
    }

    function stepUnlocked(index) {
        if (!legalConsentAccepted && index > 2)
            return false
        if (index <= highestReachedStep)
            return true
        if (index === highestReachedStep + 1)
            return true
        if (index === 5)
            return authenticated && highestReachedStep >= 4
        return false
    }

    function openLegalReader(kind) {
        legalReaderKind = kind
        legalReaderReachedEnd = false
        legalReader.open()
    }

    function acceptLegalReader() {
        if (!legalReaderReachedEnd)
            return
        if (legalReaderKind === "privacy") {
            legalPrivacyReviewed = true
            legalPrivacyChecked = true
        } else {
            legalTermsReviewed = true
            legalTermsChecked = true
        }
        legalReader.close()
        if (legalTermsReviewed && legalPrivacyReviewed && !legalConsentAccepted)
            acceptLegalConsent(true, true, true)
    }

    function switchStep(index) {
        index = Math.max(0, Math.min(5, index))
        if (!legalConsentAccepted && index > 2)
            index = 2
        if (!authenticated && !root.browserReadOnly && index > 4)
            index = 4
        highestReachedStep = Math.max(highestReachedStep, index)
        if (index === currentStep) {
            stageReveal = 1.0
            return
        }
        currentStep = index
        stageFlick.contentY = 0
        if (index === 3)
            Qt.callLater(() => nodeSelectionController.refresh())
        if (Theme.reduceMotion) {
            stageReveal = 1.0
        } else {
            stageReveal = 0.0
            stageEntrance.restart()
        }
    }

    function synchronizeFlow() {
        if (!legalConsentAccepted && currentStep > 2) {
            switchStep(2)
            return
        }
        if (!authenticated && !root.browserReadOnly && currentStep > 4)
            switchStep(4)
    }

    function nextStep() {
        if (currentStep === 0 || currentStep === 1) {
            switchStep(currentStep + 1)
            return
        }

        if (currentStep === 2) {
            if (!legalConsentAccepted) {
                if (!legalTermsReviewed || !legalPrivacyReviewed || !legalTermsChecked || !legalPrivacyChecked)
                    return
                acceptLegalConsent(legalTermsChecked, legalPrivacyChecked,
                                   legalTermsReviewed && legalPrivacyReviewed)
            }
            Qt.callLater(() => switchStep(3))
            return
        }

        if (currentStep === 3) {
            switchStep(4)
            return
        }

        if (currentStep === 4) {
            if (authenticated || root.browserReadOnly)
                switchStep(5)
            return
        }

        if (currentStep === 5)
            finishSetup()
    }

    function previousStep() {
        if (currentStep <= 0)
            return
        switchStep(currentStep - 1)
    }

    function primaryActionText() {
        if (currentStep === 2 && !legalConsentAccepted)
            return qsTrId("genycaster.setup.agree_continue")
        if (currentStep === 4 && root.browserReadOnly && !authenticated)
            return qsTrId("genycaster.setup.continue_as_guest")
        if (currentStep === 4 && !authenticated)
            return qsTrId("genycaster.setup.sign_in_required")
        if (currentStep === 5)
            return qsTrId("genycaster.setup.finish")
        return qsTrId("genycaster.setup.continue")
    }

    function primaryActionEnabled() {
        if (authenticationInProgress)
            return false
        if (currentStep === 2)
            return legalConsentAccepted || (legalTermsReviewed && legalPrivacyReviewed && legalTermsChecked && legalPrivacyChecked)
        if (currentStep === 4)
            return authenticated || root.browserReadOnly
        if (currentStep === 5)
            return (authenticated || root.browserReadOnly) && legalConsentAccepted
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
            if (authenticationRoute === "recovery")
                return qsTrId("genycaster.setup.recovery_working")
            return authenticationRoute === "farcaster"
                    ? qsTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request")
                    : qsTrId("genycaster.shell.connecting_securely_to_genyleap")
        }
        return qsTrId("genycaster.shell.sign_in_to_interact")
    }

    function phaseBody() {
        if (authenticationRoute === "recovery" && authenticationStarting)
            return qsTrId("genycaster.setup.recovery_working_body")
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

    onAuthenticatedChanged: {
        if (authenticated && currentStep === 4)
            Qt.callLater(() => switchStep(5))
        else
            Qt.callLater(synchronizeFlow)
    }

    onSetupCompletedChanged: {
        if (!setupCompleted) {
            highestReachedStep = returningAfterLogout ? 4 : 0
            Qt.callLater(() => switchStep(returningAfterLogout ? 4 : 0))
            return
        }
        highestReachedStep = 5
        Qt.callLater(synchronizeFlow)
    }

    onLegalConsentAcceptedChanged: {
        if (legalConsentAccepted) {
            legalTermsChecked = true
            legalPrivacyChecked = true
            legalTermsReviewed = true
            legalPrivacyReviewed = true
            if (currentStep === 2) {
                Qt.callLater(() => switchStep(3))
                return
            }
        }
        Qt.callLater(synchronizeFlow)
    }

    onVisibleChanged: {
        if (!visible)
            return
        if (legalConsentAccepted) {
            legalTermsChecked = true
            legalPrivacyChecked = true
            legalTermsReviewed = true
            legalPrivacyReviewed = true
        }
        forceActiveFocus()
        if (!setupCompleted) {
            if (returningAfterLogout)
                highestReachedStep = Math.max(highestReachedStep, 4)
            switchStep(returningAfterLogout ? 4 : 0)
        } else
            synchronizeFlow()
    }

    Component.onCompleted: {
        legalTermsChecked = legalConsentAccepted
        legalPrivacyChecked = legalConsentAccepted
        legalTermsReviewed = legalConsentAccepted
        legalPrivacyReviewed = legalConsentAccepted
        synchronizeFlow()
    }

    Keys.onEscapePressed: event => {
        if (legalReader.opened)
            legalReader.close()
        else if (authenticationInProgress)
            cancelAuthentication()
        event.accepted = true
    }

    NumberAnimation {
        id: stageEntrance
        target: root
        property: "stageReveal"
        from: 0.0
        to: 1.0
        duration: Theme.motionEmphasis
        easing.type: Easing.OutCubic
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 64

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.phone ? Theme.space4 : Theme.space6
                anchors.rightMargin: root.phone ? Theme.space4 : Theme.space6
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                RowLayout {
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Icon {
                        text: root.genyBrandIcon
                        color: Theme.accent
                        preferredSize: 26
                        Accessible.name: qsTrId("genycaster.app.genycaster")
                    }

                    ColumnLayout {
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.app.genycaster")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            horizontalAlignment: Theme.textStartAlignment
                        }
                        Label {
                            visible: !root.phone
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.release.beta_1_0") + " · " + qsTrId("genycaster.setup.workspace_label")
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                            horizontalAlignment: Theme.textStartAlignment
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    visible: !root.phone
                    text: root.stepTitle(root.currentStep)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                }

                Row {
                    visible: true
                    spacing: 6
                    layoutDirection: Theme.uiDirection
                    Repeater {
                        model: root.wizardSteps
                        delegate: Superellipse {
                            required property int modelData
                            width: modelData === root.currentStep ? 28 : 12
                            height: 4
                            curveDepth: 2
                            fillColor: modelData === root.currentStep ? Theme.primaryFill
                                 : modelData < root.currentStep ? Theme.inkMuted
                                 : Theme.borderStrong
                            Behavior on width {
                                NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                }

                Label {
                    visible: true
                    text: (root.wizardSteps.indexOf(root.currentStep) + 1) + " / " + root.wizardSteps.length
                    color: Theme.inkSubtle
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderMuted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            layoutDirection: Theme.uiDirection

            Rectangle {
                visible: !root.compact
                Layout.preferredWidth: 252
                Layout.fillHeight: true
                color: Theme.surface
                border.width: 0

                Rectangle {
                    x: Theme.rtl ? 0 : parent.width - width
                    width: 1
                    height: parent.height
                    color: Theme.borderMuted
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: Theme.space7
                    anchors.bottomMargin: Theme.space5
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.progress_label")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightBold
                            horizontalAlignment: Theme.textStartAlignment
                        }

                        Item {
                            Layout.preferredHeight: 2
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // Each step owns its own status icon and text. Avoid a
                        // decorative rail passing through unrelated hit targets.
                        Column {
                            id: stepRail
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            spacing: 6

                            Repeater {
                                model: root.wizardSteps

                                delegate: Button {
                                    id: stepButton
                                    required property int modelData
                                    readonly property bool complete: root.stepComplete(modelData)
                                    readonly property bool current: modelData === root.currentStep
                                    width: parent.width
                                    height: 58
                                    enabled: true
                                    opacity: 1.0
                                    hoverEnabled: true
                                    focusPolicy: Qt.StrongFocus
                                    Accessible.name: root.stepTitle(modelData)
                                    Accessible.description: current
                                                            ? qsTrId("genycaster.setup.title") : ""
                                    onClicked: {
                                        if (root.stepUnlocked(modelData))
                                            root.switchStep(modelData)
                                    }

                                    background: Superellipse {
                                        fillColor: stepButton.current ? Theme.surfaceSelected
                                                   : stepButton.hovered ? Theme.surfaceHover : "transparent"
                                        borderWidth: Theme.keyboardFocusVisible(stepButton) ? 2 : 0
                                        borderColor: Theme.focus
                                        curveDepth: Theme.curveControl
                                    }

                                    contentItem: RowLayout {
                                        spacing: Theme.space3
                                        layoutDirection: Theme.uiDirection

                                        Superellipse {
                                            Layout.preferredWidth: 30
                                            Layout.preferredHeight: 30
                                            curveDepth: 15
                                            exponent: 2.01
                                            fillColor: stepButton.current ? Theme.primaryFill
                                                       : stepButton.hovered ? Theme.surfaceHover : Theme.surfaceMuted
                                            borderWidth: Theme.strokeStandard
                                            borderColor: stepButton.current ? Theme.primaryFill
                                                         : stepButton.hovered ? Theme.accent : Theme.borderMuted

                                            Icon {
                                                anchors.centerIn: parent
                                                text: root.stepIcon(modelData)
                                                color: stepButton.current ? Theme.accentInk
                                                       : stepButton.hovered ? Theme.accent : Theme.inkMuted
                                                preferredSize: Theme.iconSizeSmall
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.stepTitle(modelData)
                                            color: stepButton.current || stepButton.hovered
                                                   ? Theme.ink : Theme.inkMuted
                                            font.pixelSize: Theme.typeLabelSize
                                            font.weight: stepButton.current || stepButton.hovered
                                                         ? Theme.weightDemiBold : Theme.weightMedium
                                            horizontalAlignment: Theme.textStartAlignment
                                            elide: Text.ElideNone
                                            wrapMode: Text.WordWrap
                                            maximumLineCount: 2
                                        }

                                        Icon {
                                            visible: stepButton.current || stepButton.hovered
                                            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                            color: Theme.accent
                                            preferredSize: Theme.iconSizeSmall
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.space2
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Flickable {
                    id: stageFlick
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: width
                    contentHeight: stageCanvas.height
                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height
                    opacity: 0.82 + root.stageReveal * 0.18

                    transform: Translate {
                        y: Theme.reduceMotion ? 0 : (1.0 - root.stageReveal) * 12
                    }

                    ScrollBar.vertical: GenyScrollBar {
                        policy: stageFlick.contentHeight > stageFlick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                    }

                    Item {
                        id: stageCanvas
                        width: Math.min(stageFlick.width - (root.phone ? Theme.space4 * 2 : Theme.space6 * 2),
                                        root.compact ? root.setupContentWidth : 720)
                        x: Math.max(root.phone ? Theme.space4 : Theme.space6,
                                    Math.round((stageFlick.width - width) / 2))
                        height: Math.max(stageFlick.height,
                                         stageLoader.y + stageLoader.implicitHeight + Theme.space6)

                        Loader {
                            id: stageLoader
                            width: parent.width
                            y: root.phone ? Theme.space5 : Math.max(Theme.space7,
                                (stageFlick.height - stageLoader.implicitHeight) * 0.16)
                            sourceComponent: root.currentStep === 0 ? welcomeStage
                                           : root.currentStep === 1 ? personalizeStage
                                           : root.currentStep === 2 ? privacyStage
                                           : root.currentStep === 3 ? networkStage
                                           : root.currentStep === 4 ? accountStage
                                           : readyStage
                        }
                    }
                }

                Item {
                    // This footer must remain visible above mobile browser
                    // controls and safe areas even when the stage scrolls.
                    id: setupFooter
                    z: 20
                    Layout.fillWidth: true
                    Layout.minimumHeight: root.phone ? 72 : 72
                    Layout.preferredHeight: root.phone ? 78 : 72
                    Layout.maximumHeight: root.phone ? 92 : 72

                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width
                        height: 1
                        color: Theme.borderMuted
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: root.phone ? Theme.space4 : Theme.space6
                        anchors.rightMargin: root.phone ? Theme.space4 : Theme.space6
                        spacing: Theme.space3
                        layoutDirection: Theme.uiDirection

                        ActionButton {
                            visible: root.currentStep > 0
                            text: qsTrId("genycaster.setup.back")
                            iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                            quiet: true
                            enabled: !root.authenticationInProgress
                            onClicked: root.previousStep()
                        }

                        Item { Layout.fillWidth: !root.phone }

                        SetupPrimaryButton {
                            visible: root.currentStep !== 4 || root.authenticated || root.browserReadOnly
                            Layout.fillWidth: root.phone
                            Layout.maximumWidth: root.phone ? 420 : 300
                            Layout.minimumWidth: root.phone ? 0 : 168
                            text: root.primaryActionText()
                            enabled: root.primaryActionEnabled()
                            iconGlyph: root.currentStep === 5 ? Theme.iconCheck : ""
                            showArrow: root.currentStep !== 5
                            onClicked: root.nextStep()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: legalReader
        modal: true
        focus: true
        width: root.phone ? root.width : Math.min(root.width - Theme.space7 * 2, 620)
        height: root.phone ? Math.min(root.height - Theme.space4, 640)
                           : Math.min(root.height - Theme.space6 * 2, 510)
        x: root.phone ? 0 : Math.round((root.width - width) / 2)
        y: root.phone ? root.height - height : Math.round((root.height - height) / 2)
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "y"; from: root.phone ? root.height : Math.round((root.height - legalReader.height) / 2);
                                  to: root.phone ? root.height - legalReader.height : Math.round((root.height - legalReader.height) / 2);
                                  duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic }
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.reduceMotion ? 0 : Theme.motionFast }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "y"; to: root.phone ? root.height : Math.round((root.height - legalReader.height) / 2);
                                  duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.InCubic }
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.reduceMotion ? 0 : Theme.motionFast }
            }
        }
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            legalReaderFlick.contentY = 0
            Qt.callLater(() => {
                root.legalReaderReachedEnd =
                    root.legalConsentAccepted
                    || legalReaderFlick.contentHeight <= legalReaderFlick.height + 2
            })
        }

        background: Superellipse {
            fillColor: Theme.surface
            borderWidth: 1
            borderColor: Theme.borderStrong
            curveDepth: Theme.curveContainer
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                visible: root.phone
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.space2
                width: 40
                height: 4
                color: Theme.borderStrong
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? 64 : 70
                Layout.leftMargin: Theme.space4
                Layout.rightMargin: Theme.space4
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                Icon {
                    text: root.legalReaderKind === "privacy" ? "system/lock" : Theme.iconDocument
                    color: Theme.inkMuted
                    preferredSize: Theme.iconSize
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: root.legalReaderKind === "privacy"
                              ? qsTrId("genycaster.setup.privacy_title")
                              : qsTrId("genycaster.setup.terms_title")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                        horizontalAlignment: Theme.textStartAlignment
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.legalReaderKind === "privacy"
                              ? "genyleap.com/privacy"
                              : "genyleap.com/terms-of-use"
                        color: Theme.inkSubtle
                        font.family: Theme.codeFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Theme.textStartAlignment
                        elide: Text.ElideMiddle
                    }
                }

                ActionButton {
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.shell.cancel")
                    onClicked: legalReader.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderMuted
            }

            Flickable {
                id: legalReaderFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: legalReaderColumn.implicitHeight + Theme.space5 * 2
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                onContentYChanged: {
                    if (contentY + height >= contentHeight - 4)
                        root.legalReaderReachedEnd = true
                }
                onContentHeightChanged: {
                    if (contentHeight <= height + 2)
                        root.legalReaderReachedEnd = true
                }

                ScrollBar.vertical: GenyScrollBar {
                    policy: legalReaderFlick.contentHeight > legalReaderFlick.height
                            ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                Column {
                    id: legalReaderColumn
                    width: legalReaderFlick.width - Theme.space5 * 2
                    x: Theme.space5
                    y: Theme.space5
                    spacing: Theme.space4

                    Label {
                        width: parent.width
                        text: qsTrId("genycaster.setup.legal_summary_label")
                        color: Theme.accent
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        horizontalAlignment: Theme.textStartAlignment
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: root.legalReaderKind === "privacy"
                              ? qsTrId("genycaster.setup.privacy_body")
                              : qsTrId("genycaster.setup.terms_body")
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
                        font.weight: Theme.weightMedium
                        lineHeight: 1.5
                        horizontalAlignment: Theme.textStartAlignment
                        wrapMode: Text.WordWrap
                    }

                    ActionButton {
                        width: Math.min(parent.width, 280)
                        compact: true
                        quiet: false
                        text: root.legalReaderKind === "privacy"
                              ? qsTrId("genycaster.setup.open_privacy")
                              : qsTrId("genycaster.setup.open_terms")
                        onClicked: Qt.openUrlExternally(
                            root.legalReaderKind === "privacy"
                            ? "https://genyleap.com/privacy"
                            : "https://genyleap.com/terms-of-use")
                    }

                    Label {
                        width: parent.width
                        text: qsTrId("genycaster.setup.legal_governing_note")
                        wrapMode: Text.WordWrap
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        lineHeight: 1.45
                        horizontalAlignment: Theme.textStartAlignment
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderMuted
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? 88 : 76
                Layout.leftMargin: Theme.space4
                Layout.rightMargin: Theme.space4
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                RowLayout {
                    visible: !root.phone
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Icon {
                        text: root.legalReaderReachedEnd ? Theme.iconCheck : Theme.iconInfo
                        color: root.legalReaderReachedEnd ? Theme.ink : Theme.inkSubtle
                        preferredSize: 16
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.legalReaderReachedEnd
                              ? qsTrId("genycaster.setup.confirm_document")
                              : qsTrId("genycaster.setup.scroll_to_continue")
                        color: root.legalReaderReachedEnd ? Theme.inkMuted : Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                }

                SetupPrimaryButton {
                    Layout.fillWidth: root.phone
                    Layout.minimumWidth: root.phone ? 0 : 180
                    Layout.maximumWidth: root.phone ? 10000 : 300
                    text: qsTrId("genycaster.setup.mark_document_reviewed")
                    enabled: root.legalReaderReachedEnd
                    iconGlyph: Theme.iconCheck
                    showArrow: false
                    onClicked: root.acceptLegalReader()
                }
            }
        }
    }

    Component {
        id: welcomeStage

        ColumnLayout {
            spacing: Theme.space3

            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: root.genyBrandIcon
                title: qsTrId("genycaster.setup.welcome_title")
                subtitle: qsTrId("genycaster.setup.language_orbit_hint")
                maximumTextWidth: 520
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? Theme.space3 : Theme.space4
            }

            SetupLanguageSelector {
                id: setupLanguageSelector
                Layout.fillWidth: true
                Layout.maximumWidth: 430
                model: root.greetings
                selectedLabel: localizationController.languageLabel
                selectedDetail: localizationController.regionalLocaleLabel

                Component.onCompleted: currentIndex =
                    root.greetingIndexForTag(localizationController.effectiveLanguageTag)

                onActivated: (index, entry) => {
                    if (entry && entry.tag)
                        localizationController.setLanguageTag(entry.tag)
                }

                Connections {
                    target: localizationController
                    function onEffectiveLanguageTagChanged() {
                        if (!setupLanguageSelector.userInteracted) {
                            setupLanguageSelector.currentIndex =
                                root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: personalizeStage

        ColumnLayout {
            spacing: Theme.space4

            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconPalette
                title: qsTrId("genycaster.setup.welcome_personal_title")
                subtitle: qsTrId("genycaster.setup.personalize_body")
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? Theme.space3 : Theme.space5
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: personalizeGrid.implicitHeight
                fillColor: "transparent"
                borderWidth: 0
                borderColor: "transparent"
                curveDepth: Theme.curveContainer

                GridLayout {
                    id: personalizeGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 0
                    columns: 1
                    columnSpacing: 0
                    rowSpacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: Theme.iconLanguage
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSize
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.language_region")
                                    color: Theme.ink
                                    font.pixelSize: root.setupSectionTitleSize
                                    font.weight: Theme.weightDemiBold
                                    horizontalAlignment: Theme.textStartAlignment
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: localizationController.languageLabel
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Theme.textStartAlignment
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.phone ? 1 : 2
                            columnSpacing: Theme.space3
                            rowSpacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.locale.regional_format")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Theme.textStartAlignment
                                }

                                GenyComboBox {
                                    Layout.fillWidth: true
                                    model: localizationController.availableRegionalLocales
                                    textRole: "label"
                                    valueRole: "id"
                                    currentIndex: root.indexFor(
                                        localizationController.availableRegionalLocales,
                                        localizationController.regionalLocaleTag)
                                    onActivated: localizationController.setRegionalLocaleTag(
                                        localizationController.availableRegionalLocales[index].id)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.locale.time_zone")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Theme.textStartAlignment
                                }

                                GenyComboBox {
                                    Layout.fillWidth: true
                                    model: localizationController.availableTimeZones
                                    textRole: "label"
                                    valueRole: "id"
                                    currentIndex: root.indexFor(
                                        localizationController.availableTimeZones,
                                        localizationController.timeZoneId)
                                    onActivated: localizationController.setTimeZoneId(
                                        localizationController.availableTimeZones[index].id)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.borderMuted
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: Theme.iconPalette
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSize
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.settings.appearance")
                                color: Theme.ink
                                font.pixelSize: root.setupSectionTitleSize
                                font.weight: Theme.weightDemiBold
                                horizontalAlignment: Theme.textStartAlignment
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Repeater {
                                model: [
                                    { id: "system", label: qsTrId("genycaster.settings.mode_system"), icon: Theme.iconSystem },
                                    { id: "light", label: qsTrId("genycaster.settings.mode_light"), icon: Theme.iconSun },
                                    { id: "dark", label: qsTrId("genycaster.settings.mode_dark"), icon: Theme.iconMoon }
                                ]

                                delegate: Button {
                                    id: modeButton
                                    required property var modelData
                                    readonly property bool selected: appearanceController.themeMode === modelData.id
                                    Layout.fillWidth: true
                                    implicitHeight: 44
                                    hoverEnabled: true
                                    focusPolicy: Qt.StrongFocus
                                    Accessible.name: modelData.label
                                    Accessible.checked: selected
                                    onClicked: appearanceController.setThemeMode(modelData.id)

                                    background: Superellipse {
                                        fillColor: modeButton.selected
                                                   ? Theme.primaryFill
                                                   : modeButton.down ? Theme.surfacePressed
                                                   : modeButton.hovered ? Theme.surfaceHover : Theme.surface
                                        borderWidth: modeButton.selected ? 0
                                                   : Theme.keyboardFocusVisible(modeButton) ? 2 : Theme.strokeStandard
                                        borderColor: Theme.keyboardFocusVisible(modeButton) ? Theme.focus : Theme.borderMuted
                                        curveDepth: Theme.curveControl
                                    }

                                    contentItem: RowLayout {
                                        spacing: Theme.space2
                                        layoutDirection: Theme.uiDirection

                                        Icon {
                                            text: modeButton.modelData.icon
                                            color: modeButton.selected
                                                   ? Theme.accentInk : Theme.inkMuted
                                            preferredSize: Theme.iconSizeSmall
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: modeButton.modelData.label
                                            color: modeButton.selected
                                                   ? Theme.accentInk : Theme.ink
                                            font.pixelSize: Theme.typeCaptionSize
                                            font.weight: Theme.weightDemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.phone ? Theme.space1 : Theme.space2
                            layoutDirection: Theme.uiDirection

                            Repeater {
                                model: [
                                    { id: "geny", color: Theme.swatchGeny },
                                    { id: "ocean", color: Theme.swatchOcean },
                                    { id: "violet", color: Theme.swatchViolet },
                                    { id: "ember", color: Theme.swatchEmber },
                                    { id: "rose", color: Theme.swatchRose },
                                    { id: "mint", color: Theme.swatchMint },
                                    { id: "mono", color: Theme.swatchMono }
                                ]

                                delegate: Button {
                                    id: swatchButton
                                    required property var modelData
                                    readonly property bool selected: appearanceController.colorThemeId === modelData.id
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 48
                                    implicitHeight: 40
                                    focusPolicy: Qt.StrongFocus
                                    Accessible.name: modelData.id
                                    Accessible.checked: selected
                                    onClicked: appearanceController.setColorThemeId(modelData.id)

                                    background: Superellipse {
                                        curveDepth: 8
                                        fillColor: "transparent"
                                        borderWidth: Theme.keyboardFocusVisible(swatchButton) ? 2 : 0
                                        borderColor: Theme.focus
                                    }

                                    contentItem: Item {
                                        Superellipse {
                                            anchors.centerIn: parent
                                            width: swatchButton.selected ? 30 : 24
                                            height: width
                                            curveDepth: width / 2
                                            exponent: 2.01
                                            fillColor: modelData.color
                                            borderWidth: swatchButton.selected ? 3 : 1
                                            borderColor: swatchButton.selected
                                                          ? Theme.surface : Theme.borderStrong

                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: Theme.motionStandard
                                                    easing.type: Easing.OutCubic
                                                }
                                            }

                                            Icon {
                                                anchors.centerIn: parent
                                                visible: swatchButton.selected
                                                text: Theme.iconCheck
                                                color: Theme.contrastInk(modelData.color)
                                                preferredSize: 15
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        GenySwitch {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.reduce_motion")
                            checked: appearanceController.reduceMotion
                            onToggled: appearanceController.setReduceMotion(checked)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: privacyStage

        ColumnLayout {
            spacing: Theme.space4

            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: "system/lock"
                title: qsTrId("genycaster.setup.legal_title")
                subtitle: qsTrId("genycaster.setup.privacy_compact_intro")
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? Theme.space3 : Theme.space5
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 1
                layoutDirection: Theme.uiDirection
                columnSpacing: 0
                rowSpacing: Theme.space2

                Repeater {
                    model: [
                        {
                            title: qsTrId("genycaster.setup.terms_title"),
                            body: qsTrId("genycaster.setup.terms_summary"),
                            icon: Theme.iconDocument,
                            privacy: false
                        },
                        {
                            title: qsTrId("genycaster.setup.privacy_title"),
                            body: qsTrId("genycaster.setup.privacy_summary"),
                            icon: "system/lock",
                            privacy: true
                        }
                    ]

                    delegate: SetupActionCard {
                        required property var modelData
                        readonly property bool reviewed: modelData.privacy
                                                         ? root.legalPrivacyReviewed
                                                         : root.legalTermsReviewed
                        Layout.fillWidth: true
                        title: modelData.title
                        subtitle: modelData.body
                        iconGlyph: modelData.icon
                        selected: reviewed
                        trailingText: reviewed ? qsTrId("genycaster.setup.document_accepted") : ""
                        onClicked: root.openLegalReader(modelData.privacy ? "privacy" : "terms")
                    }
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.legalConsentAccepted
                      ? qsTrId("genycaster.setup.consent_recorded")
                      : qsTrId("genycaster.setup.legal_click_to_review")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
        }
    }

    Component {
        id: networkStage

        ColumnLayout {
            spacing: Theme.space4

            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconWifi
                title: qsTrId("genycaster.setup.connection_mode_title")
                subtitle: qsTrId("genycaster.setup.connection_mode_help")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 2
                columnSpacing: Theme.space2
                rowSpacing: Theme.space2
                layoutDirection: Theme.uiDirection

                SetupActionCard {
                    Layout.fillWidth: true
                    title: qsTrId("genycaster.setup.network_hypersnap_category").arg(qsTrId("genycaster.setup.decentralized_label"))
                    subtitle: qsTrId("genycaster.setup.hypersnap_explainer")
                    iconGlyph: "quilibrium/hypersnap"
                    primary: presentationBridge.preferredNetwork === "hypersnap"
                    selected: presentationBridge.preferredNetwork === "hypersnap"
                    onClicked: presentationBridge.setPreferredNetwork("hypersnap")
                }

                SetupActionCard {
                    Layout.fillWidth: true
                    title: qsTrId("genycaster.setup.network_farcaster_category").arg(qsTrId("genycaster.setup.centralized_service_label"))
                    subtitle: presentationBridge.farcasterReadConfigured
                              ? qsTrId("genycaster.setup.farcaster_explainer")
                              : qsTrId("genycaster.setup.farcaster_not_configured")
                    iconGlyph: "brands/farcaster"
                    primary: presentationBridge.preferredNetwork === "farcaster"
                    selected: presentationBridge.preferredNetwork === "farcaster"
                    onClicked: presentationBridge.setPreferredNetwork("farcaster")
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.network_connection")
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.if_farcaster_relay_cannot_connect_through_a_system_proxy")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    ActionButton {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.system_proxy")
                        selected: nodeSelectionController.proxyMode === "system"
                        onClicked: nodeSelectionController.setProxyMode("system")
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.direct")
                        selected: nodeSelectionController.proxyMode === "direct"
                        onClicked: nodeSelectionController.setProxyMode("direct")
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: nodeSelectionController.proxyRestartRequired
                    text: qsTrId("genycaster.setup.restart_genycaster_to_apply_the_connection_change")
                    color: Theme.warning
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.network_settings_explainer")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Theme.textStartAlignment
            }

            ActionButton {
                Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                text: qsTrId("genycaster.setup.advanced_nodes")
                iconGlyph: Theme.iconWifi
                quiet: true
                onClicked: root.showAdvancedNodes = !root.showAdvancedNodes
            }

            SetupActionCard {
                visible: root.showAdvancedNodes && presentationBridge.preferredNetwork === "farcaster"
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.farcaster_hosted_node")
                subtitle: presentationBridge.farcasterReadConfigured
                          ? presentationBridge.farcasterReadEndpoint
                          : qsTrId("genycaster.setup.farcaster_missing_key")
                iconGlyph: "brands/farcaster"
                selected: presentationBridge.farcasterReadConfigured
                          && presentationBridge.preferredNetwork === "farcaster"
                enabled: presentationBridge.farcasterReadConfigured
                onClicked: presentationBridge.setPreferredNetwork("farcaster")
            }

            ColumnLayout {
                visible: root.showAdvancedNodes && presentationBridge.preferredNetwork === "farcaster"
                Layout.fillWidth: true
                spacing: Theme.space2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.farcaster_hub_directory")
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.raw_hub_apis_are_for_protocol_messages_and_health")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    enabled: false
                    title: qsTrId("genycaster.setup.pinata_hub_farcaster")
                    subtitle: qsTrId("genycaster.setup.hub_pinata_cloud_hub_v1_api_not_a_compatible")
                    iconGlyph: "brands/farcaster"
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    enabled: false
                    title: qsTrId("genycaster.setup.neynar_hub_farcaster")
                    subtitle: qsTrId("genycaster.setup.authenticated_hub_access_separate_from_the_neynar_indexed_feed")
                    iconGlyph: "brands/farcaster"
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    enabled: false
                    title: qsTrId("genycaster.setup.standard_crypto_hub_farcaster")
                    subtitle: qsTrId("genycaster.setup.hub_endpoint_availability_and_authentication_must_be_verified_before")
                    iconGlyph: "brands/farcaster"
                }
            }

            SetupActionCard {
                visible: root.showAdvancedNodes && presentationBridge.preferredNetwork === "hypersnap"
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.network_automatic")
                subtitle: qsTrId("genycaster.setup.network_experimental")
                iconGlyph: Theme.iconFidelity
                primary: nodeSelectionController.automatic
                selected: nodeSelectionController.automatic
                onClicked: nodeSelectionController.useAutomaticSelection()
            }

            RowLayout {
                visible: root.showAdvancedNodes && presentationBridge.preferredNetwork === "hypersnap"
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.network_body")
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.textStartAlignment
                    elide: Text.ElideRight
                }

                ActionButton {
                    text: qsTrId("genycaster.setup.network_retest")
                    iconGlyph: Theme.iconRefresh
                    quiet: true
                    busy: nodeSelectionController.probing
                    onClicked: nodeSelectionController.refresh()
                }
            }

            ColumnLayout {
                visible: root.showAdvancedNodes && presentationBridge.preferredNetwork === "hypersnap"
                Layout.fillWidth: true
                spacing: Theme.space2

                Repeater {
                    model: nodeSelectionController.nodes

                    delegate: SetupActionCard {
                        required property var modelData
                        Layout.fillWidth: true
                        title: modelData.label
                        subtitle: modelData.endpoint
                        iconGlyph: "quilibrium/hypersnap"
                        trailingText: modelData.status === "checking" ? "…"
                                      : modelData.latencyMs >= 0 ? modelData.latencyMs + " ms"
                                      : modelData.status === "auth-gated" ? qsTrId("genycaster.setup.requires_authentication")
                                      : modelData.status === "incompatible" ? qsTrId("genycaster.setup.not_a_hypersnap_node")
                                      : modelData.status === "offline"
                                        ? qsTrId("genycaster.setup.network_unreachable") : "—"
                        primary: modelData.selected && !nodeSelectionController.automatic
                        selected: modelData.selected
                        enabled: modelData.status === "online" || modelData.selected
                        onClicked: nodeSelectionController.selectNode(modelData.id)
                    }
                }
            }

            Superellipse {
                visible: root.showAdvancedNodes && presentationBridge.preferredNetwork === "hypersnap"
                Layout.fillWidth: true
                implicitHeight: networkStateRow.implicitHeight + Theme.space2 * 2
                fillColor: "transparent"
                borderWidth: 0
                borderColor: "transparent"
                curveDepth: Theme.curveControl

                RowLayout {
                    id: networkStateRow
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Icon {
                        text: nodeSelectionController.restartRequired
                              ? Theme.iconWarning : Theme.iconCheck
                        color: nodeSelectionController.restartRequired ? Theme.warning : Theme.accent
                        preferredSize: Theme.iconSize
                    }

                    Label {
                        Layout.fillWidth: true
                        text: nodeSelectionController.restartRequired
                              ? qsTrId("genycaster.setup.network_restart_note")
                              : qsTrId("genycaster.setup.network_current_note")
                        color: nodeSelectionController.restartRequired ? Theme.warning : Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        horizontalAlignment: Theme.textStartAlignment
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Component {
        id: accountStage

        ColumnLayout {
            spacing: Theme.space4

            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconUser
                title: root.browserReadOnly && !root.authenticated
                       ? qsTrId("genycaster.setup.recovery_title")
                       : root.authenticated
                         ? qsTrId("genycaster.setup.account_verified")
                         : root.accountIntent === "new"
                           ? qsTrId("genycaster.setup.new_profile")
                           : qsTrId("genycaster.setup.sign_in_required")
                subtitle: root.browserReadOnly && !root.authenticated
                          ? qsTrId("genycaster.setup.recovery_description")
                          : !root.authenticated && root.accountIntent === "new"
                            ? qsTrId("genycaster.setup.profile_creation_browser")
                            : root.authenticated || root.authenticationInProgress
                              ? root.phaseBody()
                              : qsTrId("genycaster.setup.account_methods_explainer")
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? Theme.space3 : Theme.space5
            }

            ColumnLayout {
                visible: root.authenticated
                Layout.fillWidth: true
                spacing: Theme.space2

                DirectionAwareLabel {
                    visible: !root.signerReady
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.viewer_title")
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    visible: !root.signerReady
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.viewer_description")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.signerStatusText
                    color: root.signerReady ? Theme.success : Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                DirectionAwareLabel {
                    visible: root.signerErrorText.length > 0
                    Layout.fillWidth: true
                    text: root.signerErrorText
                    color: Theme.warning
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                ActionButton {
                    visible: !root.signerReady && !root.signerBusy
                    Layout.fillWidth: true
                    text: root.browserReadOnly
                          ? qsTrId("genycaster.shell.enable_publishing_with_recovery_phrase")
                          : qsTrId("genycaster.shell.enable_interactions")
                    onClicked: root.authorizeSigner()
                }
            }

            Superellipse {
                visible: root.authenticated
                Layout.fillWidth: true
                implicitHeight: verifiedGrid.implicitHeight + root.setupCardPadding * 2
                fillColor: root.setupCard
                borderWidth: Theme.strokeStandard
                borderColor: root.setupBorder
                curveDepth: Theme.curveContainer

                RowLayout {
                    id: verifiedGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: root.setupCardPadding
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection

                    Avatar {
                        sourceUrl: root.accountAvatarUrl
                        displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                        avatarSize: 56
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            color: Theme.ink
                            font.pixelSize: root.setupSectionTitleSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }

                        DirectionAwareLabel {
                            visible: root.accountUsername.length > 0
                            Layout.fillWidth: true
                            text: "@" + root.accountUsername
                            forceLeftToRight: true
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: Theme.space1
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: Theme.iconCheck
                                color: Theme.accent
                                preferredSize: Theme.iconSizeSmall
                            }

                            Label {
                                text: root.authenticationRoute === "recovery"
                                      ? qsTrId("genycaster.setup.recovery_verified")
                                      : root.authenticationRoute === "farcaster"
                                        ? qsTrId("genycaster.shell.farcaster_sign_in_openproof_verification")
                                        : qsTrId("genycaster.shell.verified_by_openproof")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }
                    }

                    Icon {
                        text: Theme.iconCheck
                        color: Theme.accent
                        preferredSize: Theme.iconSize
                    }
                }
            }

            DirectionAwareLabel {
                visible: root.browserReadOnly && !root.authenticated
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.recovery_privacy")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                visible: !root.authenticated
                Layout.fillWidth: true
                spacing: Theme.space3

                SetupActionCard {
                    visible: root.browserReadOnly
                    Layout.fillWidth: true
                    title: qsTrId("genycaster.setup.recovery_button")
                    subtitle: qsTrId("genycaster.setup.recovery_button_body")
                    iconGlyph: Theme.iconUser
                    primary: true
                    busy: root.authenticationRoute === "recovery" && root.authenticationInProgress
                    enabled: root.runtimeReady && !root.authenticationInProgress
                    onClicked: root.recoverAccount()
                }

                ActionButton {
                    visible: root.browserReadOnly
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.other_sign_in_methods")
                    quiet: true
                    enabled: !root.authenticationInProgress
                    onClicked: root.alternateMethodsExpanded = !root.alternateMethodsExpanded
                }

                Superellipse {
                    visible: root.browserReadOnly && root.alternateMethodsExpanded
                    Layout.fillWidth: true
                    implicitHeight: viewerNote.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceMuted
                    borderWidth: Theme.strokeStandard
                    borderColor: root.setupBorder
                    curveDepth: Theme.curveControl
                    ColumnLayout {
                        id: viewerNote
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.viewer_title")
                            font.weight: Theme.weightDemiBold
                            color: Theme.ink
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.viewer_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                    }
                }

                GridLayout {
                    visible: !root.browserReadOnly
                    Layout.fillWidth: true
                    columns: root.phone ? 1 : 2
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    SetupActionCard {
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.setup.existing_profile")
                        subtitle: qsTrId("genycaster.setup.existing_profile_body")
                        iconGlyph: Theme.iconSignIn
                        primary: root.accountIntent === "existing"
                        selected: root.accountIntent === "existing"
                        enabled: !root.authenticationInProgress
                        onClicked: root.accountIntent = "existing"
                    }

                    SetupActionCard {
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.setup.new_profile")
                        subtitle: qsTrId("genycaster.setup.new_profile_body")
                        iconGlyph: Theme.iconUser
                        primary: false
                        selected: false
                        trailingText: qsTrId("genycaster.shell.coming_soon")
                        enabled: false
                    }
                }

                ColumnLayout {
                    // Registration is not implemented inside Genycaster yet.
                    // Do not expose a misleading action until secure FID creation exists.
                    visible: false
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    SetupActionCard {
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.setup.open_registration")
                        subtitle: qsTrId("genycaster.setup.registration_handoff")
                        iconGlyph: "brands/farcaster"
                        primary: true
                        enabled: !root.authenticationInProgress
                        onClicked: root.openRegistration()
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.already_created")
                        quiet: true
                        enabled: !root.authenticationInProgress
                        onClicked: root.accountIntent = "existing"
                    }
                }

                ColumnLayout {
                    visible: root.accountIntent === "existing"
                             && (!root.browserReadOnly || root.alternateMethodsExpanded)
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    SetupActionCard {
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.shell.continue_with_genyleap")
                        subtitle: qsTrId("genycaster.setup.account_genyleap_body")
                        iconGlyph: root.genyBrandIcon
                        primary: root.activeSignInRoute === "openproof"
                        busy: root.authenticationInProgress && root.authenticationRoute === "openproof"
                        showChevron: !root.authenticationInProgress
                        enabled: root.authenticationAvailable && !root.authenticationInProgress
                        onClicked: root.continueWithGenyleap()
                    }

                    SetupActionCard {
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.shell.sign_in_with_farcaster")
                        subtitle: qsTrId("genycaster.setup.account_farcaster_body")
                        iconGlyph: "brands/farcaster"
                        primary: root.activeSignInRoute === "farcaster"
                        busy: root.authenticationInProgress && root.authenticationRoute === "farcaster"
                        showChevron: !root.authenticationInProgress
                        enabled: root.authenticationAvailable && !root.authenticationInProgress
                        onClicked: root.continueWithFarcaster()
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.authentication_explainer")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Theme.textStartAlignment
                    }
                }
            }

            Superellipse {
                visible: root.authenticationApprovalAvailable
                Layout.fillWidth: true
                implicitHeight: approvalRow.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeStandard
                borderColor: root.setupBorder
                curveDepth: Theme.curveControl

                RowLayout {
                    id: approvalRow
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection

                    GenySpinner {
                        running: root.authenticationInProgress
                        color: Theme.accent
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.phaseBody()
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }

                    SetupPrimaryButton {
                        Layout.fillWidth: root.phone
                        Layout.minimumWidth: root.phone ? 0 : 180
                        Layout.maximumWidth: 300
                        text: root.authenticationRoute === "farcaster"
                              ? qsTrId("genycaster.shell.open_farcaster")
                              : qsTrId("genycaster.shell.open_genyleap")
                        iconGlyph: Theme.iconArrowUpRight
                        showArrow: false
                        onClicked: root.openApproval()
                    }
                }
            }

            ActionButton {
                visible: root.authenticationInProgress
                Layout.alignment: Qt.AlignHCenter
                text: qsTrId("genycaster.shell.cancel")
                quiet: true
                onClicked: root.cancelAuthentication()
            }

            Superellipse {
                visible: root.accountIntent === "existing" && root.authenticationErrorText.length > 0
                Layout.fillWidth: true
                implicitHeight: authErrorRow.implicitHeight + Theme.space3 * 2
                fillColor: Theme.dangerSurface
                borderWidth: 1
                borderColor: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.35)
                curveDepth: Theme.curveControl

                RowLayout {
                    id: authErrorRow
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection

                    Icon {
                        text: Theme.iconWarning
                        color: Theme.danger
                        preferredSize: 18
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.authenticationErrorText
                        color: Theme.ink
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                visible: !root.runtimeReady
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                GenySpinner {
                    running: true
                    color: Theme.accent
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.bridge.starting_local_read_runtime")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
        }
    }

    Component {
        id: readyStage

        ColumnLayout {
            spacing: Theme.space4

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: readyHero.implicitHeight
                fillColor: "transparent"
                borderWidth: 0
                borderColor: "transparent"
                curveDepth: Theme.curveContainer

                GridLayout {
                    id: readyHero
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 0
                    columns: 1
                    layoutDirection: Theme.uiDirection
                    columnSpacing: 0
                    rowSpacing: Theme.space3

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.phone ? 136 : 156

                        Avatar {
                            anchors.centerIn: parent
                            sourceUrl: root.accountAvatarUrl
                            displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            avatarSize: 76
                        }

                        Superellipse {
                            width: 28
                            height: 28
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 28
                            anchors.verticalCenterOffset: 28
                            fillColor: Theme.primaryFill
                            curveDepth: 10
                            borderWidth: 2
                            borderColor: root.setupCard

                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconCheck
                                color: Theme.accentInk
                                preferredSize: 17
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        Label {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.status.ready")
                            color: Theme.ink
                            font.pixelSize: root.setupPageTitleSize
                            font.weight: Theme.weightBold
                            lineHeight: Theme.typeTitle2LineHeight
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.accountName.length > 0
                                  ? qsTrId("genycaster.setup.ready_for").arg(root.accountName)
                                  : qsTrId("genycaster.setup.ready_generic")
                            color: Theme.inkMuted
                            font.pixelSize: root.setupBodySize
                            lineHeight: Theme.typeLabelLineHeight
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: Theme.iconCheck
                                color: Theme.accent
                                preferredSize: Theme.iconSizeSmall
                            }

                            Label {
                                text: qsTrId("genycaster.setup.consent_recorded")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightMedium
                            }
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 2
                columnSpacing: Theme.space2
                rowSpacing: Theme.space2
                layoutDirection: Theme.uiDirection

                SetupActionCard {
                    Layout.fillWidth: true
                    compactCard: true
                    title: localizationController.languageLabel
                    subtitle: localizationController.regionalLocaleLabel
                    iconGlyph: Theme.iconLanguage
                    onClicked: root.switchStep(0)
                }

                SetupActionCard {
                    Layout.fillWidth: true
                    compactCard: true
                    title: root.accentCode()
                    subtitle: appearanceController.themeMode
                    iconGlyph: Theme.iconPalette
                    onClicked: root.switchStep(1)
                }

                SetupActionCard {
                    Layout.fillWidth: true
                    compactCard: true
                    title: presentationBridge.preferredNetwork === "farcaster"
                           ? "Farcaster" : root.hypersnapDisplayName
                    subtitle: {
                        const nodes = nodeSelectionController.nodes
                        for (let index = 0; index < nodes.length; ++index) {
                            if (nodes[index].selected && nodes[index].latencyMs >= 0)
                                return nodes[index].latencyMs + " ms"
                        }
                        return qsTrId("genycaster.setup.network_selected")
                    }
                    iconGlyph: Theme.iconWifi
                    onClicked: root.switchStep(3)
                }

                SetupActionCard {
                    Layout.fillWidth: true
                    compactCard: true
                    title: root.accountName.length > 0 ? root.accountName : root.accountUsername
                    subtitle: root.authenticationRoute === "farcaster"
                              ? qsTrId("genycaster.shell.farcaster_sign_in_openproof_verification")
                              : qsTrId("genycaster.shell.verified_by_openproof")
                    iconGlyph: Theme.iconUser
                    complete: true
                    onClicked: root.switchStep(4)
                }
            }
        }
    }
}
