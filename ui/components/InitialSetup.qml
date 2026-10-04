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
    property bool languageOrbitUserControlled: false
    property int currentStep: 0
    property real stageReveal: 1.0

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal openApproval()
    signal cancelAuthentication()
    signal acceptLegalConsent(bool termsAccepted, bool privacyAcknowledged, bool reviewCompleted)
    signal finishSetup()

    readonly property bool phone: width < 680
    readonly property bool compact: width < 920
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

    function switchStep(index) {
        index = Math.max(0, Math.min(5, index))
        if (!legalConsentAccepted && !(legalTermsChecked && legalPrivacyChecked) && index > 2)
            index = 2
        if (!authenticated && index > 4)
            index = 4
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
            Qt.callLater(() => switchStep(0))
            return
        }
        Qt.callLater(synchronizeFlow)
    }

    onLegalConsentAcceptedChanged: {
        if (legalConsentAccepted) {
            legalTermsChecked = true
            legalPrivacyChecked = true
        }
        Qt.callLater(synchronizeFlow)
    }

    onVisibleChanged: {
        if (!visible)
            return
        if (legalConsentAccepted) {
            legalTermsChecked = true
            legalPrivacyChecked = true
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
        synchronizeFlow()
    }

    Keys.onEscapePressed: event => {
        if (authenticationInProgress)
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
            Layout.preferredHeight: root.phone ? 70 : 76

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
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        fillColor: Theme.primaryFill
                        curveDepth: 12

                        Icon {
                            anchors.centerIn: parent
                            text: "social/sparkles"
                            color: Theme.accentInk
                            preferredSize: 18
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
                            width: modelData === root.currentStep ? 20 : 6
                            height: 6
                            radius: 3
                            color: modelData < root.currentStep ? Theme.success
                                 : modelData === root.currentStep ? Theme.accent
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

            Item {
                visible: !root.compact
                Layout.preferredWidth: 220
                Layout.fillHeight: true

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: Theme.space7
                    anchors.leftMargin: Theme.space5
                    anchors.rightMargin: Theme.space4
                    spacing: 4

                    Repeater {
                        model: root.wizardSteps
                        delegate: Button {
                            required property int modelData
                            width: parent.width
                            height: 46
                            enabled: modelData <= root.currentStep
                                     || (modelData === 5 && root.authenticated)
                            hoverEnabled: true
                            onClicked: root.switchStep(modelData)

                            background: Superellipse {
                                fillColor: parent.hovered || modelData === root.currentStep
                                           ? Theme.surfaceHover : "transparent"
                                curveDepth: Theme.curveCompact
                            }

                            contentItem: RowLayout {
                                spacing: Theme.space3
                                layoutDirection: Theme.uiDirection

                                Icon {
                                    text: root.stepIcon(modelData)
                                    color: modelData < root.currentStep ? Theme.success
                                         : modelData === root.currentStep ? Theme.accent
                                         : Theme.inkSubtle
                                    preferredSize: 18
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.stepTitle(modelData)
                                    color: modelData === root.currentStep ? Theme.ink : Theme.inkMuted
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: modelData === root.currentStep
                                                 ? Theme.weightDemiBold : Theme.weightMedium
                                    elide: Text.ElideRight
                                }

                                Icon {
                                    visible: modelData < root.currentStep
                                    text: Theme.iconCheck
                                    color: Theme.success
                                    preferredSize: 15
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.borderMuted
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
                    contentHeight: Math.max(height, stageColumn.implicitHeight + Theme.space8)
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

                    Column {
                        id: stageColumn
                        width: Math.min(stageFlick.width - (root.phone ? Theme.space4 * 2 : Theme.space7 * 2), 820)
                        x: Math.max(root.phone ? Theme.space4 : Theme.space7,
                                    Math.round((stageFlick.width - width) / 2))
                        topPadding: root.phone ? Theme.space5 : Theme.space7
                        bottomPadding: Theme.space7

                        Loader {
                            width: parent.width
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
                    Layout.preferredHeight: root.phone ? 70 : 76

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
                            visible: root.currentStep !== 4
                            text: root.primaryActionText()
                            prominent: true
                            positive: root.currentStep === 5
                            enabled: root.primaryActionEnabled()
                            iconGlyph: root.currentStep === 5 ? Theme.iconCheck : ""
                            onClicked: root.nextStep()
                        }
                    }
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
                font.pixelSize: root.phone ? Theme.typeTitle1Size : Theme.typeDisplaySize
                font.weight: Theme.weightBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                Layout.maximumWidth: 620
                Layout.alignment: Qt.AlignHCenter
                text: qsTrId("genycaster.setup.language_orbit_hint")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeBodySize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item {
                id: orbitFrame
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? 300 : 340
                Layout.topMargin: Theme.space2

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(parent.width, 520)
                    height: 1
                    color: Theme.borderMuted
                    opacity: 0.65
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(parent.width, 400)
                    height: 94
                    radius: 47
                    color: Theme.primarySurface
                    border.width: 1
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                }

                PathView {
                    id: languageOrbit
                    anchors.fill: parent
                    model: root.greetings
                    pathItemCount: 7
                    preferredHighlightBegin: 0.5
                    preferredHighlightEnd: 0.5
                    highlightRangeMode: PathView.StrictlyEnforceRange
                    snapMode: PathView.SnapToItem
                    maximumFlickVelocity: 1100

                    path: Path {
                        startX: 24
                        startY: orbitFrame.height / 2
                        PathAttribute { name: "itemScale"; value: 0.62 }
                        PathAttribute { name: "itemOpacity"; value: 0.20 }
                        PathAttribute { name: "itemAngle"; value: -58 }
                        PathLine { x: orbitFrame.width / 2; y: orbitFrame.height / 2 }
                        PathAttribute { name: "itemScale"; value: 1.10 }
                        PathAttribute { name: "itemOpacity"; value: 1.0 }
                        PathAttribute { name: "itemAngle"; value: 0 }
                        PathLine { x: orbitFrame.width - 24; y: orbitFrame.height / 2 }
                        PathAttribute { name: "itemScale"; value: 0.62 }
                        PathAttribute { name: "itemOpacity"; value: 0.20 }
                        PathAttribute { name: "itemAngle"; value: 58 }
                    }

                    delegate: Item {
                        required property int index
                        required property var modelData
                        width: Math.min(220, languageOrbit.width * 0.46)
                        height: 88
                        scale: PathView.itemScale
                        opacity: PathView.itemOpacity
                        z: PathView.isCurrentItem ? 3 : 1
                        transform: Rotation {
                            origin.x: width / 2
                            origin.y: height / 2
                            axis.x: 0
                            axis.y: 1
                            axis.z: 0
                            angle: PathView.itemAngle
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.hello
                                color: PathView.isCurrentItem ? Theme.ink : Theme.inkMuted
                                font.family: Theme.fontFamilyForText(modelData.hello)
                                font.pixelSize: PathView.isCurrentItem ? 32 : 22
                                font.weight: PathView.isCurrentItem ? Theme.weightBold : Theme.weightMedium
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: PathView.isCurrentItem
                                text: modelData.language
                                color: Theme.accent
                                font.family: Theme.fontFamilyForText(modelData.language)
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
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
                        interval: 1750
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
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.space2
                Icon {
                    text: Theme.iconCheck
                    color: Theme.success
                    preferredSize: 16
                }
                Label {
                    text: localizationController.languageLabel
                    color: Theme.success
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.language_applies_now")
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }

    Component {
        id: personalizeStage

        ColumnLayout {
            spacing: Theme.space5

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Label {
                    text: qsTrId("genycaster.setup.welcome_personal_title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.personalize_body")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeBodySize
                    wrapMode: Text.WordWrap
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: localeColumn.implicitHeight + Theme.space5 * 2
                fillColor: Theme.surface
                borderWidth: 1
                borderColor: Theme.borderMuted
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: localeColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space5
                    spacing: Theme.space4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3
                        Icon { text: Theme.iconLanguage; color: Theme.accent; preferredSize: 20 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: qsTrId("genycaster.settings.language_region")
                                color: Theme.ink
                                font.pixelSize: Theme.typeTitle3Size
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
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: appearanceColumn.implicitHeight + Theme.space5 * 2
                fillColor: Theme.surface
                borderWidth: 1
                borderColor: Theme.borderMuted
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: appearanceColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space5
                    spacing: Theme.space4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3
                        Icon { text: Theme.iconPalette; color: Theme.accent; preferredSize: 20 }
                        Label {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.appearance")
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle3Size
                            font.weight: Theme.weightDemiBold
                        }
                        Rectangle {
                            width: 22
                            height: 22
                            radius: 11
                            color: Theme.accent
                            border.width: 1
                            border.color: Theme.borderStrong
                        }
                        Label {
                            text: root.accentCode()
                            color: Theme.inkMuted
                            font.family: Theme.codeFontFamily
                            font.pixelSize: Theme.typeCaptionSize
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
                        spacing: root.phone ? Theme.space2 : Theme.space3

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
                                Layout.maximumWidth: 64
                                implicitHeight: 48
                                Accessible.name: modelData.id
                                onClicked: appearanceController.setColorThemeId(modelData.id)

                                background: Superellipse {
                                    fillColor: Theme.surfaceMuted
                                    borderWidth: appearanceController.colorThemeId === modelData.id ? 2 : 1
                                    borderColor: appearanceController.colorThemeId === modelData.id
                                                 ? Theme.focus : Theme.borderMuted
                                    curveDepth: Theme.curveCompact
                                }

                                contentItem: Item {
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 24
                                        height: 24
                                        radius: 12
                                        color: modelData.color

                                        Icon {
                                            anchors.centerIn: parent
                                            visible: appearanceController.colorThemeId === modelData.id
                                            text: Theme.iconCheck
                                            color: Theme.contrastInk(modelData.color)
                                            preferredSize: 13
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

    Component {
        id: privacyStage

        ColumnLayout {
            spacing: Theme.space5

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Label {
                    text: qsTrId("genycaster.setup.legal_title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.privacy_compact_intro")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeBodySize
                    wrapMode: Text.WordWrap
                }
            }

            Repeater {
                model: [
                    {
                        title: qsTrId("genycaster.setup.terms_title"),
                        body: qsTrId("genycaster.setup.terms_summary"),
                        action: qsTrId("genycaster.setup.open_terms"),
                        icon: Theme.iconDocument,
                        privacy: false
                    },
                    {
                        title: qsTrId("genycaster.setup.privacy_title"),
                        body: qsTrId("genycaster.setup.privacy_summary"),
                        action: qsTrId("genycaster.setup.open_privacy"),
                        icon: Theme.iconInfo,
                        privacy: true
                    }
                ]

                delegate: Superellipse {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: legalCardColumn.implicitHeight + Theme.space4 * 2
                    fillColor: Theme.surface
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        id: legalCardColumn
                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        spacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            Icon {
                                text: modelData.icon
                                color: modelData.privacy ? Theme.info : Theme.warning
                                preferredSize: 20
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: Theme.ink
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightDemiBold
                            }
                            ActionButton {
                                compact: true
                                quiet: true
                                text: modelData.action
                                iconGlyph: Theme.iconArrowUpRight
                                onClicked: presentationBridge.openExternalUrl(
                                    modelData.privacy
                                    ? "https://genyleap.com/privacy"
                                    : "https://genyleap.com/terms")
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modelData.body
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeLabelSize
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Button {
                id: termsConsentButton
                Layout.fillWidth: true
                checkable: true
                checked: root.legalTermsChecked
                enabled: !root.legalConsentAccepted
                onToggled: root.legalTermsChecked = checked
                implicitHeight: 56

                background: Superellipse {
                    fillColor: parent.checked ? Theme.successSurface : Theme.surface
                    borderWidth: 1
                    borderColor: parent.checked ? Theme.success : Theme.borderMuted
                    curveDepth: Theme.curveControl
                }

                contentItem: RowLayout {
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection
                    Icon {
                        text: termsConsentButton.checked ? Theme.iconCheck : Theme.iconDocument
                        color: termsConsentButton.checked ? Theme.success : Theme.inkSubtle
                        preferredSize: 18
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.accept_terms")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Button {
                id: privacyConsentButton
                Layout.fillWidth: true
                checkable: true
                checked: root.legalPrivacyChecked
                enabled: !root.legalConsentAccepted
                onToggled: root.legalPrivacyChecked = checked
                implicitHeight: 56

                background: Superellipse {
                    fillColor: parent.checked ? Theme.successSurface : Theme.surface
                    borderWidth: 1
                    borderColor: parent.checked ? Theme.success : Theme.borderMuted
                    curveDepth: Theme.curveControl
                }

                contentItem: RowLayout {
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection
                    Icon {
                        text: privacyConsentButton.checked ? Theme.iconCheck : Theme.iconInfo
                        color: privacyConsentButton.checked ? Theme.success : Theme.inkSubtle
                        preferredSize: 18
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.acknowledge_privacy")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Icon {
                    text: root.legalConsentAccepted ? Theme.iconCheck : Theme.iconInfo
                    color: root.legalConsentAccepted ? Theme.success : Theme.info
                    preferredSize: 16
                }
                Label {
                    Layout.fillWidth: true
                    text: root.legalConsentAccepted
                          ? qsTrId("genycaster.setup.consent_recorded")
                          : qsTrId("genycaster.setup.privacy_local_note")
                    color: root.legalConsentAccepted ? Theme.success : Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component {
        id: networkStage

        ColumnLayout {
            spacing: Theme.space5

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                RowLayout {
                    spacing: Theme.space3
                    Icon { text: Theme.iconWifi; color: Theme.accent; preferredSize: 25 }
                    Label {
                        text: qsTrId("genycaster.setup.network_title")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle1Size
                        font.weight: Theme.weightBold
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.network_body")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeBodySize
                    wrapMode: Text.WordWrap
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: experimentalRow.implicitHeight + Theme.space3 * 2
                fillColor: Theme.warningSurface
                borderWidth: 1
                borderColor: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.35)
                curveDepth: Theme.curveControl

                RowLayout {
                    id: experimentalRow
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space3
                    Icon { text: Theme.iconWarning; color: Theme.warning; preferredSize: 18 }
                    Label {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.network_experimental")
                        color: Theme.ink
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    text: qsTrId("genycaster.setup.network_automatic")
                    iconGlyph: Theme.iconFidelity
                    selected: nodeSelectionController.automatic
                    onClicked: nodeSelectionController.useAutomaticSelection()
                }
                Item { Layout.fillWidth: true }
                ActionButton {
                    text: qsTrId("genycaster.setup.network_retest")
                    iconGlyph: Theme.iconRefresh
                    quiet: true
                    busy: nodeSelectionController.probing
                    onClicked: nodeSelectionController.refresh()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Repeater {
                    model: nodeSelectionController.nodes

                    delegate: Button {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 72
                        enabled: modelData.status !== "offline"
                        onClicked: nodeSelectionController.selectNode(modelData.id)

                        background: Superellipse {
                            fillColor: modelData.selected ? Theme.primarySurface
                                                       : parent.hovered ? Theme.surfaceHover : Theme.surface
                            borderWidth: modelData.selected ? 2 : 1
                            borderColor: modelData.selected ? Theme.focus : Theme.borderMuted
                            curveDepth: Theme.curveControl
                        }

                        contentItem: RowLayout {
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: "quilibrium/hypersnap"
                                color: modelData.online ? Theme.success
                                     : modelData.status === "offline" ? Theme.danger
                                     : Theme.inkMuted
                                preferredSize: 24
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
                                        font.weight: Theme.weightDemiBold
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
                                    font.weight: Theme.weightDemiBold
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
                                text: Theme.iconCheck
                                color: Theme.accent
                                preferredSize: 17
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Icon {
                    text: nodeSelectionController.restartRequired ? Theme.iconWarning : Theme.iconCheck
                    color: nodeSelectionController.restartRequired ? Theme.warning : Theme.success
                    preferredSize: 16
                }
                Label {
                    Layout.fillWidth: true
                    text: nodeSelectionController.restartRequired
                          ? qsTrId("genycaster.setup.network_restart_note")
                          : qsTrId("genycaster.setup.network_current_note")
                    color: nodeSelectionController.restartRequired ? Theme.warning : Theme.success
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component {
        id: accountStage

        ColumnLayout {
            spacing: Theme.space5

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Label {
                    text: root.authenticated
                          ? qsTrId("genycaster.setup.account_verified")
                          : qsTrId("genycaster.setup.sign_in_required")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                }
                Label {
                    Layout.fillWidth: true
                    text: root.phaseBody()
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeBodySize
                    wrapMode: Text.WordWrap
                }
            }

            Superellipse {
                visible: root.authenticated
                Layout.fillWidth: true
                implicitHeight: 108
                fillColor: Theme.successSurface
                borderWidth: 1
                borderColor: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.38)
                curveDepth: Theme.curveContainer

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space4
                    spacing: Theme.space4
                    layoutDirection: Theme.uiDirection

                    Avatar {
                        sourceUrl: root.accountAvatarUrl
                        displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                        avatarSize: 68
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle3Size
                            font.weight: Theme.weightBold
                            elide: Text.ElideRight
                        }
                        Label {
                            visible: root.accountUsername.length > 0
                            text: "@" + root.accountUsername
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeLabelSize
                        }
                        RowLayout {
                            spacing: Theme.space2
                            Icon { text: Theme.iconCheck; color: Theme.success; preferredSize: 14 }
                            Label {
                                text: qsTrId("genycaster.shell.verified_by_openproof")
                                color: Theme.success
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: !root.authenticated
                Layout.fillWidth: true
                spacing: Theme.space3

                Superellipse {
                    Layout.fillWidth: true
                    implicitHeight: genyleapColumn.implicitHeight + Theme.space4 * 2
                    fillColor: Theme.surface
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        id: genyleapColumn
                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        spacing: Theme.space3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            Icon { text: "brands/geny"; color: Theme.accent; preferredSize: 26 }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Label {
                                    text: "Genyleap · OpenProof"
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeTitle3Size
                                    font.weight: Theme.weightDemiBold
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.setup.account_genyleap_body")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.shell.continue_with_genyleap")
                            prominent: true
                            iconGlyph: Theme.iconSignIn
                            enabled: root.authenticationAvailable && !root.authenticationInProgress
                            busy: root.authenticationInProgress && root.authenticationRoute !== "farcaster"
                            onClicked: root.continueWithGenyleap()
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.account_create_openproof_soon")
                            iconGlyph: Theme.iconPlus
                            enabled: false
                        }
                    }
                }

                Superellipse {
                    Layout.fillWidth: true
                    implicitHeight: farcasterColumn.implicitHeight + Theme.space4 * 2
                    fillColor: Theme.surface
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        id: farcasterColumn
                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        spacing: Theme.space3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            Icon { text: "brands/farcaster"; color: Theme.accent; preferredSize: 26 }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Label {
                                    text: "Farcaster"
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeTitle3Size
                                    font.weight: Theme.weightDemiBold
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.setup.account_farcaster_body")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.shell.sign_in_with_farcaster")
                            iconGlyph: Theme.iconSignIn
                            enabled: root.authenticationAvailable && !root.authenticationInProgress
                            busy: root.authenticationInProgress && root.authenticationRoute === "farcaster"
                            onClicked: root.continueWithFarcaster()
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.account_create_farcaster_soon")
                            iconGlyph: Theme.iconPlus
                            enabled: false
                        }
                    }
                }

                ActionButton {
                    visible: root.authenticationApprovalAvailable
                    Layout.fillWidth: true
                    text: root.authenticationRoute === "farcaster"
                          ? qsTrId("genycaster.shell.open_farcaster")
                          : qsTrId("genycaster.shell.open_genyleap")
                    prominent: true
                    iconGlyph: Theme.iconArrowUpRight
                    onClicked: root.openApproval()
                }

                ActionButton {
                    visible: root.authenticationInProgress
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTrId("genycaster.shell.cancel")
                    quiet: true
                    onClicked: root.cancelAuthentication()
                }
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
                    Icon { text: Theme.iconWarning; color: Theme.danger; preferredSize: 18 }
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
                GenySpinner { running: true; color: Theme.accent }
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
            spacing: Theme.space5

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 120

                Avatar {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    sourceUrl: root.accountAvatarUrl
                    displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                    avatarSize: 92
                }

                Superellipse {
                    width: 28
                    height: 28
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 70
                    anchors.horizontalCenterOffset: 34
                    fillColor: Theme.success
                    curveDepth: 10
                    borderWidth: 3
                    borderColor: Theme.background
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconCheck
                        color: Theme.contrastInk(Theme.success)
                        preferredSize: 14
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.status.ready")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: root.accountName.length > 0
                          ? qsTrId("genycaster.setup.ready_for").arg(root.accountName)
                          : qsTrId("genycaster.setup.ready_generic")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeBodySize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Button {
                    Layout.fillWidth: true
                    implicitHeight: 60
                    onClicked: root.switchStep(0)
                    background: Superellipse {
                        fillColor: parent.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }
                    contentItem: RowLayout {
                        spacing: Theme.space3
                        Icon { text: Theme.iconLanguage; color: Theme.accent; preferredSize: 18 }
                        Label {
                            Layout.fillWidth: true
                            text: localizationController.languageLabel
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        Label {
                            text: localizationController.regionalLocaleLabel
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    implicitHeight: 60
                    onClicked: root.switchStep(1)
                    background: Superellipse {
                        fillColor: parent.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }
                    contentItem: RowLayout {
                        spacing: Theme.space3
                        Icon { text: Theme.iconPalette; color: Theme.accent; preferredSize: 18 }
                        Rectangle {
                            width: 24
                            height: 24
                            radius: 12
                            color: Theme.accent
                            border.width: 1
                            border.color: Theme.borderStrong
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.accentCode()
                            color: Theme.ink
                            font.family: Theme.codeFontFamily
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        Label {
                            text: appearanceController.themeMode
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    implicitHeight: 60
                    onClicked: root.switchStep(3)
                    background: Superellipse {
                        fillColor: parent.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }
                    contentItem: RowLayout {
                        spacing: Theme.space3
                        Icon { text: Theme.iconWifi; color: Theme.accent; preferredSize: 18 }
                        Label {
                            Layout.fillWidth: true
                            text: "Hypersnap v2"
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        Label {
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
                }

                Button {
                    Layout.fillWidth: true
                    implicitHeight: 60
                    onClicked: root.switchStep(4)
                    background: Superellipse {
                        fillColor: parent.hovered ? Theme.surfaceHover : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                    }
                    contentItem: RowLayout {
                        spacing: Theme.space3
                        Avatar {
                            sourceUrl: root.accountAvatarUrl
                            displayName: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            avatarSize: 34
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.accountName.length > 0 ? root.accountName : root.accountUsername
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }
                        Icon { text: Theme.iconCheck; color: Theme.success; preferredSize: 16 }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.space2
                Icon { text: Theme.iconCheck; color: Theme.success; preferredSize: 16 }
                Label {
                    text: qsTrId("genycaster.setup.consent_recorded")
                    color: Theme.success
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }
            }
        }
    }
}
