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
            border.width: brandInteraction.activeFocus ? 2 : 0
            border.color: Theme.accent
            // Pointer-driven volumetric G and arrow. The shader only
            // repaints when its theme, pointer or interaction changes.
            property real pointerX: 0.5
            property real pointerY: 0.5
            property bool pointerInside: false
            property real tapImpulse: 0
            // A complete 360° turn is available both automatically and by
            // pointer/touch drag. Rotation stops during manual interaction and
            // resumes gently; reduced-motion disables continuous animation.
            property real orbitYaw: 0
            property real dragYaw: 0
            property real dragPitch: -5
            property real viewZoom: 1.0
            property bool autoOrbitEnabled: true
            property bool dragging: false
            function resetView() {
                rotationCooldown.restart()
                orbitYaw = 0
                dragYaw = 0
                dragPitch = -5
                viewZoom = 1.0
                pointerX = 0.5
                pointerY = 0.5
                tapImpulse = 0
            }
            function changeZoom(steps) {
                viewZoom = Math.max(0.78, Math.min(1.40,
                                                viewZoom + steps * 0.10))
                rotationCooldown.restart()
            }
            Behavior on viewZoom {
                enabled: !Theme.reduceMotion
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            property real pressX: 0
            property real pressY: 0
            property bool movedByDrag: false
            readonly property bool motionAllowed: root.visible && visible && !Theme.reduceMotion
            Timer {
                id: rotationCooldown
                interval: 1600
                repeat: false
            }
            Timer {
                id: automaticRotation
                interval: 40
                repeat: true
                running: desktopBrandPane.motionAllowed
                         && desktopBrandPane.autoOrbitEnabled
                         && !desktopBrandPane.dragging
                         && !rotationCooldown.running
                onTriggered: {
                    // One unhurried revolution per minute at a bounded
                    // ~25 updates/second; WASM does not redraw off-screen.
                    desktopBrandPane.orbitYaw =
                            (desktopBrandPane.orbitYaw + 0.24) % 360
                }
            }
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.dark ? "#111622" : "#F7F9FC" }
                GradientStop { position: 0.56; color: Theme.dark ? "#1A2232" : "#EDF1F8" }
                GradientStop { position: 1.0; color: Theme.dark ? "#111824" : "#E6ECF6" }
            }
            // The canonical SVG becomes an oversized, angled brand canvas.
            // Roughly 40% extends beyond the left edge; the rest is softly
            // cropped by the pane, never scaled as a small standalone icon.
            clip: true
            // Keep this pane and its metadata above the full-screen
            // transparent Flickable, which is created later in setupCanvas.
            z: 2
            // GenyCaster's canonical G/forward-arrow is volumetrically
            // raymarched by Qt's GPU scene graph: one scene, one shader, no
            // DOM overlay or WebGL-only branch. This QSB contains portable
            // SPIR-V, GLES, HLSL and Metal code for native and WebAssembly.
            ShaderEffect {
                id: desktopBrandArt
                z: 0
                anchors.fill: parent
                property vector2d paneSize: Qt.vector2d(width, height)
                property vector2d pointer: Qt.vector2d(
                    desktopBrandPane.pointerX, desktopBrandPane.pointerY)
                property real darkMode: Theme.dark ? 1.0 : 0.0
                property real pulseValue: desktopBrandPane.tapImpulse
                property real orbitYaw: desktopBrandPane.orbitYaw
                property real dragYaw: desktopBrandPane.dragYaw
                property real dragPitch: desktopBrandPane.dragPitch
                property real viewZoom: desktopBrandPane.viewZoom
                // The atlas is derived from the EXACT paths in the official
                // genycaster-icon.svg, not circular/triangular approximations.
                // R/G preserve both SVG paths; B smooths their combined normal.
                property var logoDistanceMap: Image {
                    source: "qrc:/qt/qml/Farcaster/Client/shaders/genycaster_logo_sdf.png"
                    sourceSize.width: 1664
                    sourceSize.height: 1664
                    smooth: true
                    visible: false
                }
                property var logoNormalMap: Image {
                    source: "qrc:/qt/qml/Farcaster/Client/shaders/genycaster_logo_normals.png"
                    sourceSize.width: 1664
                    sourceSize.height: 1664
                    smooth: true
                    visible: false
                }
                fragmentShader: "qrc:/qt/qml/Farcaster/Client/shaders/setup_glass_g.frag.qsb"
                blending: false
            }

            // Edge progression is connected to the actual Setup stage.
            // It follows the selected accent in light and dark themes.
            Rectangle {
                z: 1
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 2
                color: Theme.dark ? "#35435C" : "#C8D2E3"
            }
            Rectangle {
                z: 2
                anchors.right: parent.right
                width: 2
                height: desktopBrandPane.height / root.progressCount
                y: height * root.currentStep
                color: Theme.accent
                Behavior on y {
                    enabled: !Theme.reduceMotion
                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                }
            }

            // The brand panel itself is an optional interactive surface:
            // hover = restrained parallax; click = one brief precision pulse.
            // The footer (z:3) and the setup navigation (z:20) remain on top.
            MouseArea {
                id: brandInteraction
                anchors.fill: parent
                z: 2
                hoverEnabled: true
                activeFocusOnTab: true
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                Keys.onReturnPressed: {
                    if (desktopBrandPane.motionAllowed) brandAccentTap.restart()
                }
                Keys.onSpacePressed: {
                    desktopBrandPane.autoOrbitEnabled = !desktopBrandPane.autoOrbitEnabled
                }
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_R || event.key === Qt.Key_Home) {
                        desktopBrandPane.resetView()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
                        desktopBrandPane.changeZoom(1)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Minus) {
                        desktopBrandPane.changeZoom(-1)
                        event.accepted = true
                    }
                }
                Keys.onLeftPressed: {
                    desktopBrandPane.dragYaw -= 12
                    rotationCooldown.restart()
                }
                Keys.onRightPressed: {
                    desktopBrandPane.dragYaw += 12
                    rotationCooldown.restart()
                }
                Keys.onUpPressed: {
                    desktopBrandPane.dragPitch =
                        Math.max(-65, desktopBrandPane.dragPitch - 9)
                    rotationCooldown.restart()
                }
                Keys.onDownPressed: {
                    desktopBrandPane.dragPitch =
                        Math.min(65, desktopBrandPane.dragPitch + 9)
                    rotationCooldown.restart()
                }
                Accessible.onPressAction: {
                    if (desktopBrandPane.motionAllowed) brandAccentTap.restart()
                }
                onEntered: desktopBrandPane.pointerInside = true
                onExited: {
                    desktopBrandPane.pointerInside = false
                    desktopBrandPane.pointerX = 0.5
                    desktopBrandPane.pointerY = 0.5
                }
                onPressed: function(mouse) {
                    desktopBrandPane.dragging = true
                    desktopBrandPane.movedByDrag = false
                    desktopBrandPane.pressX = mouse.x
                    desktopBrandPane.pressY = mouse.y
                    rotationCooldown.stop()
                }
                onPositionChanged: function(mouse) {
                    if (!root.visible) return
                    if (pressed) {
                        const dx = mouse.x - desktopBrandPane.pressX
                        const dy = mouse.y - desktopBrandPane.pressY
                        if (Math.abs(dx) + Math.abs(dy) > 2)
                            desktopBrandPane.movedByDrag = true
                        desktopBrandPane.dragYaw =
                            (desktopBrandPane.dragYaw + dx * 0.8) % 360
                        desktopBrandPane.dragPitch = Math.max(-65,
                            Math.min(65, desktopBrandPane.dragPitch + dy * 0.40))
                        desktopBrandPane.pressX = mouse.x
                        desktopBrandPane.pressY = mouse.y
                    } else {
                        desktopBrandPane.pointerX =
                            Math.max(0, Math.min(1, mouse.x / width))
                        desktopBrandPane.pointerY =
                            Math.max(0, Math.min(1, mouse.y / height))
                    }
                }
                onWheel: function(wheel) {
                    desktopBrandPane.changeZoom(wheel.angleDelta.y > 0 ? 1 : -1)
                    wheel.accepted = true
                }
                onReleased: {
                    desktopBrandPane.dragging = false
                    rotationCooldown.restart()
                }
                onCanceled: {
                    desktopBrandPane.dragging = false
                    rotationCooldown.restart()
                }
                onClicked: {
                    if (!desktopBrandPane.movedByDrag
                            && desktopBrandPane.motionAllowed)
                        brandAccentTap.restart()
                }
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("3D GenyCaster logo. Drag to orbit, scroll to zoom. Press R to reset.")
            }
            // Precise GPU demonstration controls, independent of Setup navigation.
            // The canonical path and signed-distance atlas are never modified
            // when the user orbits, zooms or resets the viewer.
            Rectangle {
                id: gpuStageControls
                z: 4
                anchors.bottom: desktopBrandMetadata.top
                anchors.bottomMargin: 86
                anchors.horizontalCenter: parent.horizontalCenter
                width: stageControlRow.width + 14
                height: 46
                radius: 14
                color: Theme.dark ? "#18293C" : "#F1F6FB"
                border.width: 1
                border.color: Theme.dark ? "#36536B" : "#C8D7E5"
                Row {
                    id: stageControlRow
                    anchors.centerIn: parent
                    spacing: 3
                    ActionButton {
                        objectName: "gpuResetView"
                        width: 35; height: 35
                        compact: true; quiet: true; iconOnly: true
                        iconSize: 17
                        iconGlyph: Theme.iconRefresh
                        text: qsTr("Reset 3D view")
                        onClicked: desktopBrandPane.resetView()
                    }
                    ActionButton {
                        objectName: "gpuAutoOrbit"
                        width: 35; height: 35
                        compact: true; quiet: true; iconOnly: true
                        iconSize: 16
                        iconGlyph: desktopBrandPane.autoOrbitEnabled
                                   ? Theme.iconPause : Theme.iconPlay
                        text: desktopBrandPane.autoOrbitEnabled
                              ? qsTr("Pause rotation") : qsTr("Resume rotation")
                        onClicked: {
                            desktopBrandPane.autoOrbitEnabled =
                                !desktopBrandPane.autoOrbitEnabled
                            rotationCooldown.stop()
                        }
                    }
                    ActionButton {
                        objectName: "gpuZoomOut"
                        width: 35; height: 35
                        compact: true; quiet: true; iconOnly: true
                        iconSize: 17
                        iconGlyph: "actions/minus"
                        text: qsTr("Zoom out")
                        enabled: desktopBrandPane.viewZoom > 0.781
                        onClicked: desktopBrandPane.changeZoom(-1)
                    }
                    ActionButton {
                        objectName: "gpuZoomIn"
                        width: 35; height: 35
                        compact: true; quiet: true; iconOnly: true
                        iconSize: 17
                        iconGlyph: Theme.iconPlus
                        text: qsTr("Zoom in")
                        enabled: desktopBrandPane.viewZoom < 1.399
                        onClicked: desktopBrandPane.changeZoom(1)
                    }
                }
            }
            Row {
                z: 4
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.topMargin: 35
                spacing: 7
                Rectangle {
                    width: 7
                    height: 7
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.dark ? "#82D5CD" : "#297D8D"
                }
                Text {
                    text: "LIVE GPU  /  WASM"
                    color: Theme.dark ? "#AFC9DD" : "#41677C"
                    font.family: Theme.latinFontFamily
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }
            }
            Text {
                z: 3
                anchors.bottom: gpuStageControls.top
                anchors.bottomMargin: 16
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("DRAG TO ORBIT   ·   SCROLL TO ZOOM")
                color: Theme.dark ? "#829CAF" : "#688094"
                font.family: Theme.latinFontFamily
                font.pixelSize: 10
                font.letterSpacing: .75
                font.weight: Font.Medium
            }
            SequentialAnimation {
                id: brandAccentTap
                running: false
                NumberAnimation {
                    target: desktopBrandPane
                    property: "tapImpulse"
                    from: 0
                    to: 1
                    duration: 160
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: desktopBrandPane
                    property: "tapImpulse"
                    from: 1
                    to: 0
                    duration: 540
                    easing.type: Easing.OutCubic
                }
            }

            // Product metadata is pinned inside the desktop left rail,
            // independent of the scrolling form and bottom-right CTA.
            Column {
                id: desktopBrandMetadata
                z: 3
                visible: desktopBrandPane.visible
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 32
                anchors.rightMargin: 24
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 28
                spacing: 7

                Rectangle {
                    width: 36
                    height: 1
                    color: Theme.dark ? "#6E718D" : "#A0A0C4"
                    opacity: 0.75
                }
                // The company owns the copyright. Keep the link confined to
                // the sidebar, keyboard-accessible and visually subtle.
                Item {
                    id: genyleapCopyrightLink
                    width: parent.width
                    height: 28
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Link
                    Accessible.name: copyrightLabel.text
                    Accessible.onPressAction: Qt.openUrlExternally("https://genyleap.com/")
                    Keys.onReturnPressed: Qt.openUrlExternally("https://genyleap.com/")
                    Keys.onEnterPressed: Qt.openUrlExternally("https://genyleap.com/")
                    Keys.onSpacePressed: Qt.openUrlExternally("https://genyleap.com/")

                    Text {
                        id: copyrightLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "© " + new Date().getFullYear() + " Genyleap"
                        color: Theme.dark ? "#E2E3F2" : "#4B4E6C"
                        font.family: Theme.latinFontFamily
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        font.underline: genyleapCopyrightMouse.containsMouse || genyleapCopyrightLink.activeFocus
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: genyleapCopyrightMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://genyleap.com/")
                    }
                }
                Text {
                    width: parent.width
                    text: "Beta 1.0  ·  v" + (Qt.application.version || "1.0.0")
                    color: Theme.dark ? "#ACB0CA" : "#6B6E8D"
                    font.family: Theme.latinFontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        // The exact same eight-stage indicator is used in Welcome.
        SetupStepIndicator {
            id: setupProgress
            visible: !root.phone && root.currentStep !== 0
            stepCount: root.progressCount
            currentStep: root.currentStep
            anchors.top: parent.top
            anchors.topMargin: 44
            x: desktopBrandPane.visible
               ? desktopBrandPane.width + (parent.width - desktopBrandPane.width - width) / 2
               : (parent.width - width) / 2
            z: 5
            Accessible.name: root.stepTitle(root.currentStep)
                             + " (" + (root.currentStep + 1) + "/" + root.progressCount + ")"
        }

        ActionButton {
            z: 20
            visible: root.currentStep > 0
            anchors.top: parent.top
            anchors.topMargin: Theme.space4
            // Navigation belongs to the content column, not the GPU art.
            // The opposite-side Review/Close action obeys the same inset.
            x: Theme.rtl ? root.width - width - (root.phone ? Theme.space2 : Theme.space5)
                         : (desktopBrandPane.visible ? desktopBrandPane.width + Theme.space5
                                                    : (root.phone ? Theme.space2 : Theme.space5))
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
            x: Theme.rtl ? (desktopBrandPane.visible ? desktopBrandPane.width + Theme.space5
                                                      : (root.phone ? Theme.space2 : Theme.space5))
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
                Layout.alignment: desktopBrandPane.visible ? Qt.AlignLeft
                                : root.phone ? (Theme.rtl ? Qt.AlignRight : Qt.AlignLeft)
                                : Qt.AlignHCenter
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
                horizontalAlignment: root.phone || desktopBrandPane.visible ? Theme.textStartAlignment : Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.setup.design_welcome_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                lineHeight: 1.36
                wrapMode: Text.WordWrap
                horizontalAlignment: root.phone || desktopBrandPane.visible ? Theme.textStartAlignment : Text.AlignHCenter
            }
            // Welcome and all later pages share marker geometry and count.
            SetupStepIndicator {
                Layout.alignment: root.phone ? Qt.AlignHCenter
                                  : desktopBrandPane.visible ? Qt.AlignLeft : Qt.AlignHCenter
                Layout.topMargin: root.phone ? Theme.space6 : Theme.space3
                stepCount: root.progressCount
                currentStep: root.currentStep
                Accessible.name: root.stepTitle(root.currentStep)
                                 + " (" + (root.currentStep + 1) + "/" + root.progressCount + ")"
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
                    objectName: "setupCreateAccountUnavailable"
                    accountShowcase: true
                    unavailable: true
                    title: qsTrId("genycaster.setup.design_create_account")
                    subtitle: qsTrId("genycaster.setup.new_profile_body")
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
            // Local custody recovery is the preferred existing-account route.
            // Keep it first, with clear visual emphasis; the two external
            // identity methods are secondary alternatives.
            SetupActionCard {
                visible: !root.authenticated
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                setupEmphasis: true
                iconGlyph: "system/lock"
                title: qsTrId("genycaster.setup.design_recovery_title")
                enabled: !root.authenticationInProgress && !root.recoverySubmitPending
                onClicked: {
                    if (root.browserReadOnly) {
                        // The DOM recovery dialog is presented only on an
                        // explicit click. No phrase enters QML or C++.
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
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.native_recovery_placeholder")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                Item {
                    id: nativeRecoveryGrid
                    objectName: "nativeRecoveryWordGrid"
                    Layout.fillWidth: true
                    implicitHeight: recoveryWordLayout.implicitHeight
                    property bool distributing: false
                    // Qt cannot track dynamically indexed Repeater children inside
                    // a function binding. Publish the count on every field change.
                    property int completeWordCount: 0
                    function refreshCompletion() {
                        completeWordCount = countWords()
                    }
                    function clearAll() {
                        for (let i = 0; i < 24; ++i) {
                            const field = recoveryWordRepeater.itemAt(i)
                            if (field) field.clear()
                        }
                        completeWordCount = 0
                        revealWords.checked = false
                    }
                    function countWords() {
                        let result = 0
                        for (let i = 0; i < 24; ++i) {
                            const field = recoveryWordRepeater.itemAt(i)
                            if (field && field.text.trim().length > 0) ++result
                        }
                        return result
                    }
                    function phrase() {
                        let result = []
                        for (let i = 0; i < 24; ++i) {
                            const field = recoveryWordRepeater.itemAt(i)
                            if (field) result.push(field.text.trim().toLowerCase())
                        }
                        const text = result.join(" ")
                        result.fill("")
                        return text
                    }
                    function distribute(raw, offset) {
                        const words = String(raw).trim().toLowerCase().split(/\s+/).filter(Boolean)
                        if (words.length === 0) return
                        distributing = true
                        const start = words.length === 24 ? 0 : offset
                        for (let i = 0; i < words.length && start + i < 24; ++i) {
                            const field = recoveryWordRepeater.itemAt(start + i)
                            if (field) field.text = words[i].replace(/[^a-z]/g, "")
                        }
                        const pastedCount = words.length
                        words.fill("")
                        distributing = false
                        refreshCompletion()
                        if (start === 0 && completeWordCount === 24) revealWords.forceActiveFocus()
                        else recoveryWordRepeater.itemAt(Math.min(23, start + pastedCount))?.forceActiveFocus()
                    }
                    onVisibleChanged: if (!visible) clearAll()
                    GridLayout {
                        id: recoveryWordLayout
                        width: parent.width
                        columns: root.phone ? 2 : 4
                        columnSpacing: Theme.space2
                        rowSpacing: Theme.space2
                        layoutDirection: Qt.LeftToRight
                        Repeater {
                            id: recoveryWordRepeater
                            model: 24
                            delegate: TextField {
                                id: wordField
                                required property int index
                                objectName: "nativeRecoveryWord" + (index + 1)
                                Layout.fillWidth: true
                                Layout.minimumWidth: root.phone ? 100 : 112
                                implicitHeight: 48
                                maximumLength: 4095
                                echoMode: revealWords.checked ? TextInput.Normal : TextInput.Password
                                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                                enabled: !root.authenticationInProgress && !root.recoverySubmitPending
                                horizontalAlignment: TextInput.AlignLeft
                                color: Theme.ink
                                font.pixelSize: 15
                                leftPadding: Theme.space3
                                rightPadding: Theme.space2
                                placeholderText: String(index + 1).padStart(2, "0")
                                Accessible.name: qsTrId("genycaster.setup.native_recovery_placeholder")
                                                 + " " + (index + 1)
                                background: Superellipse {
                                    fillColor: Theme.surface
                                    borderWidth: wordField.activeFocus ? 2 : Theme.strokeStandard
                                    borderColor: wordField.activeFocus ? Theme.focus : Theme.borderMuted
                                    curveDepth: Theme.curveControl
                                }
                                onTextChanged: nativeRecoveryGrid.refreshCompletion()
                                onTextEdited: {
                                    if (nativeRecoveryGrid.distributing) return
                                    if (/\s/.test(text))
                                        nativeRecoveryGrid.distribute(text, index)
                                    else
                                        text = text.toLowerCase().replace(/[^a-z]/g, "")
                                }
                                Keys.onPressed: function(event) {
                                    if ((event.key === Qt.Key_Space || event.key === Qt.Key_Return)
                                        && index < 23) {
                                        recoveryWordRepeater.itemAt(index + 1)?.forceActiveFocus()
                                        event.accepted = true
                                    } else if (event.key === Qt.Key_Backspace && !text && index > 0) {
                                        recoveryWordRepeater.itemAt(index - 1)?.forceActiveFocus()
                                    }
                                }
                            }
                        }
                    }
                }
                Button {
                    id: revealWords
                    objectName: "nativeRecoveryRevealWords"
                    checkable: true
                    checked: false
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                    implicitWidth: revealContent.implicitWidth + Theme.space2
                    implicitHeight: 44
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: qsTrId("genycaster.setup.show_recovery_words")
                    Accessible.checked: checked
                    onVisibleChanged: if (!visible) checked = false
                    background: Item {}
                    contentItem: RowLayout {
                        id: revealContent
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        Superellipse {
                            Layout.preferredWidth: 23
                            Layout.preferredHeight: 23
                            fillColor: revealWords.checked ? Theme.ink : Theme.surface
                            borderColor: revealWords.activeFocus ? Theme.focus : Theme.borderMuted
                            borderWidth: revealWords.activeFocus ? 2 : 1
                            curveDepth: 6
                            Icon {
                                anchors.centerIn: parent
                                visible: revealWords.checked
                                text: Theme.iconCheck
                                color: Theme.surface
                                preferredSize: 15
                            }
                        }
                        DirectionAwareLabel {
                            text: qsTrId("genycaster.setup.show_recovery_words")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.ink
                        }
                    }
                }
                SetupPrimaryButton {
                    Layout.fillWidth: true
                    neutralPrimary: true
                    text: qsTrId("genycaster.setup.recovery_button")
                    enabled: root.runtimeReady && !root.authenticationInProgress
                          && !root.recoverySubmitPending
                          && nativeRecoveryGrid.completeWordCount === 24
                    onClicked: {
                        let words = nativeRecoveryGrid.phrase()
                        nativeRecoveryGrid.clearAll()
                        root.recoverySubmitPending = true
                        root.nativeRecoverAccount(words)
                        root.switchStep(5)
                        words = ""
                    }
                }
            }
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
                    id: completionBadge
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
