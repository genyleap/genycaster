pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Eight-stage onboarding. Every stage is a single task on one quiet canvas.
// Account custody, provider approval and recovery stay with PresentationBridge.
Item {
    id: root
    objectName: "initialSetup"
    focus: visible
    property bool reviewMode: false
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

    property int currentStep: 0
    property int highestReachedStep: 0
    property string accountIntent: ""
    property bool showRecovery: false
    property bool showAdvanced: false
    property bool guestSelected: false
    property bool consentChecked: false
    readonly property bool phone: width < 680
    readonly property int setupGutter: phone ? 24 : 48
    readonly property bool browserReadOnly: wasmReadOnly
    readonly property bool authenticationInProgress:
        authenticationPhase === "starting" || authenticationPhase === "awaiting-approval"
        || authenticationPhase === "linking-farcaster"
    readonly property var wizardSteps: [0,1,2,3,4,5,6,7]
    readonly property var greetings: [
        {tag:"en", hello:"Hello", language:"English"},
        {tag:"fa", hello:"سلام", language:"فارسی"},
        {tag:"ar", hello:"مرحباً", language:"العربية"},
        {tag:"es", hello:"Hola", language:"Español"},
        {tag:"fr", hello:"Bonjour", language:"Français"},
        {tag:"de", hello:"Hallo", language:"Deutsch"},
        {tag:"pt", hello:"Olá", language:"Português"},
        {tag:"ru", hello:"Привет", language:"Русский"},
        {tag:"ja", hello:"こんにちは", language:"日本語"},
        {tag:"ko", hello:"안녕하세요", language:"한국어"},
        {tag:"zh", hello:"你好", language:"中文"},
        {tag:"hi", hello:"नमस्ते", language:"हिन्दी"}
    ]

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal openRegistration()
    signal openApproval()
    signal cancelAuthentication()
    signal authorizeSigner()
    signal recoverAccount()
    signal nativeRecoverAccount(string recoveryWords)
    signal acceptLegalConsent(bool termsAccepted, bool privacyAcknowledged, bool reviewCompleted)
    signal finishSetup()
    signal closeReviewRequested()

    Accessible.name: qsTrId("genycaster.setup.title")

    function greetingIndexForTag(tag) {
        const normalized = String(tag || "").toLowerCase().split(/[-_]/)[0]
        for (let i = 0; i < greetings.length; ++i) {
            if (greetings[i].tag === normalized)
                return i
        }
        return 0
    }
    function indexFor(model, id) {
        for (let i = 0; i < model.length; ++i)
            if (String(model[i].id) === String(id)) return i
        return 0
    }
    function stepTitle(index) {
        switch (index) {
        case 0: return qsTrId("genycaster.setup.welcome")
        case 1: return qsTrId("genycaster.locale.language")
        case 2: return qsTrId("genycaster.setup.privacy_terms")
        case 3: return qsTrId("genycaster.header.account")
        case 4: return qsTrId("genycaster.setup.sign_in_required")
        case 5: return qsTrId("genycaster.shell.approve_sign_in")
        case 6: return qsTrId("genycaster.settings.appearance")
        case 7: return qsTrId("genycaster.status.ready")
        }
        return ""
    }
    function switchStep(step) {
        let target = Math.max(0, Math.min(7, step))
        if (!reviewMode && !legalConsentAccepted && !consentChecked && target > 2)
            target = 2
        if (!reviewMode && !authenticated && !guestSelected && target > 5)
            target = 5
        currentStep = target
        highestReachedStep = Math.max(highestReachedStep, target)
        stageScroll.contentY = 0
    }
    function nextStep() {
        if (currentStep === 0) { switchStep(1); return }
        if (currentStep === 1) { switchStep(2); return }
        if (currentStep === 2) {
            if (!legalConsentAccepted && !consentChecked) return
            if (!legalConsentAccepted) acceptLegalConsent(true, true, true)
            // The bridge persists consent asynchronously.
            Qt.callLater(() => switchStep(3))
            return
        }
        if (currentStep === 3) {
            if (accountIntent === "existing") switchStep(4)
            return
        }
        if (currentStep === 4) {
            if (authenticated) switchStep(5)
            return
        }
        if (currentStep === 5) {
            if (authenticated || guestSelected || reviewMode) switchStep(6)
            return
        }
        if (currentStep === 6) { switchStep(7); return }
        if (currentStep === 7) {
            if (reviewMode) closeReviewRequested()
            else finishSetup()
        }
    }
    function previousStep() {
        if (currentStep === 0) {
            if (reviewMode) closeReviewRequested()
            return
        }
        if (currentStep === 6 && guestSelected) { switchStep(4); return }
        if (currentStep === 5 && authenticationInProgress)
            cancelAuthentication()
        switchStep(currentStep - 1)
    }
    function actionText() {
        if (currentStep === 0) return qsTrId("genycaster.setup.design_get_started")
        if (currentStep === 2 && !legalConsentAccepted) return qsTrId("genycaster.setup.agree_continue")
        if (currentStep === 7) return reviewMode ? qsTrId("genycaster.locale.close") : qsTrId("genycaster.setup.design_start")
        return qsTrId("genycaster.setup.continue")
    }
    function actionEnabled() {
        if (currentStep === 2) return legalConsentAccepted || consentChecked
        if (currentStep === 3) return accountIntent === "existing"
        if (currentStep === 4) return authenticated
        if (currentStep === 5) return authenticated || guestSelected || reviewMode
        if (currentStep === 7) return reviewMode || legalConsentAccepted && (authenticated || guestSelected)
        return true
    }
    onVisibleChanged: {
        if (!visible) return
        if (reviewMode) {
            switchStep(0)
        } else if (returningAfterLogout || setupCompleted) {
            switchStep(3)
        } else {
            accountIntent = ""
            guestSelected = false
            consentChecked = false
            switchStep(0)
        }
        forceActiveFocus()
    }
    onAuthenticationPhaseChanged: {
        if (visible && authenticationInProgress && currentStep >= 4 && currentStep <= 5)
            switchStep(5)
    }
    onAuthenticatedChanged: {
        if (visible && authenticated && currentStep >= 3 && currentStep <= 5)
            switchStep(5)
    }
    Keys.onEscapePressed: event => {
        if (reviewMode) closeReviewRequested()
        else previousStep()
        event.accepted = true
    }

    Rectangle { anchors.fill: parent; color: Theme.dark ? Theme.background : Theme.surface }
    // Don't let touches on the empty full-screen backdrop fall through to casts.
    MouseArea {
        anchors.fill: parent
        z: -1
        enabled: root.visible
        acceptedButtons: Qt.AllButtons
        propagateComposedEvents: false
        onPressed: mouse => mouse.accepted = true
        onWheel: wheel => wheel.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            visible: !root.phone
            Layout.fillWidth: true
            Layout.preferredHeight: root.phone ? 0 : 44
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7
                Repeater {
                    model: 4
                    delegate: Superellipse {
                        required property int index
                        width: index === Math.floor(root.currentStep / 2) ? 17 : 6
                        height: 4
                        curveDepth: 2
                        fillColor: index === Math.floor(root.currentStep / 2) ? Theme.accent : Theme.borderStrong
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

            Rectangle {
                visible: !root.phone && root.width >= 1100 && root.currentStep === 0
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.round(parent.width * 0.34)
                color: Theme.dark ? Theme.surfaceMuted : Qt.rgba(Theme.swatchGeny.r, Theme.swatchGeny.g, Theme.swatchGeny.b, 0.10)
                Image {
                    anchors.centerIn: parent
                    width: Math.min(180, parent.width * 0.56)
                    height: width
                    source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                    sourceSize: Qt.size(360, 360)
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.85
                }
            }

            ActionButton {
                z: 20
                visible: root.currentStep > 0
                anchors.top: parent.top
                anchors.topMargin: Theme.space2
                anchors.left: parent.left
                anchors.leftMargin: root.phone ? Theme.space2 : Theme.space6
                compact: true
                quiet: true
                iconOnly: true
                text: qsTrId("genycaster.setup.back")
                iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                onClicked: root.previousStep()
            }
            ActionButton {
                z: 20
                visible: root.reviewMode
                anchors.top: parent.top
                anchors.topMargin: Theme.space2
                anchors.right: parent.right
                anchors.rightMargin: root.phone ? Theme.space2 : Theme.space6
                compact: true
                quiet: true
                iconOnly: true
                text: qsTrId("genycaster.locale.close")
                iconGlyph: Theme.iconClose
                onClicked: root.closeReviewRequested()
            }

            Flickable {
                id: stageScroll
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                contentWidth: width
                contentHeight: Math.max(height, stagePanel.y + stagePanel.implicitHeight + Theme.space6)
                ScrollBar.vertical: GenyScrollBar {
                    policy: stageScroll.contentHeight > stageScroll.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                Item {
                    id: stagePanel
                    width: Math.max(0, Math.min(root.phone ? 440 : 560,
                                             stageScroll.width - root.setupGutter * 2))
                    x: root.currentStep === 0 && !root.phone && root.width >= 1100
                       ? Math.round(stageScroll.width * 0.30
                                    + (stageScroll.width * 0.70 - width) / 2)
                       : Math.round((stageScroll.width - width) / 2)
                    // Optical position per scene from the approved storyboard.
                    y: root.currentStep === 0 || root.currentStep === 7
                       ? Math.max(48, (stageScroll.height - stageLoader.implicitHeight) * 0.40)
                       : root.currentStep === 5
                       ? Math.max(70, (stageScroll.height - stageLoader.implicitHeight) * 0.43)
                       : root.currentStep === 1
                       ? Math.max(62, (stageScroll.height - stageLoader.implicitHeight) * 0.20)
                       : Math.max(44, Math.min(110, stageScroll.height * 0.12))
                    implicitHeight: stageLoader.implicitHeight

                    Loader {
                        id: stageLoader
                        width: parent.width
                        sourceComponent: root.currentStep === 0 ? welcomeStage
                                         : root.currentStep === 1 ? languageStage
                                         : root.currentStep === 2 ? termsStage
                                         : root.currentStep === 3 ? accountStage
                                         : root.currentStep === 4 ? signInStage
                                         : root.currentStep === 5 ? verifyingStage
                                         : root.currentStep === 6 ? appearanceStage : completeStage
                    }
                }
            }
        }

        Rectangle {
            visible: [0,1,2,6,7].indexOf(root.currentStep) >= 0
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? (root.phone ? 82 : 86) : 0
            color: Theme.dark ? Theme.background : Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.phone ? Theme.space5 : Theme.space8
                anchors.rightMargin: root.phone ? Theme.space5 : Theme.space8
                anchors.bottomMargin: root.phone ? Theme.space4 : Theme.space5
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                Item { Layout.fillWidth: !root.phone }

                SetupPrimaryButton {
                    visible: root.currentStep !== 4 || root.authenticated
                    Layout.fillWidth: root.phone
                    Layout.maximumWidth: root.phone ? 16777215 : 420
                    Layout.minimumWidth: root.phone ? 0 : 240
                    text: root.actionText()
                    enabled: root.actionEnabled()
                    neutralPrimary: true
                    showArrow: root.currentStep === 0
                    onClicked: root.nextStep()
                }

                Item { Layout.fillWidth: !root.phone }
            }
        }
    }


    Component {
        id: welcomeStage
        ColumnLayout {
            spacing: Theme.space4
            LayoutMirroring.enabled: Theme.rtl

            Image {
                Layout.preferredWidth: 62
                Layout.preferredHeight: 62
                Layout.alignment: root.phone ? Qt.AlignLeft : Qt.AlignHCenter
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(160,160)
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                text: qsTrId("genycaster.setup.welcome_title")
                font.pixelSize: root.phone ? 32 : 36
                font.weight: Theme.weightBold
                lineHeight: 1.14
                color: Theme.ink
                wrapMode: Text.WordWrap
                horizontalAlignment: root.phone ? Theme.textStartAlignment : Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.design_welcome_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                lineHeight: 1.36
                wrapMode: Text.WordWrap
                horizontalAlignment: root.phone ? Theme.textStartAlignment : Text.AlignHCenter
            }
            Row {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.space6
                spacing: 7
                Repeater {
                    model: 3
                    delegate: Superellipse {
                        required property int index
                        width: 6
                        height: 6
                        curveDepth: 3
                        fillColor: index === 0 ? Theme.ink : Theme.borderStrong
                    }
                }
            }
        }
    }

    Component {
        id: languageStage
        ColumnLayout {
            spacing: Theme.space2
            SetupLanguageSelector {
                id: languageWheel
                Layout.fillWidth: true
                storyboard: true
                model: root.greetings
                selectedLabel: localizationController.languageLabel
                selectedDetail: localizationController.regionalLocaleLabel
                Component.onCompleted: currentIndex =
                    root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                onActivated: (index, entry) => {
                    if (entry && entry.tag && !localizationController.setLanguageTag(entry.tag))
                        languageWheel.currentIndex = root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                }
                Connections {
                    target: localizationController
                    function onEffectiveLanguageTagChanged() {
                        languageWheel.currentIndex = root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                    }
                }
            }
        }
    }

    Component {
        id: termsStage
        ColumnLayout {
            spacing: Theme.space3
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: "system/lock"
                title: qsTrId("genycaster.setup.design_terms_title")
                subtitle: qsTrId("genycaster.setup.design_terms_description")
                centered: !root.phone
            }
            Item { Layout.preferredHeight: Theme.space3 }
            SetupActionCard {
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                title: qsTrId("genycaster.setup.terms_title")
                showChevron: true
                onClicked: Qt.openUrlExternally("https://genyleap.com/terms-of-use")
            }
            SetupActionCard {
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                title: qsTrId("genycaster.setup.privacy_title")
                showChevron: true
                onClicked: Qt.openUrlExternally("https://genyleap.com/privacy")
            }
            CheckBox {
                id: consentCheck
                Layout.fillWidth: true
                Layout.topMargin: Theme.space3
                checked: root.legalConsentAccepted || root.consentChecked
                enabled: !root.legalConsentAccepted
                text: qsTrId("genycaster.setup.single_legal_consent")
                onToggled: root.consentChecked = checked
                focusPolicy: Qt.StrongFocus
                spacing: Theme.space2
                LayoutMirroring.enabled: Theme.rtl
                implicitHeight: Math.max(Theme.touchTarget, consentLabel.implicitHeight + Theme.space2 * 2)
                indicator: Superellipse {
                    implicitWidth: 21
                    implicitHeight: 21
                    x: consentCheck.leftPadding
                    y: (consentCheck.height - height) / 2
                    fillColor: consentCheck.checked ? Theme.ink : Theme.surface
                    borderColor: consentCheck.checked ? Theme.ink : Theme.borderStrong
                    borderWidth: 1
                    curveDepth: 5
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconCheck
                        visible: consentCheck.checked
                        color: Theme.surface
                        preferredSize: 14
                    }
                }
                contentItem: DirectionAwareLabel {
                    id: consentLabel
                    text: consentCheck.text
                    color: Theme.ink
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: consentCheck.indicator.width + consentCheck.spacing
                }
                background: Superellipse {
                    fillColor: "transparent"
                    borderWidth: Theme.keyboardFocusVisible(consentCheck) ? 2 : 0
                    borderColor: Theme.focus
                }
            }
        }
    }

    Component {
        id: accountStage
        ColumnLayout {
            spacing: Theme.space3
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconUser
                title: qsTrId("genycaster.setup.design_account_title")
                subtitle: qsTrId("genycaster.setup.design_account_description")
                centered: !root.phone
            }
            Item { Layout.preferredHeight: Theme.space4 }
            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 2
                rowSpacing: Theme.space3
                columnSpacing: Theme.space3
                layoutDirection: Theme.uiDirection
                SetupActionCard {
                    Layout.fillWidth: true
                    onboardingStyle: true
                    primary: true
                    neutralPrimary: true
                    title: qsTrId("genycaster.setup.design_create_account")
                    subtitle: qsTrId("genycaster.setup.new_profile_body")
                    iconGlyph: root.phone ? "" : Theme.iconUser
                    showChevron: true
                    onClicked: {
                        root.accountIntent = "new"
                        root.openRegistration()
                    }
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    onboardingStyle: true
                    primary: false
                    title: qsTrId("genycaster.setup.design_existing_account")
                    subtitle: qsTrId("genycaster.setup.design_existing_description")
                    iconGlyph: root.phone ? "" : Theme.iconLink
                    showChevron: true
                    onClicked: {
                        root.accountIntent = "existing"
                        root.switchStep(4)
                    }
                }
            }
        }
    }

    Component {
        id: signInStage
        ColumnLayout {
            spacing: Theme.space3
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconLink
                title: qsTrId("genycaster.setup.design_signin_title")
                subtitle: qsTrId("genycaster.setup.design_signin_description")
                centered: !root.phone
            }
            Item { Layout.preferredHeight: Theme.space4 }
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                iconGlyph: "brands/geny"
                title: qsTrId("genycaster.shell.continue_with_genyleap")
                enabled: root.runtimeReady && root.authenticationAvailable && !root.authenticationInProgress
                onClicked: {
                    root.continueWithGenyleap()
                    root.switchStep(5)
                }
            }
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                iconGlyph: "brands/farcaster"
                title: qsTrId("genycaster.shell.sign_in_with_farcaster")
                enabled: root.runtimeReady && root.authenticationAvailable && !root.authenticationInProgress
                onClicked: {
                    root.continueWithFarcaster()
                    root.switchStep(5)
                }
            }
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                iconGlyph: "system/lock"
                title: qsTrId("genycaster.setup.design_recovery_title")
                onClicked: {
                    if (root.browserReadOnly) {
                        root.recoverAccount()
                        root.switchStep(5)
                    } else {
                        root.showRecovery = !root.showRecovery
                    }
                }
            }
            ColumnLayout {
                visible: root.showRecovery && !root.browserReadOnly && !root.authenticated
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.recovery_privacy")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                TextField {
                    id: recoveryField
                    Layout.fillWidth: true
                    maximumLength: 4095
                    echoMode: revealWords.checked ? TextInput.Normal : TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    placeholderText: qsTrId("genycaster.setup.native_recovery_placeholder")
                    enabled: !root.authenticationInProgress
                    onVisibleChanged: if (!visible) clear()
                    Accessible.name: qsTrId("genycaster.setup.native_recovery_placeholder")
                }
                CheckBox {
                    id: revealWords
                    text: qsTrId("genycaster.setup.show_recovery_words")
                    onVisibleChanged: if (!visible) checked = false
                }
                SetupPrimaryButton {
                    Layout.fillWidth: true
                    neutralPrimary: true
                    text: qsTrId("genycaster.setup.recovery_button")
                    enabled: root.runtimeReady && !root.authenticationInProgress
                          && (recoveryField.text.trim().split(/\s+/).length === 12
                              || recoveryField.text.trim().split(/\s+/).length === 24)
                    onClicked: {
                        let words = recoveryField.text
                        recoveryField.clear()
                        revealWords.checked = false
                        root.nativeRecoverAccount(words)
                        root.switchStep(5)
                        words = ""
                    }
                }
            }
            SetupActionCard {
                visible: root.authenticated
                Layout.fillWidth: true
                compactCard: true
                title: qsTrId("genycaster.setup.account_verified")
                iconGlyph: Theme.iconCheck
                complete: true
                onClicked: root.switchStep(5)
            }
            ActionButton {
                visible: root.browserReadOnly && !root.authenticated
                Layout.alignment: Qt.AlignHCenter
                quiet: true
                compact: true
                text: qsTrId("genycaster.setup.continue_as_guest")
                onClicked: {
                    root.guestSelected = true
                    root.switchStep(6)
                }
            }
            DirectionAwareLabel {
                visible: root.authenticationErrorText.length > 0
                Layout.fillWidth: true
                text: root.authenticationErrorText
                color: Theme.danger
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }

    Component {
        id: verifyingStage
        ColumnLayout {
            spacing: Theme.space4
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 78
                GenySpinner {
                    anchors.centerIn: parent
                    width: 50
                    height: 50
                    strokeWidth: 3
                    running: root.authenticationInProgress
                    visible: !root.authenticated
                    color: Theme.accent
                }
                Icon {
                    anchors.centerIn: parent
                    visible: root.authenticated
                    text: Theme.iconCheck
                    color: Theme.accent
                    preferredSize: 44
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticated ? qsTrId("genycaster.setup.account_verified")
                    : root.authenticationPhase === "linking-farcaster"
                    ? qsTrId("genycaster.shell.link_your_farcaster_account")
                    : root.authenticationPhase === "awaiting-approval"
                    ? qsTrId("genycaster.shell.approve_sign_in")
                    : qsTrId("genycaster.setup.design_verifying_title")
                color: Theme.ink
                font.pixelSize: root.phone ? 25 : 30
                font.weight: Theme.weightBold
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticated ? qsTrId("genycaster.setup.account_verified_body")
                    : qsTrId("genycaster.setup.design_verifying_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            SetupPrimaryButton {
                visible: !root.authenticated && root.authenticationApprovalAvailable
                Layout.fillWidth: true
                neutralPrimary: true
                text: root.authenticationRoute === "farcaster"
                    ? qsTrId("genycaster.shell.open_farcaster")
                    : qsTrId("genycaster.shell.open_genyleap")
                onClicked: root.openApproval()
            }
            SetupActionCard {
                visible: root.authenticated && !root.signerReady
                Layout.fillWidth: true
                compactCard: true
                title: qsTrId("genycaster.shell.enable_interactions")
                busy: root.signerBusy
                enabled: !root.signerBusy && root.runtimeReady
                onClicked: root.authorizeSigner()
            }
            ActionButton {
                visible: root.authenticationInProgress
                Layout.alignment: Qt.AlignHCenter
                quiet: true
                compact: true
                text: qsTrId("genycaster.shell.cancel")
                onClicked: {
                    root.cancelAuthentication()
                    root.switchStep(4)
                }
            }
            DirectionAwareLabel {
                visible: root.authenticationErrorText.length > 0 || root.signerErrorText.length > 0
                Layout.fillWidth: true
                text: root.authenticationErrorText.length > 0
                    ? root.authenticationErrorText : root.signerErrorText
                color: Theme.danger
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            SetupActionCard {
                visible: !root.authenticated && !root.authenticationInProgress
                Layout.fillWidth: true
                compactCard: true
                title: qsTrId("genycaster.setup.other_sign_in_methods")
                onClicked: root.switchStep(4)
            }
        }
    }

    Component {
        id: appearanceStage
        ColumnLayout {
            spacing: Theme.space3
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconMoon
                title: qsTrId("genycaster.setup.design_appearance_title")
                subtitle: qsTrId("genycaster.setup.design_appearance_description")
                centered: !root.phone
            }
            Item { Layout.preferredHeight: Theme.space4 }
            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 3
                columnSpacing: Theme.space2
                rowSpacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Repeater {
                    model: [
                        { key: "light", label: qsTrId("genycaster.settings.mode_light"), glyph: Theme.iconSun },
                        { key: "dark", label: qsTrId("genycaster.settings.mode_dark"), glyph: Theme.iconMoon },
                        { key: "system", label: qsTrId("genycaster.settings.mode_system"), glyph: Theme.iconSystem }
                    ]
                    delegate: Button {
                        id: appearanceChoice
                        required property var modelData
                        readonly property bool chosen: appearanceController.themeMode === modelData.key
                        Layout.fillWidth: true
                        Layout.minimumWidth: root.phone ? 0 : 140
                        Layout.preferredWidth: root.phone ? -1 : 165
                        implicitHeight: root.phone ? 62 : 110
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData.label
                        Accessible.checked: chosen
                        onClicked: appearanceController.setThemeMode(modelData.key)
                        background: Superellipse {
                            fillColor: appearanceChoice.chosen ? Theme.surfaceMuted : Theme.surface
                            borderWidth: appearanceChoice.chosen ? 2 : 1
                            borderColor: appearanceChoice.chosen ? Theme.accent : Theme.borderMuted
                            curveDepth: Theme.curveContainer
                        }
                        contentItem: Item {
                            RowLayout {
                                visible: root.phone
                                anchors.fill: parent
                                anchors.leftMargin: Theme.space3
                                anchors.rightMargin: Theme.space3
                                spacing: Theme.space3
                                Icon {
                                    text: appearanceChoice.modelData.glyph
                                    color: appearanceChoice.chosen ? Theme.ink : Theme.inkMuted
                                    preferredSize: 22
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: appearanceChoice.modelData.label
                                    color: Theme.ink
                                    font.weight: Theme.weightDemiBold
                                    horizontalAlignment: Theme.textStartAlignment
                                }
                                Icon {
                                    visible: appearanceChoice.chosen
                                    text: Theme.iconCheck
                                    color: Theme.accent
                                    preferredSize: 17
                                }
                            }
                            ColumnLayout {
                                visible: !root.phone
                                anchors.centerIn: parent
                                spacing: Theme.space2
                                Icon {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: appearanceChoice.modelData.glyph
                                    color: Theme.ink
                                    preferredSize: 28
                                }
                                DirectionAwareLabel {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: appearanceChoice.modelData.label
                                    color: Theme.ink
                                    font.weight: Theme.weightDemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            Icon {
                                visible: !root.phone && appearanceChoice.chosen
                                anchors.top: parent.top
                                anchors.right: parent.right
                                text: Theme.iconCheck
                                color: Theme.accent
                                preferredSize: 14
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: completeStage
        ColumnLayout {
            spacing: Theme.space5
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                Superellipse {
                    width: 66
                    height: 66
                    anchors.centerIn: parent
                    curveDepth: 20
                    fillColor: Theme.accent
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconCheck
                        color: Theme.accentInk
                        preferredSize: 36
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.design_complete_title")
                color: Theme.ink
                font.pixelSize: root.phone ? 30 : 34
                font.weight: Theme.weightBold
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.design_complete_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                lineHeight: 1.35
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
