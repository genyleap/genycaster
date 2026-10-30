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
        if (currentStep === 0) return qsTrId("genycaster.setup.continue")
        if (currentStep === 2 && !legalConsentAccepted) return qsTrId("genycaster.setup.agree_continue")
        if (currentStep === 7) return reviewMode ? qsTrId("genycaster.locale.close") : qsTrId("genycaster.setup.finish")
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

    Rectangle { anchors.fill: parent; color: Theme.background }
    // Don't let touches on the empty full-screen backdrop fall through to casts.
    MouseArea { anchors.fill: parent; z: -1; onPressed: mouse => mouse.accepted = true }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.phone ? 64 : 76

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.phone ? Theme.space4 : Theme.space7
                anchors.rightMargin: root.phone ? Theme.space4 : Theme.space7
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                ActionButton {
                    visible: root.currentStep > 0 || root.reviewMode
                    iconOnly: true
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.setup.back")
                    iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                    onClicked: root.previousStep()
                }

                Image {
                    source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    fillMode: Image.PreserveAspectFit
                    sourceSize: Qt.size(90,90)
                }
                DirectionAwareLabel {
                    text: "GenyCaster"
                    font.pixelSize: 18
                    font.weight: Theme.weightBold
                    color: Theme.ink
                }
                Item { Layout.fillWidth: true }

                Row {
                    spacing: 5
                    layoutDirection: Qt.LeftToRight
                    Repeater {
                        model: root.wizardSteps
                        delegate: Superellipse {
                            required property int modelData
                            width: modelData === root.currentStep ? 20 : 6
                            height: 6
                            curveDepth: 3
                            fillColor: modelData === root.currentStep
                                       ? Theme.accent : modelData < root.currentStep
                                       ? Theme.inkMuted : Theme.borderStrong
                        }
                    }
                }
                ActionButton {
                    visible: root.reviewMode
                    iconOnly: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.locale.close")
                    onClicked: root.closeReviewRequested()
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
                width: Math.round(parent.width * 0.30)
                color: Theme.surfaceMuted
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
                    width: Math.min(560, stageScroll.width - (root.phone ? 40 : 96))
                    x: root.currentStep === 0 && !root.phone && root.width >= 1100
                       ? Math.round(stageScroll.width * 0.30
                                    + (stageScroll.width * 0.70 - width) / 2)
                       : Math.round((stageScroll.width - width) / 2)
                    y: Math.max(root.phone ? Theme.space5 : Theme.space7,
                                (stageScroll.height - stageLoader.implicitHeight) * (root.phone ? 0.11 : 0.22))
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

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.phone ? 90 : 100

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
                    Layout.maximumWidth: root.phone ? 16777215 : 440
                    Layout.minimumWidth: root.phone ? 0 : 240
                    text: root.actionText()
                    enabled: root.actionEnabled()
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
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? 110 : 140
                Image {
                    anchors.centerIn: parent
                    width: 76
                    height: 76
                    fillMode: Image.PreserveAspectFit
                    sourceSize: Qt.size(228,228)
                    source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.welcome_title")
                color: Theme.ink
                font.pixelSize: root.phone ? 36 : 42
                font.weight: Theme.weightBold
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.world_conversations")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeBodySize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Component {
        id: languageStage
        ColumnLayout {
            spacing: Theme.space4
            SetupLanguageSelector {
                id: wheel
                Layout.fillWidth: true
                model: root.greetings
                selectedLabel: localizationController.languageLabel
                selectedDetail: localizationController.regionalLocaleLabel
                Component.onCompleted: currentIndex =
                    root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                onActivated: (index, entry) => {
                    if (entry && entry.tag && !localizationController.setLanguageTag(entry.tag))
                        wheel.currentIndex = root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                }
                Connections {
                    target: localizationController
                    function onEffectiveLanguageTagChanged() {
                        wheel.currentIndex = root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                    }
                }
            }
        }
    }

    Component {
        id: termsStage
        ColumnLayout {
            spacing: Theme.space4
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: "system/lock"
                title: qsTrId("genycaster.setup.privacy_terms")
                subtitle: qsTrId("genycaster.setup.privacy_compact_intro")
            }
            Item { Layout.preferredHeight: Theme.space3 }
            SetupActionCard {
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.terms_title")
                subtitle: qsTrId("genycaster.setup.open_terms")
                iconGlyph: "system/lock"
                onClicked: Qt.openUrlExternally("https://genyleap.com/terms-of-use")
            }
            SetupActionCard {
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.privacy_title")
                subtitle: qsTrId("genycaster.setup.open_privacy")
                iconGlyph: "system/lock"
                onClicked: Qt.openUrlExternally("https://genyleap.com/privacy")
            }
            CheckBox {
                id: consent
                Layout.fillWidth: true
                Layout.topMargin: Theme.space5
                text: qsTrId("genycaster.setup.single_legal_consent")
                checked: root.legalConsentAccepted || root.consentChecked
                enabled: !root.legalConsentAccepted
                spacing: Theme.space3
                LayoutMirroring.enabled: Theme.rtl
                implicitHeight: Math.max(Theme.touchTarget, legalText.implicitHeight + Theme.space3 * 2)
                onToggled: root.consentChecked = checked
                indicator: Superellipse {
                    implicitWidth: 24; implicitHeight: 24
                    x: consent.leftPadding
                    y: Math.round((consent.height - height) / 2)
                    fillColor: consent.checked ? Theme.primaryFill : Theme.surface
                    borderColor: consent.checked ? Theme.primaryFill : Theme.borderStrong
                    borderWidth: 1
                    curveDepth: 6
                    Icon {
                        anchors.centerIn: parent
                        visible: consent.checked
                        text: Theme.iconCheck
                        color: Theme.accentInk
                        preferredSize: 15
                    }
                }
                contentItem: DirectionAwareLabel {
                    id: legalText
                    text: consent.text
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: consent.indicator.width + consent.spacing
                }
                background: Superellipse {
                    fillColor: Theme.surfaceMuted
                    borderColor: Theme.keyboardFocusVisible(consent) ? Theme.focus : Theme.borderMuted
                    borderWidth: Theme.keyboardFocusVisible(consent) ? 2 : 1
                    curveDepth: Theme.curveContainer
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
                title: qsTrId("genycaster.header.account")
                subtitle: qsTrId("genycaster.setup.account_methods_explainer")
            }
            Item { Layout.preferredHeight: Theme.space4 }
            SetupActionCard {
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.new_profile")
                subtitle: qsTrId("genycaster.setup.new_profile_body")
                primary: root.accountIntent !== "existing"
                showSelectionMark: false
                trailingText: qsTrId("genycaster.shell.coming_soon")
                onClicked: root.accountIntent = "new"
            }
            SetupActionCard {
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.existing_profile")
                subtitle: qsTrId("genycaster.setup.existing_profile_body")
                primary: root.accountIntent === "existing"
                showSelectionMark: false
                onClicked: {
                    root.accountIntent = "existing"
                    root.switchStep(4)
                }
            }
            ColumnLayout {
                visible: root.accountIntent === "new"
                Layout.fillWidth: true
                spacing: Theme.space3
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.registration_handoff")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeLabelSize
                    wrapMode: Text.WordWrap
                }
                SetupPrimaryButton {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.open_registration")
                    onClicked: root.openRegistration()
                }
            }
        }
    }

    Component {
        id: signInStage
        ColumnLayout {
            spacing: Theme.space4
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconLink
                title: qsTrId("genycaster.setup.sign_in_required")
                subtitle: qsTrId("genycaster.setup.authentication_explainer")
            }
            Item { Layout.preferredHeight: Theme.space4 }
            SetupActionCard {
                visible: root.authenticated
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.account_verified")
                subtitle: qsTrId("genycaster.setup.account_verified_body")
                iconGlyph: Theme.iconCheck
                complete: true
                onClicked: root.switchStep(5)
            }
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                title: qsTrId("genycaster.shell.continue_with_genyleap")
                subtitle: qsTrId("genycaster.setup.account_genyleap_body")
                iconGlyph: "brands/geny"
                enabled: root.runtimeReady && root.authenticationAvailable && !root.authenticationInProgress
                onClicked: {
                    root.switchStep(5)
                    root.continueWithGenyleap()
                }
            }
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                title: qsTrId("genycaster.shell.sign_in_with_farcaster")
                subtitle: qsTrId("genycaster.setup.account_farcaster_body")
                iconGlyph: "brands/farcaster"
                enabled: root.runtimeReady && root.authenticationAvailable && !root.authenticationInProgress
                onClicked: {
                    root.switchStep(5)
                    root.continueWithFarcaster()
                }
            }
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                title: qsTrId("genycaster.setup.recovery_button")
                subtitle: qsTrId("genycaster.setup.recovery_button_body")
                iconGlyph: "system/lock"
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
                spacing: Theme.space3
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.recovery_privacy")
                    color: Theme.inkMuted
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareTextField {
                    id: phraseField
                    Layout.fillWidth: true
                    maximumLength: 4095
                    echoMode: revealPhrase.checked ? TextInput.Normal : TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    placeholderText: qsTrId("genycaster.setup.native_recovery_placeholder")
                    enabled: !root.authenticationInProgress
                    onVisibleChanged: if (!visible) clear()
                    Accessible.name: qsTrId("genycaster.setup.native_recovery_placeholder")
                }
                CheckBox {
                    id: revealPhrase
                    text: qsTrId("genycaster.setup.show_recovery_words")
                    onVisibleChanged: if (!visible) checked = false
                }
                SetupPrimaryButton {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.recovery_button")
                    enabled: root.runtimeReady && !root.authenticationInProgress
                             && (phraseField.text.trim().split(/\s+/).length === 12
                                 || phraseField.text.trim().split(/\s+/).length === 24)
                    onClicked: {
                        let words = phraseField.text
                        phraseField.clear()
                        revealPhrase.checked = false
                        root.switchStep(5)
                        if (root.browserReadOnly) root.recoverAccount()
                        else root.nativeRecoverAccount(words)
                        words = ""
                    }
                }
            }
            SetupPrimaryButton {
                visible: root.browserReadOnly && !root.authenticated
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.continue_as_guest")
                onClicked: {
                    root.guestSelected = true
                    root.switchStep(6)
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticationErrorText
                visible: text.length > 0
                color: Theme.danger
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }

    Component {
        id: verifyingStage
        ColumnLayout {
            spacing: Theme.space5
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? 110 : 150
                GenySpinner {
                    anchors.centerIn: parent
                    width: 56; height: 56
                    strokeWidth: 4
                    running: root.authenticationInProgress
                    color: root.authenticated ? Theme.success : Theme.accent
                }
                Icon {
                    anchors.centerIn: parent
                    visible: root.authenticated
                    text: Theme.iconCheck
                    color: Theme.success
                    preferredSize: 48
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticated ? qsTrId("genycaster.setup.account_verified")
                      : root.authenticationPhase === "linking-farcaster"
                      ? qsTrId("genycaster.shell.link_your_farcaster_account")
                      : root.authenticationPhase === "awaiting-approval"
                      ? qsTrId("genycaster.shell.approve_sign_in")
                      : root.authenticationPhase === "starting"
                      ? qsTrId("genycaster.setup.recovery_working")
                      : qsTrId("genycaster.setup.sign_in_required")
                font.pixelSize: root.phone ? 28 : 34
                font.weight: Theme.weightBold
                color: Theme.ink
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticated
                      ? qsTrId("genycaster.setup.account_verified_body")
                      : root.authenticationPhase === "awaiting-approval"
                      ? qsTrId("genycaster.shell.approve_the_request_in_a_farcaster_compatible_client_t")
                      : qsTrId("genycaster.setup.recovery_working_body")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            SetupActionCard {
                visible: root.authenticated && !root.signerReady
                Layout.fillWidth: true
                title: qsTrId("genycaster.shell.enable_interactions")
                subtitle: root.signerStatusText
                iconGlyph: Theme.iconCheck
                busy: root.signerBusy
                enabled: !root.signerBusy && root.runtimeReady
                onClicked: root.authorizeSigner()
            }
            DirectionAwareLabel {
                visible: root.signerErrorText.length > 0
                Layout.fillWidth: true
                text: root.signerErrorText
                color: Theme.warning
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
            SetupPrimaryButton {
                visible: !root.authenticated && root.authenticationApprovalAvailable
                Layout.fillWidth: true
                text: root.authenticationRoute === "farcaster"
                      ? qsTrId("genycaster.shell.open_farcaster")
                      : qsTrId("genycaster.shell.open_genyleap")
                onClicked: root.openApproval()
            }
            ActionButton {
                visible: root.authenticationInProgress
                Layout.alignment: Qt.AlignHCenter
                quiet: true
                text: qsTrId("genycaster.shell.cancel")
                onClicked: { root.cancelAuthentication(); root.switchStep(4) }
            }
            DirectionAwareLabel {
                visible: root.authenticationErrorText.length > 0
                Layout.fillWidth: true
                text: root.authenticationErrorText
                color: Theme.danger
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            SetupActionCard {
                visible: !root.authenticated && !root.authenticationInProgress
                Layout.fillWidth: true
                title: qsTrId("genycaster.shell.try_again")
                onClicked: root.switchStep(4)
            }
        }
    }

    Component {
        id: appearanceStage
        ColumnLayout {
            spacing: Theme.space4
            SetupPageHeader {
                Layout.fillWidth: true
                iconGlyph: Theme.iconMoon
                title: qsTrId("genycaster.settings.appearance")
                subtitle: qsTrId("genycaster.setup.personalize_body")
            }
            Item { Layout.preferredHeight: Theme.space3 }
            Repeater {
                model: [
                    { key:"light", title:qsTrId("genycaster.settings.mode_light"), glyph:Theme.iconSun },
                    { key:"dark", title:qsTrId("genycaster.settings.mode_dark"), glyph:Theme.iconMoon },
                    { key:"system", title:qsTrId("genycaster.settings.mode_system"), glyph:Theme.iconSystem }
                ]
                delegate: SetupActionCard {
                    required property var modelData
                    Layout.fillWidth: true
                    title: modelData.title
                    iconGlyph: modelData.glyph
                    selected: appearanceController.themeMode === modelData.key
                    primary: false
                    showChevron: false
                    onClicked: appearanceController.setThemeMode(modelData.key)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space3
                spacing: Theme.space2
                Repeater {
                    model: [
                        { key:"geny", shade:Theme.swatchGeny },
                        { key:"ocean", shade:Theme.swatchOcean },
                        { key:"violet", shade:Theme.swatchViolet },
                        { key:"ember", shade:Theme.swatchEmber },
                        { key:"rose", shade:Theme.swatchRose },
                        { key:"mint", shade:Theme.swatchMint },
                        { key:"mono", shade:Theme.swatchMono }
                    ]
                    delegate: Button {
                        id: chip
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 44
                        Accessible.name: modelData.key
                        Accessible.checked: appearanceController.colorThemeId === modelData.key
                        onClicked: appearanceController.setColorThemeId(modelData.key)
                        background: Superellipse {
                            fillColor: "transparent"
                            borderWidth: chip.activeFocus ? 2 : 0
                            borderColor: Theme.focus
                            curveDepth: 10
                        }
                        contentItem: Superellipse {
                            width: 28; height: 28
                            curveDepth: 14
                            fillColor: chip.modelData.shade
                            borderWidth: appearanceController.colorThemeId === chip.modelData.key ? 3 : 1
                            borderColor: appearanceController.colorThemeId === chip.modelData.key ? Theme.ink : Theme.borderMuted
                        }
                    }
                }
            }
            ActionButton {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space3
                quiet: true
                text: qsTrId("genycaster.setup.advanced_nodes")
                iconGlyph: Theme.iconSettings
                onClicked: root.showAdvanced = !root.showAdvanced
            }
            ColumnLayout {
                visible: root.showAdvanced
                Layout.fillWidth: true
                spacing: Theme.space3
                DirectionAwareLabel {
                    text: qsTrId("genycaster.setup.connection_mode_title")
                    color: Theme.inkMuted
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    title: qsTrId("genycaster.setup.network_hypersnap_category").arg(qsTrId("genycaster.setup.decentralized_label"))
                    iconGlyph: Theme.iconGlobe
                    selected: presentationBridge.preferredNetwork === presentationBridge.independentNetworkKey
                    showChevron: false
                    onClicked: presentationBridge.setPreferredNetwork(presentationBridge.independentNetworkKey)
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    title: qsTrId("genycaster.setup.network_farcaster_category").arg(qsTrId("genycaster.setup.centralized_service_label"))
                    iconGlyph: "brands/farcaster"
                    selected: presentationBridge.preferredNetwork === presentationBridge.hostedNetworkKey
                    showChevron: false
                    onClicked: presentationBridge.setPreferredNetwork(presentationBridge.hostedNetworkKey)
                }
                GenyComboBox {
                    Layout.fillWidth: true
                    model: localizationController.availableRegionalLocales
                    textRole: "label"; valueRole: "id"
                    currentIndex: root.indexFor(localizationController.availableRegionalLocales, localizationController.regionalLocaleTag)
                    onActivated: (index) => localizationController.setRegionalLocaleTag(localizationController.availableRegionalLocales[index].id)
                }
                GenyComboBox {
                    Layout.fillWidth: true
                    model: localizationController.availableTimeZones
                    textRole: "label"; valueRole: "id"
                    currentIndex: root.indexFor(localizationController.availableTimeZones, localizationController.timeZoneId)
                    onActivated: (index) => localizationController.setTimeZoneId(localizationController.availableTimeZones[index].id)
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
                Layout.preferredHeight: root.phone ? 130 : 168
                Superellipse {
                    anchors.centerIn: parent
                    width: 74; height: 74
                    fillColor: Theme.primaryFill
                    curveDepth: 24
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
                text: qsTrId("genycaster.status.ready")
                color: Theme.ink
                font.pixelSize: root.phone ? 32 : 40
                font.weight: Theme.weightBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.ready_generic")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            SetupActionCard {
                Layout.fillWidth: true
                compactCard: true
                title: localizationController.languageLabel
                iconGlyph: Theme.iconLanguage
                onClicked: root.switchStep(1)
            }
            SetupActionCard {
                Layout.fillWidth: true
                compactCard: true
                title: qsTrId("genycaster.settings.appearance")
                subtitle: appearanceController.themeMode
                iconGlyph: Theme.iconPalette
                onClicked: root.switchStep(6)
            }
        }
    }
}
