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
    property bool stageTransitioning: false
    property real stageOpacity: 1.0
    property real stageOffset: 0.0
    property int pendingStep: -1
    readonly property bool showsPrimaryCta: [0,1,2,6,7].indexOf(currentStep) >= 0
    readonly property int progressCount: wizardSteps.length
    Timer {
        id: stageTransitionTimer
        interval: 135
        repeat: false
        onTriggered: {
            if (root.pendingStep < 0) return
            root.currentStep = root.pendingStep
            root.highestReachedStep = Math.max(root.highestReachedStep, root.currentStep)
            stageScroll.contentY = 0
            root.pendingStep = -1
            root.stageOffset = 0
            root.stageOpacity = 1
            root.stageTransitioning = false
        }
    }
    function progressWidth(index) { return currentStep === index ? 18 : 5 }
    property string accountIntent: ""
    property bool showRecovery: false
    property string legalDocument: ""
    property bool recoverySubmitPending: false
    readonly property bool verifyingBusy: authenticationInProgress && !authenticated
    function openLegalDocument(kind) {
        if (kind !== "terms" && kind !== "privacy")
            return
        legalDocument = kind
        legalReader.open()
    }
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
        if (target === currentStep && !stageTransitioning)
            return
        if (!visible || Theme.reduceMotion) {
            stageTransitionTimer.stop()
            pendingStep = -1
            stageTransitioning = false
            stageOpacity = 1
            stageOffset = 0
            currentStep = target
            highestReachedStep = Math.max(highestReachedStep, target)
            stageScroll.contentY = 0
            return
        }
        // Defer component replacement until its predecessor has faded out.
        // This also prevents rapid taps from destroying a form mid-transition.
        stageTransitioning = true
        pendingStep = target
        stageOpacity = 0
        stageOffset = target > currentStep ? -12 : 12
        stageTransitionTimer.restart()
    }
    function nextStep() {
        if (currentStep === 0) { switchStep(1); return }
        if (currentStep === 1) { switchStep(2); return }
        if (currentStep === 2) {
            if (!consentChecked) return
            if (!legalConsentAccepted) acceptLegalConsent(true, true, true)
            // Legal acknowledgement is persisted by the bridge before leaving.
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
        if (currentStep === 5 && (authenticationInProgress || recoverySubmitPending)) {
            cancelAuthentication()
            recoverySubmitPending = false
        }
        switchStep(currentStep - 1)
    }
    function actionText() {
        if (currentStep === 0) return qsTrId("genycaster.setup.design_get_started")
        if (currentStep === 2 && !legalConsentAccepted) return qsTrId("genycaster.setup.agree_continue")
        if (currentStep === 7) return reviewMode ? qsTrId("genycaster.locale.close") : qsTrId("genycaster.setup.design_start")
        return qsTrId("genycaster.setup.continue")
    }
    function actionEnabled() {
        if (currentStep === 2) return consentChecked
        if (currentStep === 3) return accountIntent === "existing"
        if (currentStep === 4) return authenticated
        if (currentStep === 5) return authenticated || guestSelected || reviewMode
        if (currentStep === 7) return reviewMode || legalConsentAccepted && (authenticated || guestSelected)
        return true
    }
    onVisibleChanged: {
        if (!visible) {
            stageTransitionTimer.stop()
            stageTransitioning = false
            pendingStep = -1
            stageOpacity = 1
            return
        }
        if (reviewMode) {
            switchStep(0)
        } else {
            // Sign-out always restores Welcome immediately, never a stale
            // choose-account screen or a deferred transition.
            stageTransitionTimer.stop()
            pendingStep = -1
            stageTransitioning = false
            stageOpacity = 1
            stageOffset = 0
            currentStep = 0
            stageScroll.contentY = 0
            highestReachedStep = 0
            accountIntent = ""
            showRecovery = false
            guestSelected = false
            consentChecked = legalConsentAccepted
        }
        forceActiveFocus()
    }
    onAuthenticationPhaseChanged: {
        if (authenticationPhase === "failed" || authenticationPhase === "signed-out")
            recoverySubmitPending = false
        if (!visible || currentStep < 4 || currentStep > 5)
            return
        // The browser's recovery-word dialog lives in the current app page.
        // Do not cover it with Verification while the words are being entered.
        if (authenticationRoute === "recovery" && browserReadOnly && !authenticated)
            return
        if (authenticationInProgress)
            switchStep(5)
        else if (!authenticated && currentStep === 5
                 && authenticationPhase === "failed")
            switchStep(4)
    }
    onAuthenticationErrorTextChanged: {
        if (authenticationErrorText.length && authenticationRoute === "recovery" && !authenticated) {
            recoverySubmitPending = false
            if (currentStep === 5) switchStep(4)
        }
    }
    onAuthenticatedChanged: {
        if (visible && authenticated && currentStep >= 3 && currentStep <= 5) {
            recoverySubmitPending = false
            switchStep(5)
        }
    }
    Keys.onEscapePressed: event => {
        if (reviewMode) closeReviewRequested()
        else previousStep()
        event.accepted = true
    }

    Rectangle { anchors.fill: parent; color: Theme.dark ? Theme.background : Theme.surface }

    // The legal reader is local to Setup rather than Overlay.overlay, which
    // may belong to a different window/sheet in Qt WASM. Both documents use
    // one reliably clickable surface; nothing opens in an external tab.
    Item {
        id: legalReader
        anchors.fill: parent
        z: 20000
        property bool opened: false
        visible: opened
        function open() {
            opened = true
            legalBodyScroll.contentY = 0
        }
        function close() { opened = false }
        Keys.onEscapePressed: event => {
            legalReader.close()
            event.accepted = true
        }
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.42)
        }
        MouseArea {
            anchors.fill: parent
            onClicked: legalReader.close()
        }
        Superellipse {
            id: legalSurface
            z: 1
            x: root.phone ? 0 : (legalReader.width - width) / 2
            y: root.phone ? legalReader.height - height : (legalReader.height - height) / 2
            width: root.phone ? legalReader.width : Math.min(650, legalReader.width - 48)
            height: root.phone ? Math.min(legalReader.height * 0.87, 730)
                               : Math.min(legalReader.height - 84, 660)
            fillColor: Theme.surface
            curveDepth: root.phone ? 28 : Theme.curveContainer
            borderWidth: Theme.strokeStandard
            borderColor: Theme.borderMuted
            property real offset: legalReader.opened ? 0 : 20
            transform: Translate { y: legalSurface.offset }
            Behavior on offset {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                    easing.type: Easing.OutCubic
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: mouse => mouse.accepted = true
            }
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.phone ? Theme.space5 : Theme.space7
                spacing: Theme.space4
                Item {
                    visible: root.phone
                    Layout.fillWidth: true
                    Layout.preferredHeight: 12
                    Superellipse {
                        anchors.centerIn: parent
                        width: 42
                        height: 5
                        curveDepth: 3
                        fillColor: Theme.borderStrong
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.legalDocument === "terms"
                              ? qsTrId("genycaster.setup.terms_title")
                              : qsTrId("genycaster.setup.privacy_title")
                        color: Theme.ink
                        font.pixelSize: root.phone ? 23 : 27
                        font.weight: Theme.weightBold
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconClose
                        text: qsTrId("genycaster.locale.close")
                        onClicked: legalReader.close()
                    }
                }
                Flickable {
                    id: legalBodyScroll
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true
                    contentWidth: width
                    contentHeight: legalBody.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick
                    DirectionAwareLabel {
                        id: legalBody
                        width: legalBodyScroll.width
                        text: root.legalDocument === "terms"
                              ? qsTrId("genycaster.setup.terms_body")
                              : qsTrId("genycaster.setup.privacy_body")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                        lineHeight: 1.55
                    }
                }
                SetupPrimaryButton {
                    Layout.fillWidth: true
                    neutralPrimary: true
                    showArrow: false
                    text: qsTrId("genycaster.locale.close")
                    onClicked: legalReader.close()
                }
            }
        }
    }

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

    Item {
        id: setupCanvas
        anchors.fill: parent

        // Split-screen composition from the approved desktop wireframe:
        // the brand artwork occupies the left third, with content and a
        // stage-local CTA on the right. No header, footer or shell chrome.
        Rectangle {
            id: desktopBrandPane
            visible: !root.phone && root.width >= 980
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.min(parent.width * 0.29, 430)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.dark ? "#1B1C2A" : "#F3F1FF" }
                GradientStop { position: 0.55; color: Theme.dark ? "#24253B" : "#E8E8FB" }
                GradientStop { position: 1.0; color: Theme.dark ? "#181923" : "#F2F1FF" }
            }
            Image {
                id: desktopBrandArt
                width: Math.min(630, desktopBrandPane.width * 1.72)
                height: width
                x: -width * 0.34
                y: (desktopBrandPane.height - height) / 2
                source: "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/setup-motif.svg"
                sourceSize: Qt.size(768, 768)
                asynchronous: false
                fillMode: Image.PreserveAspectFit
                opacity: Theme.dark ? 0.66 : 1
            }
        }

        // Four linked editorial markers, each with two real substeps.
        // The half/full fill represents all eight navigable stages.
        Row {
            id: setupProgress
            visible: !root.phone && root.currentStep !== 0
            anchors.top: parent.top
            anchors.topMargin: 58
            x: desktopBrandPane.visible
               ? desktopBrandPane.width + (parent.width - desktopBrandPane.width - width) / 2
               : (parent.width - width) / 2
            spacing: 9
            z: 5
            Accessible.name: root.stepTitle(root.currentStep)
                             + " (" + (root.currentStep + 1) + "/" + root.progressCount + ")"
            Repeater {
                model: Math.ceil(root.progressCount / 2)
                delegate: Rectangle {
                    id: progressMarker
                    required property int index
                    readonly property real completion: Math.max(0, Math.min(
                        1, (root.currentStep - index * 2 + 1) / 2))
                    width: index === 0 || index === Math.ceil(root.progressCount / 2) - 1 ? 22 : 8
                    height: index === 0 || index === Math.ceil(root.progressCount / 2) - 1 ? 4 : 8
                    radius: height / 2
                    color: Theme.borderMuted
                    Rectangle {
                        width: progressMarker.width * progressMarker.completion
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent
                        Behavior on width {
                            NumberAnimation {
                                duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }
            }
        }

        ActionButton {
            z: 20
            visible: root.currentStep > 0
            anchors.top: parent.top
            anchors.topMargin: Theme.space4
            x: Theme.rtl ? root.width - width - (root.phone ? Theme.space2 : Theme.space5)
                         : (root.phone ? Theme.space2 : Theme.space5)
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
            anchors.topMargin: Theme.space4
            x: Theme.rtl ? (root.phone ? Theme.space2 : Theme.space5)
                         : root.width - width - (root.phone ? Theme.space2 : Theme.space5)
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
            contentHeight: Math.max(height, stagePanel.y + stagePanel.implicitHeight
                                    + (root.showsPrimaryCta ? 116 : 32))
            ScrollBar.vertical: GenyScrollBar {
                policy: stageScroll.contentHeight > stageScroll.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            Item {
                id: stagePanel
                width: Math.max(0, Math.min(
                                   root.phone ? 440
                                   : (root.currentStep === 3 || root.currentStep === 6) ? 810 : 650,
                                   stageScroll.width - (desktopBrandPane.visible ? desktopBrandPane.width : 0)
                                   - root.setupGutter * 2))
                x: desktopBrandPane.visible
                   ? desktopBrandPane.width + (stageScroll.width - desktopBrandPane.width - width) / 2
                   : (stageScroll.width - width) / 2
                y: root.currentStep === 0 || root.currentStep === 7
                   ? Math.max(root.phone ? 110 : 52, (stageScroll.height - stageLoader.implicitHeight) * 0.39)
                   : root.currentStep === 5
                   ? Math.max(96, (stageScroll.height - stageLoader.implicitHeight) * 0.38)
                   : root.currentStep === 1
                   ? Math.max(76, (stageScroll.height - stageLoader.implicitHeight) * 0.21)
                   : Math.max(root.phone ? 100 : 115, Math.min(root.phone ? 145 : 125,
                                                                   stageScroll.height * 0.15))
                implicitHeight: stageLoader.implicitHeight

                Loader {
                    id: stageLoader
                    width: parent.width
                    opacity: root.stageOpacity
                    x: root.stageOffset
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.reduceMotion ? 0 : 150; easing.type: Easing.OutCubic }
                    }
                    Behavior on x {
                        NumberAnimation { duration: Theme.reduceMotion ? 0 : 190; easing.type: Easing.OutCubic }
                    }
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

        // An action belonging to the active onboarding stage, not a footer bar.
        SetupPrimaryButton {
            id: stageAction
            visible: root.showsPrimaryCta
            z: 10
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.phone ? Theme.space5 : Math.max(38, root.height * 0.085)
            width: root.phone ? Math.min(root.width - root.setupGutter * 2, 440)
                              : Math.min(300, stagePanel.width * 0.65)
            x: desktopBrandPane.visible
               ? desktopBrandPane.width + (parent.width - desktopBrandPane.width - width) / 2
               : (parent.width - width) / 2
            text: root.actionText()
            neutralPrimary: true
            enabled: !root.stageTransitioning && root.actionEnabled()
            showArrow: root.currentStep === 0
            onClicked: root.nextStep()
        }
    }

    Component {
        id: welcomeStage
        ColumnLayout {
            spacing: Theme.space4
            LayoutMirroring.enabled: Theme.rtl

            Image {
                Layout.preferredWidth: root.phone ? 62 : 42
                Layout.preferredHeight: root.phone ? 62 : 42
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
                    model: root.progressCount
                    delegate: Superellipse {
                        required property int index
                        width: root.progressWidth(index)
                        height: 6
                        curveDepth: 3
                        fillColor: index === root.currentStep ? Theme.accent
                                   : index < root.currentStep ? Theme.inkMuted : Theme.borderStrong
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
                storyboardStyle: true
                compactDesktopStyle: !root.phone
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
                onClicked: root.openLegalDocument("terms")
            }
            SetupActionCard {
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                title: qsTrId("genycaster.setup.privacy_title")
                showChevron: true
                onClicked: root.openLegalDocument("privacy")
            }
            Button {
                id: consentCheck
                Layout.fillWidth: true
                Layout.topMargin: Theme.space3
                implicitHeight: Math.max(56, consentLabel.implicitHeight + Theme.space3 * 2)
                enabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: qsTrId("genycaster.setup.single_legal_consent")
                Accessible.checked: root.consentChecked
                onClicked: root.consentChecked = !root.consentChecked
                background: Superellipse {
                    fillColor: consentCheck.down ? Theme.surfacePressed
                               : consentCheck.hovered ? Theme.surfaceHover
                               : Theme.surfaceMuted
                    borderWidth: Theme.keyboardFocusVisible(consentCheck) ? 2 : 0
                    borderColor: Theme.focus
                    curveDepth: Theme.curveContainer
                }
                contentItem: RowLayout {
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection
                    Superellipse {
                        Layout.preferredWidth: 23
                        Layout.preferredHeight: 23
                        fillColor: root.consentChecked ? Theme.ink : "transparent"
                        borderColor: Theme.inkMuted
                        borderWidth: 1
                        curveDepth: 5
                        Icon {
                            anchors.centerIn: parent
                            visible: root.consentChecked
                            text: Theme.iconCheck
                            color: Theme.surface
                            preferredSize: 15
                        }
                    }
                    DirectionAwareLabel {
                        id: consentLabel
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.single_legal_consent")
                        color: Theme.ink
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Component {
        id: accountStage
        ColumnLayout {
            spacing: Theme.space3
            SetupPageHeader {
                storyboardStyle: true
                compactDesktopStyle: !root.phone
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
                    accountShowcase: true
                    setupEmphasis: true
                    title: qsTrId("genycaster.setup.design_create_account")
                    subtitle: qsTrId("genycaster.setup.new_profile_body") + " ("
                              + qsTrId("genycaster.shell.coming_soon") + ")"
                    iconGlyph: Theme.iconUser
                    showChevron: false
                    enabled: false
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    onboardingStyle: true
                    accountShowcase: true
                    title: qsTrId("genycaster.setup.design_existing_account")
                    subtitle: qsTrId("genycaster.setup.design_existing_description")
                    iconGlyph: Theme.iconLink
                    showChevron: false
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
                storyboardStyle: true
                compactDesktopStyle: !root.phone
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
                brandImageSource: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
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
                        // Browser-only DOM phrase dialog, never marshal its
                        // secret through Qt/WebAssembly or storage.
                        root.recoverAccount()
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
                    implicitHeight: 58
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    leftPadding: Theme.space3
                    rightPadding: Theme.space3
                    onVisibleChanged: if (!visible) clear()
                    Accessible.name: qsTrId("genycaster.setup.native_recovery_placeholder")
                    background: Superellipse {
                        fillColor: Theme.surface
                        borderWidth: recoveryField.activeFocus ? 2 : Theme.strokeStandard
                        borderColor: recoveryField.activeFocus ? Theme.focus : Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }
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
                        root.recoverySubmitPending = true
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
                    running: root.verifyingBusy || root.recoverySubmitPending
                    visible: !root.authenticated && (root.verifyingBusy || root.recoverySubmitPending)
                    color: Theme.accent
                }
                Icon {
                    anchors.centerIn: parent
                    visible: root.authenticated
                    text: Theme.iconCheck
                    color: Theme.accent
                    preferredSize: 44
                    scale: visible ? 1 : 0.7
                    Behavior on scale {
                        NumberAnimation {
                            duration: Theme.reduceMotion ? 0 : 260
                            easing.type: Easing.OutBack
                        }
                    }
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.motionEmphasis }
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticated ? qsTrId("genycaster.setup.account_verified")
                    : root.authenticationPhase === "linking-farcaster"
                    ? qsTrId("genycaster.shell.link_your_farcaster_account")
                    : root.authenticationPhase === "awaiting-approval"
                    ? qsTrId("genycaster.shell.approve_sign_in")
                    : root.verifyingBusy || root.recoverySubmitPending
                    ? qsTrId("genycaster.setup.design_verifying_title")
                    : qsTrId("genycaster.setup.sign_in_required")
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
                visible: root.authenticated
                Layout.fillWidth: true
                neutralPrimary: true
                showArrow: false
                text: qsTrId("genycaster.setup.continue")
                onClicked: root.switchStep(6)
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
                visible: root.authenticationInProgress || root.recoverySubmitPending
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
                storyboardStyle: true
                compactDesktopStyle: !root.phone
                Layout.fillWidth: true
                iconGlyph: Theme.iconPalette
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
