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

    property bool legalTermsChecked: false
    property bool legalPrivacyChecked: false
    property bool legalTermsReviewed: false
    property bool legalPrivacyReviewed: false
    property string legalReaderKind: ""
    property bool legalReaderReachedEnd: false
    property bool languageOrbitUserControlled: false
    property int currentStep: 0
    property int highestReachedStep: 0
    property real stageReveal: 1.0

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal openApproval()
    signal cancelAuthentication()
    signal acceptLegalConsent(bool termsAccepted, bool privacyAcknowledged, bool reviewCompleted)
    signal finishSetup()

    readonly property bool phone: width < 680
    readonly property bool compact: width < 920

    // Setup design-system layer: every stage composes the same neutral surfaces,
    // typography scale and spacing rhythm. Accent is reserved for focus/action,
    // while semantic colors are used only for compact status feedback.
    readonly property int setupContentWidth: 720
    readonly property int setupPageTitleSize: Theme.typeTitle2Size
    readonly property int setupBodySize: Theme.typeLabelSize
    readonly property int setupSectionTitleSize: Theme.typeTitle3Size
    readonly property int setupCardPadding: Theme.space4
    readonly property int setupGap: Theme.space4
    readonly property color setupCard: Theme.surface
    readonly property color setupCardRaised: Theme.surfaceRaised
    readonly property color setupBorder: Theme.borderMuted
    readonly property color setupSelectedBorder: Theme.accent

    readonly property bool authenticationStarting: authenticationPhase === "starting"
    readonly property bool awaitingApproval: authenticationPhase === "awaiting-approval"
    readonly property bool linkingFarcaster: authenticationPhase === "linking-farcaster"
    readonly property bool authenticationInProgress:
        authenticationStarting || awaitingApproval || linkingFarcaster
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
        case 5: return "social/verified"
        default: return "social/verified"
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

    function greetingOrbitOffset(index, currentIndex) {
        const count = greetings.length
        if (count <= 0)
            return 0
        let delta = index - currentIndex
        if (delta > count / 2)
            delta -= count
        else if (delta < -count / 2)
            delta += count
        return delta
    }

    function stepComplete(index) {
        if (index === 0)
            return highestReachedStep > 0
        if (index === 1)
            return highestReachedStep > 1
        if (index === 2)
            return legalConsentAccepted || (legalTermsChecked && legalPrivacyChecked)
        if (index === 3)
            return highestReachedStep > 3
        if (index === 4)
            return authenticated
        return setupCompleted
    }

    function stepUnlocked(index) {
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
        if (legalReaderKind === "privacy") {
            legalPrivacyReviewed = true
            legalPrivacyChecked = true
        } else {
            legalTermsReviewed = true
            legalTermsChecked = true
        }
        legalReader.close()
    }

    function switchStep(index) {
        index = Math.max(0, Math.min(5, index))
        if (!legalConsentAccepted && !(legalTermsChecked && legalPrivacyChecked) && index > 2)
            index = 2
        if (!authenticated && index > 4)
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
        if (!legalConsentAccepted && !(legalTermsChecked && legalPrivacyChecked) && currentStep > 2) {
            switchStep(2)
            return
        }
        if (!authenticated && currentStep > 4)
            switchStep(4)
    }

    function nextStep() {
        if (currentStep === 0 || currentStep === 1) {
            switchStep(currentStep + 1)
            return
        }

        if (currentStep === 2) {
            if (!legalConsentAccepted) {
                if (!legalTermsChecked || !legalPrivacyChecked)
                    return
                acceptLegalConsent(legalTermsChecked, legalPrivacyChecked, true)
            }
            Qt.callLater(() => switchStep(3))
            return
        }

        if (currentStep === 3) {
            switchStep(4)
            return
        }

        if (currentStep === 4) {
            if (authenticated)
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
            return legalConsentAccepted || (legalTermsChecked && legalPrivacyChecked)
        if (currentStep === 4)
            return authenticated
        if (currentStep === 5)
            return authenticated && legalConsentAccepted
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

    onAuthenticatedChanged: {
        if (authenticated && currentStep === 4)
            Qt.callLater(() => switchStep(5))
        else
            Qt.callLater(synchronizeFlow)
    }

    onSetupCompletedChanged: {
        if (!setupCompleted) {
            highestReachedStep = 0
            Qt.callLater(() => switchStep(0))
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
        if (!setupCompleted)
            switchStep(0)
        else
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

                    Superellipse {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        fillColor: Theme.surfaceRaised
                        borderWidth: Theme.strokeStandard
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveCompact

                        Icon {
                            anchors.centerIn: parent
                            text: "social/sparkles"
                            color: Theme.accent
                            preferredSize: Theme.iconSize
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: qsTrId("genycaster.app.genycaster")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        Label {
                            visible: !root.phone
                            text: qsTrId("genycaster.setup.title")
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Row {
                    visible: root.phone
                    spacing: 6
                    Repeater {
                        model: root.wizardSteps
                        delegate: Rectangle {
                            required property int modelData
                            width: modelData === root.currentStep ? 28 : 12
                            height: 4
                            radius: 2
                            color: modelData === root.currentStep ? Theme.accent
                                 : modelData < root.currentStep ? Theme.inkMuted
                                 : Theme.borderStrong
                            Behavior on width {
                                NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                }

                Label {
                    visible: !root.phone
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

            Rectangle {
                visible: !root.compact
                Layout.preferredWidth: 224
                Layout.fillHeight: true
                color: Theme.surface
                border.width: 0

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.borderMuted
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: Theme.space6
                    anchors.bottomMargin: Theme.space5
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.title")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: (root.currentStep + 1) + " / " + root.wizardSteps.length
                            color: Theme.inkSubtle
                            font.family: Theme.latinFontFamily
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Rectangle {
                            x: 21
                            y: 23
                            width: 2
                            height: Math.max(0, stepRail.height - 46)
                            radius: 1
                            color: Theme.borderMuted
                        }

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
                                    height: 54
                                    enabled: root.stepUnlocked(modelData)
                                    opacity: enabled ? 1.0 : 0.42
                                    hoverEnabled: true
                                    onClicked: root.switchStep(modelData)

                                    background: Superellipse {
                                        fillColor: stepButton.current
                                                   ? Theme.surfaceRaised
                                                   : stepButton.hovered ? Theme.surfaceHover : "transparent"
                                        borderWidth: stepButton.current ? Theme.strokeStandard : 0
                                        borderColor: stepButton.current ? Theme.borderStrong : "transparent"
                                        curveDepth: Theme.curveControl
                                    }

                                    contentItem: RowLayout {
                                        spacing: Theme.space3
                                        layoutDirection: Theme.uiDirection

                                        Rectangle {
                                            Layout.preferredWidth: 30
                                            Layout.preferredHeight: 30
                                            radius: 15
                                            color: current ? Theme.accent : Theme.surfaceMuted
                                            border.width: Theme.strokeStandard
                                            border.color: current ? Theme.accent : Theme.borderMuted

                                            Icon {
                                                anchors.centerIn: parent
                                                visible: complete
                                                text: "social/verified"
                                                color: Theme.inkMuted
                                                preferredSize: 17
                                            }

                                            Label {
                                                anchors.centerIn: parent
                                                visible: !complete
                                                text: String(modelData + 1)
                                                color: current ? Theme.contrastInk(Theme.accent) : Theme.inkSubtle
                                                font.family: Theme.latinFontFamily
                                                font.pixelSize: Theme.typeCaptionSize
                                                font.weight: Theme.weightBold
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.stepTitle(modelData)
                                                color: current ? Theme.ink : Theme.inkMuted
                                                font.pixelSize: Theme.typeLabelSize
                                                font.weight: current ? Theme.weightBold : Theme.weightMedium
                                                elide: Text.ElideRight
                                            }

                                            Label {
                                                visible: current
                                                Layout.fillWidth: true
                                                text: qsTrId("genycaster.setup.title")
                                                color: Theme.accent
                                                font.pixelSize: Theme.typeCaptionSize
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Icon {
                                            visible: current
                                            text: Theme.iconArrowRight
                                            color: Theme.accent
                                            preferredSize: 15
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        fillColor: Theme.surfaceRaised
                        borderWidth: Theme.strokeStandard
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space2

                            Icon {
                                text: "social/verified"
                                color: Theme.accent
                                preferredSize: 18
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.setup.welcome_eyebrow")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.WordWrap
                            }
                        }
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

                    ScrollBar.vertical: ScrollBar {
                        policy: stageFlick.contentHeight > stageFlick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                    }

                    Item {
                        id: stageCanvas
                        width: Math.min(stageFlick.width - (root.phone ? Theme.space4 * 2 : Theme.space6 * 2),
                                        root.compact ? root.setupContentWidth : 820)
                        x: Math.max(root.phone ? Theme.space4 : Theme.space6,
                                    Math.round((stageFlick.width - width) / 2))
                        height: Math.max(stageFlick.height,
                                         stageLoader.y + stageLoader.implicitHeight + Theme.space6)

                        Loader {
                            id: stageLoader
                            width: parent.width
                            y: (root.currentStep === 0 || root.currentStep === 5) && !root.phone
                               ? Math.max(Theme.space5,
                                          Math.round((stageFlick.height - implicitHeight) / 2) - Theme.space3)
                               : Theme.space5
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
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64

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
                            iconGlyph: Theme.iconBack
                            quiet: true
                            enabled: !root.authenticationInProgress
                            onClicked: root.previousStep()
                        }

                        Item { Layout.fillWidth: true }

                        ActionButton {
                            visible: root.currentStep !== 4 || root.authenticated
                            text: root.primaryActionText()
                            prominent: true
                            enabled: root.primaryActionEnabled()
                            iconGlyph: root.currentStep === 5 ? "social/verified" : ""
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
        width: Math.min(root.width - (root.phone ? Theme.space3 * 2 : Theme.space7 * 2), 700)
        height: Math.min(root.height - (root.phone ? Theme.space3 * 2 : Theme.space6 * 2), 720)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
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

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                Layout.leftMargin: Theme.space4
                Layout.rightMargin: Theme.space3
                spacing: Theme.space3

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
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.legalReaderKind === "privacy"
                              ? "genyleap.com/privacy"
                              : "genyleap.com/terms"
                        color: Theme.inkSubtle
                        font.family: Theme.codeFontFamily
                        font.pixelSize: Theme.typeCaptionSize
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

                ScrollBar.vertical: ScrollBar {
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
                        text: root.legalReaderKind === "privacy"
                              ? qsTrId("genycaster.setup.privacy_updated")
                              : qsTrId("genycaster.setup.terms_updated")
                        color: Theme.accent
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
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
                        lineHeight: 1.34
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        width: parent.width
                        height: Theme.strokeHairline
                        color: Theme.borderMuted
                    }

                    Label {
                        width: parent.width
                        text: qsTrId("genycaster.setup.legal_governing_note")
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        lineHeight: Theme.typeCaptionLineHeight
                        wrapMode: Text.WordWrap
                    }

                    ActionButton {
                        width: parent.width
                        text: root.legalReaderKind === "privacy"
                              ? qsTrId("genycaster.setup.open_privacy")
                              : qsTrId("genycaster.setup.open_terms")
                        iconGlyph: Theme.iconArrowUpRight
                        quiet: true
                        onClicked: presentationBridge.openExternalUrl(
                            root.legalReaderKind === "privacy"
                            ? "https://genyleap.com/privacy"
                            : "https://genyleap.com/terms")
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
                Layout.preferredHeight: 78
                Layout.leftMargin: Theme.space4
                Layout.rightMargin: Theme.space4
                spacing: Theme.space3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Icon {
                        text: root.legalReaderReachedEnd ? "social/verified" : Theme.iconInfo
                        color: root.legalReaderReachedEnd ? Theme.success : Theme.inkSubtle
                        preferredSize: 16
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.legalReaderReachedEnd
                              ? qsTrId("genycaster.setup.legal_read_complete")
                              : qsTrId("genycaster.setup.scroll_to_continue")
                        color: root.legalReaderReachedEnd ? Theme.success : Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                }

                ActionButton {
                    text: root.legalReaderKind === "privacy"
                          ? qsTrId("genycaster.setup.acknowledge_privacy")
                          : qsTrId("genycaster.setup.accept_terms")
                    prominent: true
                    enabled: root.legalReaderReachedEnd
                    iconGlyph: "social/verified"
                    onClicked: root.acceptLegalReader()
                }
            }
        }
    }

    Component {
        id: welcomeStage

        ColumnLayout {
            spacing: Theme.space4

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTrId("genycaster.setup.welcome_eyebrow")
                color: Theme.accent
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }

            Label {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.welcome_title")
                color: Theme.ink
                font.pixelSize: root.setupPageTitleSize
                font.weight: Theme.weightBold
                horizontalAlignment: Text.AlignHCenter
                lineHeight: Theme.typeTitle2LineHeight
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                Layout.maximumWidth: 520
                Layout.alignment: Qt.AlignHCenter
                text: qsTrId("genycaster.setup.language_orbit_hint")
                color: Theme.inkMuted
                font.pixelSize: root.setupBodySize
                horizontalAlignment: Text.AlignHCenter
                lineHeight: Theme.typeLabelLineHeight
                wrapMode: Text.WordWrap
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.maximumWidth: 520
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: root.phone ? 286 : 306
                fillColor: "transparent"
                borderWidth: 0
                borderColor: "transparent"
                curveDepth: Theme.curveContainer

                Item {
                    id: orbitFrame
                    anchors.fill: parent
                    anchors.margins: Theme.space4

                    readonly property real sphereSize: Math.min(width, height - 48, root.phone ? 232 : 252)

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        width: orbitFrame.sphereSize
                        height: orbitFrame.sphereSize
                        radius: width / 2
                        color: "transparent"
                        border.width: Theme.strokeHairline
                        border.color: Theme.borderStrong
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 4 + orbitFrame.sphereSize * 0.33
                        width: orbitFrame.sphereSize
                        height: orbitFrame.sphereSize * 0.34
                        radius: height / 2
                        color: "transparent"
                        border.width: Theme.strokeHairline
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
                    }

                    Rectangle {
                        visible: false
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 4 + orbitFrame.sphereSize * 0.12
                        width: orbitFrame.sphereSize * 0.48
                        height: orbitFrame.sphereSize * 0.76
                        radius: width / 2
                        color: "transparent"
                        border.width: Theme.strokeHairline
                        border.color: Theme.borderMuted
                    }

                    Superellipse {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 4 + (orbitFrame.sphereSize - 54) / 2
                        width: 154
                        height: 54
                        fillColor: "transparent"
                        borderWidth: 0
                        borderColor: "transparent"
                        curveDepth: Theme.curveControl
                    }

                    PathView {
                        id: languageOrbit
                        width: orbitFrame.sphereSize
                        height: orbitFrame.sphereSize
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        model: root.greetings
                        pathItemCount: 5
                        preferredHighlightBegin: 0.5
                        preferredHighlightEnd: 0.5
                        highlightRangeMode: PathView.StrictlyEnforceRange
                        snapMode: PathView.SnapToItem
                        maximumFlickVelocity: 1100

                        onMovementStarted: root.languageOrbitUserControlled = true

                        path: Path {
                            startX: languageOrbit.width / 2
                            startY: 16

                            PathCubic {
                                control1X: languageOrbit.width * 0.20
                                control1Y: languageOrbit.height * 0.22
                                control2X: languageOrbit.width * 0.24
                                control2Y: languageOrbit.height * 0.40
                                x: languageOrbit.width / 2
                                y: languageOrbit.height / 2
                            }

                            PathCubic {
                                control1X: languageOrbit.width * 0.76
                                control1Y: languageOrbit.height * 0.60
                                control2X: languageOrbit.width * 0.80
                                control2Y: languageOrbit.height * 0.78
                                x: languageOrbit.width / 2
                                y: languageOrbit.height - 16
                            }
                        }

                        delegate: Item {
                            required property int index
                            required property var modelData
                            readonly property int orbitOffset:
                                root.greetingOrbitOffset(index, languageOrbit.currentIndex)
                            readonly property int orbitDistance: Math.abs(orbitOffset)
                            width: 150
                            height: 48
                            scale: PathView.isCurrentItem ? 1.0 : orbitDistance <= 1 ? 0.82 : 0.70
                            opacity: PathView.isCurrentItem ? 1.0 : orbitDistance <= 1 ? 0.48 : 0.24
                            z: PathView.isCurrentItem ? 4 : 1

                            transform: Rotation {
                                origin.x: width / 2
                                origin.y: height / 2
                                axis.x: 1
                                axis.y: 0
                                axis.z: 0
                                angle: Math.max(-52, Math.min(52, orbitOffset * 24))
                            }

                            Label {
                                anchors.centerIn: parent
                                text: modelData.hello
                                color: PathView.isCurrentItem ? Theme.ink : Theme.inkMuted
                                font.family: Theme.fontFamilyForText(modelData.hello)
                                font.pixelSize: PathView.isCurrentItem ? 24 : Theme.typeLabelSize
                                font.weight: PathView.isCurrentItem ? Theme.weightBold : Theme.weightMedium
                            }

                            TapHandler {
                                onPressedChanged: {
                                    if (pressed)
                                        root.languageOrbitUserControlled = true
                                }
                                onTapped: {
                                    root.languageOrbitUserControlled = true
                                    languageOrbit.currentIndex = index
                                    localizationController.setLanguageTag(modelData.tag)
                                }
                            }
                        }

                        Timer {
                            interval: 2100
                            repeat: true
                            running: root.visible
                                     && root.currentStep === 0
                                     && !root.languageOrbitUserControlled
                                     && !Theme.reduceMotion
                            onTriggered: {
                                if (languageOrbit.count > 0)
                                    languageOrbit.currentIndex =
                                        (languageOrbit.currentIndex + 1) % languageOrbit.count
                            }
                        }

                        Component.onCompleted: currentIndex =
                            root.greetingIndexForTag(localizationController.effectiveLanguageTag)

                        onCurrentIndexChanged: {
                            if (!root.languageOrbitUserControlled
                                    || currentIndex < 0
                                    || currentIndex >= root.greetings.length)
                                return
                            const selected = root.greetings[currentIndex]
                            if (selected && selected.tag)
                                localizationController.setLanguageTag(selected.tag)
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: languageRow.top
                        anchors.bottomMargin: Theme.space3
                        height: Theme.strokeHairline
                        color: Theme.borderMuted
                    }

                    RowLayout {
                        id: languageRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        spacing: Theme.space2

                        Icon {
                            text: Theme.iconLanguage
                            color: Theme.accent
                            preferredSize: Theme.iconSize
                        }

                        Label {
                            Layout.fillWidth: true
                            text: localizationController.languageLabel
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTrId("genycaster.setup.language_applies_now")
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                            elide: Text.ElideRight
                            visible: !root.phone
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                Label {
                    text: qsTrId("genycaster.setup.welcome_personal_title")
                    color: Theme.ink
                    font.pixelSize: root.setupPageTitleSize
                    font.weight: Theme.weightBold
                    lineHeight: Theme.typeTitle2LineHeight
                }

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 620
                    text: qsTrId("genycaster.setup.personalize_body")
                    color: Theme.inkMuted
                    font.pixelSize: root.setupBodySize
                    lineHeight: Theme.typeLabelLineHeight
                    wrapMode: Text.WordWrap
                }
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

                    Item {
                        visible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0

                        Rectangle {
                            anchors.centerIn: parent
                            width: 142
                            height: width
                            radius: width / 2
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.035)
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 104
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 62
                            height: width
                            radius: width / 2
                            color: Theme.accent

                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconPalette
                                color: Theme.contrastInk(Theme.accent)
                                preferredSize: Theme.iconSizeLarge
                            }
                        }

                        Superellipse {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            width: Math.min(parent.width, 180)
                            height: 40
                            fillColor: Theme.surface
                            borderWidth: 1
                            borderColor: Theme.borderMuted
                            curveDepth: Theme.curveControl

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: Theme.space2

                                Rectangle {
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                    radius: 7
                                    color: Theme.accent
                                    border.width: 1
                                    border.color: Theme.borderStrong
                                }

                                Label {
                                    text: root.accentCode()
                                    color: Theme.ink
                                    font.family: Theme.codeFontFamily
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightBold
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3

                            Superellipse {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                fillColor: Theme.surfaceRaised
                                borderWidth: Theme.strokeStandard
                                borderColor: Theme.borderMuted
                                curveDepth: Theme.curveCompact

                                Icon {
                                    anchors.centerIn: parent
                                    text: Theme.iconLanguage
                                    color: Theme.accent
                                    preferredSize: 19
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    text: qsTrId("genycaster.settings.language_region")
                                    color: Theme.ink
                                    font.pixelSize: root.setupSectionTitleSize
                                    font.weight: Theme.weightDemiBold
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: localizationController.languageLabel
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.phone ? 1 : 2
                            columnSpacing: Theme.space3
                            rowSpacing: Theme.space3

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1

                                Label {
                                    text: qsTrId("genycaster.locale.regional_format")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
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
                                    text: qsTrId("genycaster.locale.time_zone")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
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

                            Icon {
                                text: Theme.iconPalette
                                color: Theme.accent
                                preferredSize: 20
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.settings.appearance")
                                color: Theme.ink
                                font.pixelSize: root.setupSectionTitleSize
                                font.weight: Theme.weightDemiBold
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            Repeater {
                                model: [
                                    { id: "system", label: qsTrId("genycaster.settings.mode_system"), icon: Theme.iconSystem },
                                    { id: "light", label: qsTrId("genycaster.settings.mode_light"), icon: Theme.iconSun },
                                    { id: "dark", label: qsTrId("genycaster.settings.mode_dark"), icon: Theme.iconMoon }
                                ]

                                delegate: ActionButton {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    compact: true
                                    text: modelData.label
                                    iconGlyph: modelData.icon
                                    selected: appearanceController.themeMode === modelData.id
                                    onClicked: appearanceController.setThemeMode(modelData.id)
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.phone ? Theme.space1 : Theme.space2

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
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 48
                                    implicitHeight: 40
                                    Accessible.name: modelData.id
                                    onClicked: appearanceController.setColorThemeId(modelData.id)

                                    background: Item {}

                                    contentItem: Item {
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: appearanceController.colorThemeId === modelData.id ? 30 : 24
                                            height: width
                                            radius: width / 2
                                            color: modelData.color
                                            border.width: appearanceController.colorThemeId === modelData.id ? 3 : 1
                                            border.color: appearanceController.colorThemeId === modelData.id
                                                          ? Theme.surface : Theme.borderStrong

                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: Theme.motionStandard
                                                    easing.type: Easing.OutCubic
                                                }
                                            }

                                            Icon {
                                                anchors.centerIn: parent
                                                visible: appearanceController.colorThemeId === modelData.id
                                                text: "social/verified"
                                                color: Theme.contrastInk(modelData.color)
                                                preferredSize: 15
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3

                            Icon {
                                text: Theme.iconAccessibility
                                color: appearanceController.reduceMotion ? Theme.accent : Theme.inkSubtle
                                preferredSize: 18
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.settings.reduce_motion")
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                            }

                            GenySwitch {
                                checked: appearanceController.reduceMotion
                                onToggled: appearanceController.setReduceMotion(checked)
                            }
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                Label {
                    text: qsTrId("genycaster.setup.legal_title")
                    color: Theme.ink
                    font.pixelSize: root.setupPageTitleSize
                    font.weight: Theme.weightBold
                    lineHeight: Theme.typeTitle2LineHeight
                }

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 620
                    text: qsTrId("genycaster.setup.privacy_compact_intro")
                    color: Theme.inkMuted
                    font.pixelSize: root.setupBodySize
                    lineHeight: Theme.typeLabelLineHeight
                    wrapMode: Text.WordWrap
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 1
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

                    delegate: Button {
                        id: legalCardButton
                        required property var modelData
                        readonly property bool reviewed: modelData.privacy
                                                         ? root.legalPrivacyReviewed
                                                         : root.legalTermsReviewed
                        Layout.fillWidth: true
                        Layout.preferredHeight: 82
                        enabled: true
                        hoverEnabled: true
                        onClicked: root.openLegalReader(modelData.privacy ? "privacy" : "terms")

                        background: Superellipse {
                            fillColor: legalCardButton.hovered ? Theme.surfaceHover : root.setupCard
                            borderWidth: Theme.strokeStandard
                            borderColor: legalCardButton.reviewed
                                         ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.26)
                                         : root.setupBorder
                            curveDepth: Theme.curveControl
                        }

                        contentItem: RowLayout {
                            spacing: Theme.space3

                            Superellipse {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                fillColor: Theme.surfaceRaised
                                borderWidth: Theme.strokeStandard
                                borderColor: Theme.borderMuted
                                curveDepth: Theme.curveCompact

                                Icon {
                                    anchors.centerIn: parent
                                    text: modelData.icon
                                    color: Theme.inkMuted
                                    preferredSize: Theme.iconSize
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: reviewed
                                          ? qsTrId("genycaster.setup.consent_recorded")
                                          : modelData.body
                                    color: reviewed ? Theme.success : Theme.inkSubtle
                                    font.pixelSize: Theme.typeCaptionSize
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }

                            Icon {
                                text: reviewed ? "social/verified" : Theme.iconArrowRight
                                color: reviewed ? Theme.success : Theme.inkSubtle
                                preferredSize: Theme.iconSize
                            }
                        }
                    }
                }
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeStandard
                borderColor: root.setupBorder
                curveDepth: Theme.curveControl

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    spacing: Theme.space3

                    Icon {
                        text: root.legalTermsChecked && root.legalPrivacyChecked
                              ? "social/verified" : Theme.iconInfo
                        color: root.legalTermsChecked && root.legalPrivacyChecked
                               ? Theme.success : Theme.inkSubtle
                        preferredSize: Theme.iconSize
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.legalConsentAccepted
                              ? qsTrId("genycaster.setup.consent_recorded")
                              : qsTrId("genycaster.setup.privacy_local_note")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightMedium
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        text: (root.legalTermsChecked ? 1 : 0) + (root.legalPrivacyChecked ? 1 : 0) + "/2"
                        color: root.legalTermsChecked && root.legalPrivacyChecked
                               ? Theme.success : Theme.inkSubtle
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightBold
                    }
                }
            }
        }
    }

    Component {
        id: networkStage

        ColumnLayout {
            spacing: Theme.space4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                RowLayout {
                    spacing: Theme.space3

                    Icon {
                        text: Theme.iconWifi
                        color: Theme.accent
                        preferredSize: 25
                    }

                    Label {
                        text: qsTrId("genycaster.setup.network_title")
                        color: Theme.ink
                        font.pixelSize: root.setupPageTitleSize
                        font.weight: Theme.weightBold
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 620
                    text: qsTrId("genycaster.setup.network_body")
                    color: Theme.inkMuted
                    font.pixelSize: root.setupBodySize
                    lineHeight: Theme.typeLabelLineHeight
                    wrapMode: Text.WordWrap
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: networkHero.implicitHeight
                fillColor: "transparent"
                borderWidth: 0
                borderColor: "transparent"
                curveDepth: Theme.curveContainer

                GridLayout {
                    id: networkHero
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 0
                    columns: 1
                    columnSpacing: 0
                    rowSpacing: Theme.space3

                    Item {
                        visible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0

                        Rectangle {
                            anchors.centerIn: parent
                            width: 126
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 88
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 52
                            height: width
                            radius: width / 2
                            color: Theme.surface
                            border.width: 1
                            border.color: Theme.borderMuted

                            Icon {
                                anchors.centerIn: parent
                                text: "quilibrium/hypersnap"
                                color: nodeSelectionController.probing ? Theme.inkMuted : Theme.accent
                                preferredSize: Theme.iconSizeLarge
                            }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            radius: 4
                            color: Theme.success
                            anchors.horizontalCenterOffset: -58
                            anchors.verticalCenterOffset: 16
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.accent
                            opacity: 0.55
                            anchors.horizontalCenterOffset: 46
                            anchors.verticalCenterOffset: -42
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            ActionButton {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.setup.network_automatic")
                                iconGlyph: Theme.iconFidelity
                                selected: nodeSelectionController.automatic
                                onClicked: nodeSelectionController.useAutomaticSelection()
                            }

                            ActionButton {
                                text: qsTrId("genycaster.setup.network_retest")
                                iconGlyph: Theme.iconRefresh
                                quiet: true
                                busy: nodeSelectionController.probing
                                onClicked: nodeSelectionController.refresh()
                            }
                        }

                        Superellipse {
                            Layout.fillWidth: true
                            implicitHeight: experimentalRow.implicitHeight + Theme.space3 * 2
                            fillColor: Theme.surfaceRaised
                            borderWidth: Theme.strokeStandard
                            borderColor: Theme.borderMuted
                            curveDepth: Theme.curveControl

                            RowLayout {
                                id: experimentalRow
                                anchors.fill: parent
                                anchors.margins: Theme.space3
                                spacing: Theme.space2

                                Icon {
                                    text: Theme.iconWarning
                                    color: Theme.warning
                                    preferredSize: 17
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.setup.network_experimental")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Repeater {
                    model: nodeSelectionController.nodes

                    delegate: Button {
                        id: nodeButton
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 64
                        enabled: modelData.status !== "offline"
                        hoverEnabled: true
                        onClicked: nodeSelectionController.selectNode(modelData.id)

                        background: Superellipse {
                            fillColor: nodeButton.hovered ? Theme.surfaceHover : root.setupCard
                            borderWidth: Theme.strokeStandard
                            borderColor: modelData.selected ? root.setupSelectedBorder : root.setupBorder
                            curveDepth: Theme.curveControl
                        }

                        contentItem: RowLayout {
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            Superellipse {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                fillColor: Theme.surfaceRaised
                                borderWidth: Theme.strokeStandard
                                borderColor: Theme.borderMuted
                                curveDepth: Theme.curveCompact

                                Icon {
                                    anchors.centerIn: parent
                                    text: "quilibrium/hypersnap"
                                    color: modelData.online ? Theme.success
                                         : modelData.status === "offline" ? Theme.danger
                                         : Theme.inkMuted
                                    preferredSize: Theme.iconSize
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.label
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeLabelSize
                                        font.weight: Theme.weightBold
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        visible: modelData.recommended
                                        text: qsTrId("genycaster.setup.network_best")
                                        color: Theme.success
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightBold
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.endpoint
                                    color: Theme.inkSubtle
                                    font.family: Theme.codeFontFamily
                                    font.pixelSize: Theme.typeCaptionSize
                                    elide: Text.ElideMiddle
                                }
                            }

                            Column {
                                spacing: 1

                                Label {
                                    anchors.right: parent.right
                                    text: modelData.status === "checking" ? "…"
                                        : modelData.latencyMs >= 0 ? modelData.latencyMs + " ms"
                                        : "—"
                                    color: modelData.online ? Theme.success
                                         : modelData.status === "offline" ? Theme.danger
                                         : Theme.inkMuted
                                    font.family: Theme.latinFontFamily
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightBold
                                }

                                Label {
                                    anchors.right: parent.right
                                    text: modelData.status === "online" ? qsTrId("genycaster.setup.network_online")
                                        : modelData.status === "offline" ? qsTrId("genycaster.setup.network_unreachable")
                                        : qsTrId("genycaster.setup.network_checking")
                                    color: Theme.inkSubtle
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }

                            Icon {
                                visible: modelData.selected
                                text: "social/verified"
                                color: Theme.success
                                preferredSize: 20
                            }
                        }
                    }
                }
            }

            Superellipse {
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

                    Icon {
                        text: nodeSelectionController.restartRequired
                              ? Theme.iconWarning : "social/verified"
                        color: nodeSelectionController.restartRequired ? Theme.warning : Theme.success
                        preferredSize: 18
                    }

                    Label {
                        Layout.fillWidth: true
                        text: nodeSelectionController.restartRequired
                              ? qsTrId("genycaster.setup.network_restart_note")
                              : qsTrId("genycaster.setup.network_current_note")
                        color: nodeSelectionController.restartRequired ? Theme.warning : Theme.success
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                Label {
                    text: root.authenticated
                          ? qsTrId("genycaster.setup.account_verified")
                          : qsTrId("genycaster.setup.sign_in_required")
                    color: Theme.ink
                    font.pixelSize: root.setupPageTitleSize
                    font.weight: Theme.weightBold
                    lineHeight: Theme.typeTitle2LineHeight
                }

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 620
                    text: root.phaseBody()
                    color: Theme.inkMuted
                    font.pixelSize: root.setupBodySize
                    lineHeight: Theme.typeLabelLineHeight
                    wrapMode: Text.WordWrap
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

                GridLayout {
                    id: verifiedGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: root.setupCardPadding
                    columns: 1
                    columnSpacing: 0
                    rowSpacing: Theme.space3

                    Item {
                        visible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0

                        Rectangle {
                            anchors.centerIn: parent
                            width: 124
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.26)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 92
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.16)
                        }

                        Avatar {
                            anchors.centerIn: parent
                            sourceUrl: root.accountAvatarUrl
                            displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            avatarSize: 74
                        }

                        Superellipse {
                            width: 32
                            height: 32
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 30
                            anchors.verticalCenterOffset: 30
                            fillColor: Theme.success
                            curveDepth: 11
                            borderWidth: 3
                            borderColor: root.setupCard

                            Icon {
                                anchors.centerIn: parent
                                text: "social/verified"
                                color: Theme.contrastInk(Theme.success)
                                preferredSize: 16
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        Label {
                            Layout.fillWidth: true
                            text: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            color: Theme.ink
                            font.pixelSize: root.setupSectionTitleSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            visible: root.accountUsername.length > 0
                            text: "@" + root.accountUsername
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeLabelSize
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            Repeater {
                                model: [
                                    {
                                        icon: "brands/geny",
                                        title: qsTrId("genycaster.shell.verified_by_openproof")
                                    },
                                    {
                                        icon: "brands/farcaster",
                                        title: qsTrId("genycaster.setup.account_verified_body")
                                    }
                                ]

                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: Theme.space2

                                    Superellipse {
                                        Layout.preferredWidth: 30
                                        Layout.preferredHeight: 30
                                        fillColor: Theme.surfaceRaised
                                        borderWidth: Theme.strokeStandard
                                        borderColor: Theme.borderMuted
                                        curveDepth: Theme.curveCompact

                                        Icon {
                                            anchors.centerIn: parent
                                            text: modelData.icon
                                            color: Theme.accent
                                            preferredSize: 18
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                        wrapMode: Text.WordWrap
                                    }

                                    Icon {
                                        text: "social/verified"
                                        color: Theme.success
                                        preferredSize: 18
                                    }
                                }
                            }
                        }

                        ActionButton {
                            visible: false
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.continue")
                            prominent: true
                            iconGlyph: Theme.iconArrowRight
                            onClicked: root.nextStep()
                        }
                    }
                }
            }

            GridLayout {
                visible: !root.authenticated
                Layout.fillWidth: true
                columns: root.phone ? 1 : 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space3

                Repeater {
                    model: [
                        {
                            brand: "brands/geny",
                            title: "Genyleap · OpenProof",
                            body: qsTrId("genycaster.setup.account_genyleap_body"),
                            action: qsTrId("genycaster.shell.continue_with_genyleap"),
                            comingSoon: qsTrId("genycaster.setup.account_create_openproof_soon"),
                            farcaster: false
                        },
                        {
                            brand: "brands/farcaster",
                            title: "Farcaster",
                            body: qsTrId("genycaster.setup.account_farcaster_body"),
                            action: qsTrId("genycaster.shell.sign_in_with_farcaster"),
                            comingSoon: qsTrId("genycaster.setup.account_create_farcaster_soon"),
                            farcaster: true
                        }
                    ]

                    delegate: Superellipse {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: authChoiceColumn.implicitHeight + Theme.space4 * 2
                        fillColor: Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveContainer

                        ColumnLayout {
                            id: authChoiceColumn
                            anchors.fill: parent
                            anchors.margins: Theme.space4
                            spacing: Theme.space3

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space3

                                Superellipse {
                                    Layout.preferredWidth: 46
                                    Layout.preferredHeight: 46
                                    fillColor: Theme.surfaceRaised
                                    curveDepth: Theme.curveCompact

                                    Icon {
                                        anchors.centerIn: parent
                                        text: modelData.brand
                                        color: Theme.accent
                                        preferredSize: 24
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: Theme.ink
                                    font.pixelSize: root.setupSectionTitleSize
                                    font.weight: Theme.weightDemiBold
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.body
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.WordWrap
                            }

                            Item { Layout.fillHeight: true }

                            ActionButton {
                                Layout.fillWidth: true
                                text: modelData.action
                                prominent: !modelData.farcaster
                                iconGlyph: Theme.iconSignIn
                                enabled: root.authenticationAvailable && !root.authenticationInProgress
                                busy: root.authenticationInProgress
                                      && (modelData.farcaster
                                          ? root.authenticationRoute === "farcaster"
                                          : root.authenticationRoute !== "farcaster")
                                onClicked: {
                                    if (modelData.farcaster)
                                        root.continueWithFarcaster()
                                    else
                                        root.continueWithGenyleap()
                                }
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: modelData.comingSoon
                                iconGlyph: Theme.iconPlus
                                quiet: true
                                enabled: false
                            }
                        }
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

                    GenySpinner {
                        running: root.authenticationInProgress
                        color: Theme.accent
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.phaseBody()
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }

                    ActionButton {
                        text: root.authenticationRoute === "farcaster"
                              ? qsTrId("genycaster.shell.open_farcaster")
                              : qsTrId("genycaster.shell.open_genyleap")
                        prominent: true
                        iconGlyph: Theme.iconArrowUpRight
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
                visible: root.authenticationErrorText.length > 0
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

                    Icon {
                        text: Theme.iconWarning
                        color: Theme.danger
                        preferredSize: 18
                    }

                    Label {
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

                GenySpinner {
                    running: true
                    color: Theme.accent
                }

                Label {
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
                    columnSpacing: 0
                    rowSpacing: Theme.space3

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.phone ? 136 : 156

                        Rectangle {
                            visible: false
                            anchors.centerIn: parent
                            width: root.phone ? 112 : 128
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                        }

                        Rectangle {
                            visible: false
                            anchors.centerIn: parent
                            width: root.phone ? 88 : 100
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                        }

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
                            fillColor: Theme.success
                            curveDepth: 10
                            borderWidth: 2
                            borderColor: root.setupCard

                            Icon {
                                anchors.centerIn: parent
                                text: "social/verified"
                                color: Theme.contrastInk(Theme.success)
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
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            Icon {
                                text: "social/verified"
                                color: Theme.success
                                preferredSize: 18
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.setup.consent_recorded")
                                color: Theme.success
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
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

                Button {
                    id: readyLanguageButton
                    Layout.fillWidth: true
                    implicitHeight: 64
                    onClicked: root.switchStep(0)

                    background: Superellipse {
                        fillColor: readyLanguageButton.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space3

                        Superellipse {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            fillColor: Theme.surfaceRaised
                            curveDepth: Theme.curveCompact

                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconLanguage
                                color: Theme.accent
                                preferredSize: 19
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: localizationController.languageLabel
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightBold
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: localizationController.regionalLocaleLabel
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }

                        Icon {
                            text: Theme.iconArrowRight
                            color: Theme.inkSubtle
                            preferredSize: 15
                        }
                    }
                }

                Button {
                    id: readyAppearanceButton
                    Layout.fillWidth: true
                    implicitHeight: 64
                    onClicked: root.switchStep(1)

                    background: Superellipse {
                        fillColor: readyAppearanceButton.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space3

                        Rectangle {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: 19
                            color: Theme.accent
                            border.width: 2
                            border.color: Theme.surface
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: root.accentCode()
                                color: Theme.ink
                                font.family: Theme.codeFontFamily
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightBold
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: appearanceController.themeMode
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }

                        Icon {
                            text: Theme.iconArrowRight
                            color: Theme.inkSubtle
                            preferredSize: 15
                        }
                    }
                }

                Button {
                    id: readyNetworkButton
                    Layout.fillWidth: true
                    implicitHeight: 64
                    onClicked: root.switchStep(3)

                    background: Superellipse {
                        fillColor: readyNetworkButton.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space3

                        Superellipse {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            fillColor: Theme.surfaceRaised
                            curveDepth: Theme.curveCompact

                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconWifi
                                color: Theme.success
                                preferredSize: 19
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: "Hypersnap v2"
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightBold
                            }

                            Label {
                                Layout.fillWidth: true
                                text: {
                                    const nodes = nodeSelectionController.nodes
                                    for (let index = 0; index < nodes.length; ++index) {
                                        if (nodes[index].selected && nodes[index].latencyMs >= 0)
                                            return nodes[index].latencyMs + " ms"
                                    }
                                    return qsTrId("genycaster.setup.network_selected")
                                }
                                color: Theme.success
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }

                        Icon {
                            text: Theme.iconArrowRight
                            color: Theme.inkSubtle
                            preferredSize: 15
                        }
                    }
                }

                Button {
                    id: readyAccountButton
                    Layout.fillWidth: true
                    implicitHeight: 64
                    onClicked: root.switchStep(4)

                    background: Superellipse {
                        fillColor: readyAccountButton.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space3

                        Avatar {
                            sourceUrl: root.accountAvatarUrl
                            displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            avatarSize: 38
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: root.accountName.length > 0 ? root.accountName : root.accountUsername
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightBold
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.verified_by_openproof")
                                color: Theme.success
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }

                        Icon {
                            text: "social/verified"
                            color: Theme.success
                            preferredSize: 18
                        }
                    }
                }
            }
        }
    }
}
