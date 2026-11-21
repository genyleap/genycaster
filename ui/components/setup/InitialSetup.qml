pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes
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
    // A named active route prevents the OpenProof and Farcaster sign-in
    // options from displaying a shared, misleading busy state.
    readonly property string activeSignInRoute: root.authenticationInProgress
        ? (root.authenticationRoute === "farcaster" ? "farcaster"
           : root.authenticationRoute === "openproof" ? "openproof" : "") : ""
    property bool authenticationApprovalAvailable: false
    property string authenticationErrorText: ""
    property var setupConnections: ({})
    property string signerStatusText: ""
    property string signerErrorText: ""
    property bool signerBusy: false
    property bool signerReady: false
    property string accountName: ""
    property string accountUsername: ""
    property string accountAvatarUrl: ""
    property string accountIdentityIssuer: ""

    property int currentStep: 0
    property int highestReachedStep: 0
    property bool stageTransitioning: false
    property real stageOpacity: 1.0
    property real stageOffset: 0.0
    property int pendingStep: -1
    readonly property bool showsPrimaryCta: [0,1,2,6,7].includes(currentStep)
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
    // Both desktop and WASM use the same intent -> protocol -> action state machine.
    property bool existingProtocolSelected: false
    property bool choosingRegistrationProvider: false
    property bool resumingRegistration: false
    property string registrationProviderId: "hypersnap-v2"
    readonly property bool nativeFidRegistered:
        registrationProviderId === "hypersnap-v2"
        && nativeRegistrationPublisher.confirmed
    readonly property bool farcasterFidRegistered:
        registrationProviderId === "farcaster-snapchain"
        && neynarRegistration.confirmed
    readonly property bool accountFidRegistered:
        nativeFidRegistered || farcasterFidRegistered
    readonly property bool showsSetupConnectivity: visible && !reviewMode && !authenticated
        && ((currentStep === 3 && choosingRegistrationProvider && !accountFidRegistered)
            || (currentStep === 4 && !resumingRegistration))
    onShowsSetupConnectivityChanged: {
        if (showsSetupConnectivity && runtimeReady)
            checkSetupConnections()
    }
    onRuntimeReadyChanged: {
        if (runtimeReady && showsSetupConnectivity)
            checkSetupConnections()
    }
    property string registrationError: ""
    property bool confirmWalletReplacement: false
    // Periodic read-only health refresh. No proof or submit runs from this.
    Timer {
        id: nativeRegistrationNetworkHealthTimer
        interval: 30000
        repeat: true
        running: root.visible && root.currentStep === 3
                 && root.choosingRegistrationProvider
                 && root.registrationProviderId === "hypersnap-v2"
                 && !root.browserReadOnly
        onTriggered: {
            if (!nativeRegistrationStatus.checking
                && !root.nativeCreationRequested
                && !nativeRegistrationPreparation.busy
                && !nativeRegistrationPublisher.busy)
                nativeRegistrationStatus.refresh()
        }
    }
    property bool nativeCreationRequested: false
    property bool nativeDetailsExpanded: false
    property bool nativeExistingResumeRequested: false
    property bool recoveryWordsCopied: false
    Timer {
        id: recoveryCopyFeedbackTimer
        interval: 4000
        repeat: false
        onTriggered: root.recoveryWordsCopied = false
    }
    function beginAccountRegistration() {
        if (registrationProviderId === "hypersnap-v2")
            nativeRegistrationStatus.refresh()
        registrationError = ""
    }
    // The authoritative read is safe to perform automatically after wallet
    // backup. Users should not need to discover an extra lookup button.
    function maybeLookupNativeFid() {
        if (registrationProviderId !== "hypersnap-v2"
            || !nativeCustodyWallet.backupVerified || !nativeRegistrationStatus.active
            || nativeRegistrationStatus.accountLookupChecked
            || nativeRegistrationStatus.accountLookupBusy
            || nativeRegistrationPublisher.pending
            || nativeRegistrationPublisher.confirmed) return
        nativeRegistrationStatus.findExistingNativeAccount(nativeCustodyWallet.address)
    }
    // One explicit Create Account click owns the entire proof + submit
    // operation. Network checks are read-only until that click, and no proof
    // completion can submit anything without this in-memory consent token.
    function resumeNativePendingChecks() {
        if (visible && currentStep === 3 && choosingRegistrationProvider
            && registrationProviderId === "hypersnap-v2"
            && nativeCustodyWallet.backupVerified
            && nativeRegistrationPublisher.pending
            && !nativeRegistrationPublisher.busy
            && !nativeRegistrationPublisher.checkingConfirmation)
            nativeRegistrationPublisher.checkConfirmation()
    }
    onCurrentStepChanged: Qt.callLater(root.resumeNativePendingChecks)
    onChoosingRegistrationProviderChanged: Qt.callLater(root.resumeNativePendingChecks)
    function startNativeAccountCreation() {
        if (nativeCreationRequested || currentStep !== 3
            || !choosingRegistrationProvider || registrationProviderId !== "hypersnap-v2"
            || !nativeCustodyWallet.backupVerified || !registrationNodeUsable
            || nativeRegistrationStatus.accountLookupBusy
            || !nativeRegistrationStatus.accountLookupChecked
            || nativeRegistrationStatus.accountLookupFailed
            || nativeRegistrationPublisher.pending || nativeRegistrationPublisher.busy
            || nativeRegistrationPublisher.confirmed) return
        if (nativeRegistrationStatus.existingNativeFid.length > 0) {
            resumeExistingNativeAccount()
            return
        }
        registrationError = ""
        nativeCreationRequested = true
        if (nativeRegistrationPreparation.prepared) {
            Qt.callLater(root.submitPreparedNativeAccount)
        } else if (!nativeRegistrationPreparation.prepare()) {
            nativeCreationRequested = false
            registrationError = qsTrId("genycaster.setup.native_proof_failed")
        }
    }
    function submitPreparedNativeAccount() {
        if (!nativeCreationRequested || !nativeRegistrationPreparation.prepared)
            return
        // The original click is no longer reusable: a retry always needs
        // another explicit user action and independent custody/FID preflight.
        nativeCreationRequested = false
        if (!nativeCustodyWallet.backupVerified
            || registrationProviderId !== "hypersnap-v2"
            || nativeRegistrationPublisher.pending || nativeRegistrationPublisher.confirmed
            || !nativeRegistrationPublisher.submitPrepared()) {
            registrationError = qsTrId("genycaster.setup.native_proof_failed")
            nativeRegistrationStatus.refresh()
        }
    }
    function resumeExistingNativeAccount(explicitRetry) {
        if (nativeExistingResumeRequested || !nativeCustodyWallet.backupVerified
            || registrationProviderId !== "hypersnap-v2"
            || nativeRegistrationPublisher.pending || nativeRegistrationPublisher.busy
            || nativeRegistrationPublisher.confirmed || authenticated
            || nativeRegistrationStatus.existingNativeFid.length === 0
            || (nativeRegistrationPublisher.existingVerificationFailed
                && !explicitRetry))
            return
        nativeExistingResumeRequested = true
        if (!nativeRegistrationPublisher.confirmExistingAccount())
            nativeExistingResumeRequested = false
    }
    Connections {
        target: nativeCustodyWallet
        function onChanged() { Qt.callLater(root.maybeLookupNativeFid) }
    }
    Connections {
        target: nativeRegistrationStatus
        function onChanged() {
            // The readiness check and wallet-backup verification can finish
            // in either order. Begin the custody-bound read when BOTH exist.
            Qt.callLater(root.maybeLookupNativeFid)
            Qt.callLater(root.resumeNativePendingChecks)
        }
        function onLookupChanged() {
            // Already registered on shard-0? Accept the custody-bound FID
            // automatically instead of offering a second Continue button.
            if (root.currentStep === 3 && root.choosingRegistrationProvider
                && root.registrationProviderId === "hypersnap-v2"
                && nativeRegistrationStatus.existingNativeFid.length > 0)
                Qt.callLater(root.resumeExistingNativeAccount)
        }
    }
    Connections {
        target: nativeRegistrationPreparation
        function onChanged() {
            if (!root.nativeCreationRequested) return
            if (nativeRegistrationPreparation.prepared)
                Qt.callLater(root.submitPreparedNativeAccount)
            else if (nativeRegistrationPreparation.failed
                     && !nativeRegistrationPreparation.busy) {
                root.nativeCreationRequested = false
                root.registrationError = qsTrId("genycaster.setup.native_proof_failed")
                // A stale anchor is common after lengthy PoW. A fresh
                // challenge read is safe, but a second submission is not.
                nativeRegistrationStatus.refresh()
            }
        }
    }
    // Readiness is scoped to the selected registration registry. Neither a
    // reachable read node nor a generated custody wallet establishes an FID.
    // Keep this status advisory; irreversible writes retain their C++ gates.
    function registrationReadinessText() {
        if (registrationProviderId === "hypersnap-v2") {
            if (nativeRegistrationStatus.checking)
                return qsTrId("genycaster.setup.native_registration_checking")
            if (!nativeRegistrationStatus.active)
                return nativeRegistrationStatus.error.length > 0
                    ? nativeRegistrationStatus.error
                    : qsTrId("genycaster.setup.native_registration_node_unavailable")
            if (!runtimeReady)
                return qsTrId("genycaster.setup.registration_runtime_initializing")
            if (!nativeRegistrationPublisher.registrationSupported)
                return qsTrId("genycaster.setup.native_registration_signing_unavailable")
            if (nativeRegistrationStatus.shardZeroBlockDelay > 120)
                return qsTr("HyperSnap is catching up. Account creation is temporarily paused.")
            if (!nativeRegistrationStatus.syncedForRegistration)
                return qsTr("Checking the HyperSnap network…")
            return qsTrId("genycaster.setup.native_registration_node_ready")
        }
        if (!runtimeReady)
            return qsTrId("genycaster.setup.registration_runtime_initializing")
        if (!neynarRegistration.registrationSupported)
            return qsTrId("genycaster.setup.neynar_runtime_unavailable")
        if (!neynarRegistration.available)
            return qsTrId("genycaster.setup.registration_provider_missing")
        return qsTrId("genycaster.setup.neynar_registration_requirements")
    }
    function nativeSubmissionStatusText() {
        const state = nativeRegistrationPublisher.confirmed
            ? qsTrId("genycaster.setup.native_registration_confirmed")
                .arg(nativeRegistrationPublisher.registeredFid)
            : nativeRegistrationPublisher.failed
              ? qsTrId("genycaster.setup.native_registration_rejected")
              : nativeRegistrationPublisher.outcomeUnknown
                ? qsTrId("genycaster.setup.native_registration_outcome_unknown")
                : qsTrId("genycaster.setup.native_registration_submitted")
        const httpStatus = nativeRegistrationPublisher.lastSubmissionHttpStatus
        // The node's free-form error response is never displayed; only its
        // bounded numeric HTTP status is included for safe diagnostics.
        return (nativeRegistrationPublisher.failed
                || nativeRegistrationPublisher.outcomeUnknown) && httpStatus > 0
            ? state + " (HTTP " + httpStatus + ")"
            : state
    }
    readonly property bool registrationNodeUsable:
        registrationProviderId === "hypersnap-v2"
        && nativeRegistrationStatus.syncedForRegistration && runtimeReady
        && nativeRegistrationPublisher.registrationSupported
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
    property bool consentChecked: false
    readonly property bool phone: Qt.platform.os === "android"
                                  || Qt.platform.os === "ios" || width < 680
    readonly property int setupGutter: phone ? 24 : Theme.scaleUi(32)
    // A compact secondary choice accompanies Get Started in the Web welcome
    // step. Do not repeat download promotions during custody or recovery.
    readonly property bool welcomeNativeAppsVisible: Qt.platform.os === "wasm"
        && currentStep === 0 && !authenticated && !reviewMode
    readonly property real welcomeActionsWidth: Math.max(0, Math.min(
        root.phone ? root.width - root.setupGutter * 2 : stagePanel.width,
        root.phone ? 440 : 560))
    // Stage buttons are adjacent to the fields they advance, not fixed to the window.
    readonly property bool desktopStageFooter: false
    readonly property bool wideStage: !phone && stagePanel.width >=
        Theme.scaleUi(520) * Math.max(1, Theme.fontScale)
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
    signal checkSetupConnections()
    signal openApproval()
    signal cancelAuthentication()
    signal authorizeSigner()
    signal nativeAppDownloadsRequested()
    signal recoverAccount(string protocol)
    signal browserCreateAccount(string protocol)
    signal nativeRecoverAccount(string recoveryWords)
    signal nativeRecoverFarcasterAccount(string recoveryWords, bool authorizeSigner)
    signal nativeRecoverHyperSnapAccount(string recoveryWords, bool authorizeSigner)
    // No implicit protocol selection: the user chooses after New/Existing.
    // A browser signer must advertise actual support before sensitive input.
    property string recoveryProtocol: ""
    signal acceptLegalConsent(bool termsAccepted, bool privacyAcknowledged, bool reviewCompleted)
    signal finishSetup()
    signal closeReviewRequested()

    Accessible.name: qsTrId("genycaster.setup.title")

    function setupConnectionStatusText(result, compact) {
        switch (result.state) {
        case "reachable":
            const elapsed = LocaleFormat.localizeDigits(result.elapsedMs)
            return compact ? qsTrId("genycaster.setup.connection_latency").arg(elapsed)
                           : qsTrId("genycaster.setup.connection_response_time").arg(elapsed)
        case "unavailable": return qsTrId("genycaster.setup.temporarily_unavailable")
        case "disabled": return qsTrId("genycaster.setup.connection_disabled")
        case "checking": return qsTrId("genycaster.settings.checking")
        default: return qsTrId("genycaster.nodes.not_tested")
        }
    }

    readonly property bool checkingSetupConnections:
        Object.values(setupConnections).some(result => result.state === "checking")

    component SetupConnectionRefresh: ActionButton {
        objectName: "setupCheckConnections"
        compact: true
        quiet: true
        iconOnly: true
        iconGlyph: Theme.iconRefresh
        busy: visible && root.checkingSetupConnections && !Theme.reduceMotion && !Theme.powerSave
        spinIconWhileBusy: true
        text: root.checkingSetupConnections ? qsTrId("genycaster.settings.checking")
                                           : qsTrId("genycaster.settings.check_now")
        enabled: root.runtimeReady && !root.checkingSetupConnections
        onClicked: root.checkSetupConnections()
    }

    function protocolConnectionState(farcaster) {
        const states = [farcaster ? setupConnections.farcasterRecovery : setupConnections.hypersnapRecovery]
        if (farcaster && setupConnections.farcasterSignIn?.state !== "not-applicable")
            states.push(setupConnections.farcasterSignIn)
        for (const state of ["unavailable", "disabled", "checking"]) {
            if (states.some(result => result?.state === state))
                return state
        }
        return states.every(result => result?.state === "reachable") ? "reachable" : "idle"
    }

    function protocolConnectionText(farcaster, state = protocolConnectionState(farcaster)) {
        if (farcaster && (state === "unavailable" || state === "disabled")) {
            const recovery = setupConnections.farcasterRecovery?.state === state
            const signIn = setupConnections.farcasterSignIn?.state === state
            if (recovery !== signIn)
                return (recovery ? qsTrId("genycaster.setup.connection_recovery")
                                 : qsTrId("genycaster.setup.connection_app_sign_in"))
                       + " · " + setupConnectionStatusText({state}, false)
        }
        return state === "reachable" ? qsTrId("genycaster.status.reachable")
             : setupConnectionStatusText({state}, false)
    }

    component SetupConnectionPanel: ColumnLayout {
        id: details
        property string protocol: ""
        property bool expanded: false
        onVisibleChanged: if (!visible) expanded = false
        onProtocolChanged: expanded = false
        spacing: Theme.space1
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            ActionButton {
                compact: true
                quiet: true
                iconGlyph: details.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                text: qsTrId("genycaster.setup.connection_status_title")
                Accessible.expandable: true
                Accessible.expanded: details.expanded
                onClicked: details.expanded = !details.expanded
            }
            Item { Layout.fillWidth: true }
            SetupConnectionRefresh { visible: details.expanded }
        }
        Repeater {
            model: details.expanded ? [
                {protocol: "farcaster-snapchain", label: qsTrId("genycaster.settings.farcaster_label") + " · " + qsTrId("genycaster.setup.connection_recovery"), result: root.setupConnections.farcasterRecovery},
                {protocol: "farcaster-snapchain", label: qsTrId("genycaster.settings.farcaster_label") + " · " + qsTrId("genycaster.setup.connection_app_sign_in"), result: root.setupConnections.farcasterSignIn},
                {protocol: "hypersnap-v2", label: qsTrId("genycaster.settings.hypersnap_label") + " · " + qsTrId("genycaster.setup.connection_recovery"), result: root.setupConnections.hypersnapRecovery}
            ] : []
            delegate: RowLayout {
                id: route
                required property var modelData
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                visible: (!details.protocol || details.protocol === modelData.protocol)
                         && modelData.result?.state !== "not-applicable"
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: route.modelData.label
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                    alignToUi: true
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.maximumWidth: details.width * 0.45
                    text: root.setupConnectionStatusText(route.modelData.result || ({}), true)
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }
    }

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
        if (!reviewMode && !authenticated && target > 5)
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
        // Never advance while a recovered device signer or custody grant
        // is being authorized. This is the same guard as the visible CTA,
        // not merely a disabled button that keyboard shortcuts can bypass.
        if ((currentStep === 5 || currentStep === 7)
                && (recoverySubmitPending || authenticationInProgress))
            return
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
            if (accountIntent === "existing")
                switchStep(4)
            return
        }
        if (currentStep === 4) {
            if (authenticated) switchStep(5)
            return
        }
        if (currentStep === 5) {
            if (reviewMode || authenticated) switchStep(6)
            return
        }
        if (currentStep === 6) { switchStep(7); return }
        if (currentStep === 7) {
            if (reviewMode) closeReviewRequested()
            else if (legalConsentAccepted && authenticated) finishSetup()
        }
    }
    function previousStep() {
        if (currentStep === 3 && choosingRegistrationProvider) {
            if (neynarRegistration.pending
                || neynarRegistration.phase === "reserved") {
                registrationError =
                    qsTrId("genycaster.setup.neynar_waiting_confirmation")
                neynarRegistration.checkConfirmation()
                return
            }
            // Back can stop local proof preparation, but never the already
            // submitted network write. Keep its read-only confirmation live.
            if (!nativeRegistrationPublisher.pending) {
                nativeRegistrationPublisher.clear()
                neynarRegistration.reset()
            }
            root.nativeCreationRequested = false
            nativeRegistrationPreparation.cancel()
            nativeRegistrationStatus.clearExistingLookup()
            // Back is a navigation action, never a wallet replacement.
            confirmWalletReplacement = false
            choosingRegistrationProvider = false
            registrationError = ""
            accountIntent = ""
            // The network picker is a nested view of step 3, not Welcome.
            stageScroll.contentY = 0
            return
        }
        if (currentStep === 0) {
            if (reviewMode) closeReviewRequested()
            return
        }
        if (currentStep === 4 && resumingRegistration) {
            resumingRegistration = false
            showRecovery = false
            switchStep(3)
            return
        }
        if (currentStep === 4 && showRecovery) {
            showRecovery = false
            stageScroll.contentY = 0
            return
        }
        if (currentStep === 4 && existingProtocolSelected) {
            existingProtocolSelected = false
            recoveryProtocol = ""
            stageScroll.contentY = 0
            return
        }
        if (currentStep === 5 && (authenticationInProgress || recoverySubmitPending)) {
            // Leave the form editable even if the cancelled network worker
            // finishes after Setup has returned to its recovery page.
            recoverySubmitPending = false
            cancelAuthentication()
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
        if ((currentStep === 5 || currentStep === 7)
                && (recoverySubmitPending || authenticationInProgress))
            return false
        if (currentStep === 2) return consentChecked
        if (currentStep === 3) return accountIntent === "existing"

        if (currentStep === 4) return authenticated
        if (currentStep === 5) return reviewMode || authenticated
        if (currentStep === 7) return reviewMode || (legalConsentAccepted
            && authenticated)
        return true
    }
    onVisibleChanged: {
        if (!visible) {
            // Closing onboarding stops polling; it cannot cancel a blockchain write.
            root.nativeCreationRequested = false
            nativeRegistrationPublisher.clear()
            nativeRegistrationPreparation.cancel()
            nativeRegistrationStatus.clearExistingLookup()
            // A paid FID reservation or irreversible network write remains
            // bound to the same backed-up custody wallet.
            neynarRegistration.stopPolling()
            if (!nativeRegistrationPublisher.pending
                && !neynarRegistration.pending
                && neynarRegistration.phase !== "reserved")
                nativeCustodyWallet.discard()
            stageTransitionTimer.stop()
            stageTransitioning = false
            pendingStep = -1
            stageOpacity = 1
            choosingRegistrationProvider = false
            registrationError = ""
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
            existingProtocolSelected = false
            recoveryProtocol = ""
            showRecovery = false
            consentChecked = legalConsentAccepted
        }
        forceActiveFocus()
        Qt.callLater(root.resumeNativePendingChecks)
    }
    onAuthenticationPhaseChanged: {
        if (authenticationPhase === "failed" || authenticationPhase === "signed-out")
            recoverySubmitPending = false
        if (!visible || currentStep < 4 || currentStep > 5)
            return
        // The browser's recovery-word dialog lives in the current app page.
        // Do not cover it with Verification while the words are being entered.
        if (browserReadOnly && !authenticated
            && (authenticationRoute === "recovery"
                || authenticationRoute === "browser-protocol-recovery"
                || authenticationRoute === "browser-protocol-create"))
            return
        if (authenticationInProgress)
            switchStep(5)
        else if (!authenticated && currentStep === 5
                 && authenticationPhase === "failed")
            switchStep(4)
    }
    onAuthenticationErrorTextChanged: {
        if (authenticationErrorText.length
            && (authenticationRoute === "recovery"
                || authenticationRoute === "browser-protocol-recovery"
                || authenticationRoute === "browser-protocol-create"
                || authenticationRoute === "hypersnap-recovery"
                || authenticationRoute === "farcaster-native-recovery")
            && !authenticated) {
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
        NeonCard {
            id: legalSurface
            interactive: false
            ambientNeon: true
            neonStrength: Theme.neonPanelStrength
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
        readonly property real brandWidth: desktopBrandPane.visible ? desktopBrandPane.width : 0
        readonly property real formWidth: width - brandWidth
        readonly property real formStart: Theme.rtl ? 0 : brandWidth

        Rectangle {
            id: desktopBrandPane
            objectName: "setupBrandPane"
            visible: !root.phone && root.width >= 980
            // One coordinate keeps the bound width independent of locale
            // changes. Switching between two anchors can briefly set both.
            x: Theme.rtl ? setupCanvas.width - width : 0
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.currentStep === 0 ? Math.min(parent.width * 0.35, 560)
                                          : Math.min(parent.width * 0.28, 400)
            border.width: brandInteraction.activeFocus ? 2 : 0
            border.color: Theme.accent
            // One local studio scene: the existing logo shader, a restrained
            // contrast pass, and light arcs sharing the same bounded clock.
            property real pointerX: 0.5
            property real pointerY: 0.5
            property bool pointerInside: false
            property real tapImpulse: 0
            // A complete 360° turn is available both automatically and by
            // pointer/touch drag. Rotation stops during manual interaction and
            // resumes gently; reduced-motion disables continuous animation.
            property real orbitYaw: 0
            property real dragYaw: -18
            property real dragPitch: -5
            property real viewZoom: 1.0
            property bool autoOrbitEnabled: true
            property bool dragging: false
            function resetView() {
                rotationCooldown.restart()
                orbitYaw = 0
                dragYaw = -18
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
            Behavior on pointerX {
                enabled: desktopBrandPane.motionAllowed && !desktopBrandPane.dragging
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }
            Behavior on pointerY {
                enabled: desktopBrandPane.motionAllowed && !desktopBrandPane.dragging
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }
            property real pressX: 0
            property real pressY: 0
            property bool movedByDrag: false
            readonly property bool motionAllowed: root.visible && visible && !Theme.reduceMotion
                && !Theme.powerSave && Qt.application.state === Qt.ApplicationActive
            onMotionAllowedChanged: {
                if (!motionAllowed) {
                    brandAccentTap.stop()
                    tapImpulse = 0
                    pointerX = 0.5
                    pointerY = 0.5
                }
            }
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
                GradientStop { position: 0.0; color: Theme.setupHeroTop }
                GradientStop { position: 0.56; color: Theme.setupHeroMiddle }
                GradientStop { position: 1.0; color: Theme.setupHeroBottom }
            }
            // Keep the sculpture and its optical framing within the rail.
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
                // Use Qt's shipped effect shaders; the canonical raymarcher
                // and its portable QSB stay paired without regeneration.
                layer.enabled: true
                layer.effect: MultiEffect {
                    contrast: 0.18
                    brightness: 0.015
                    saturation: -0.12
                }
            }

            // Wide, broken arcs frame the sculpture without touching its
            // silhouette. No particles or additional animation timers.
            Shape {
                id: brandOrbit
                objectName: "setupBrandOrbit"
                z: 1
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.91, parent.height * 0.71)
                height: width
                preferredRendererType: Shape.CurveRenderer
                opacity: 0.60 + desktopBrandPane.tapImpulse * 0.20
                rotation: -18 + desktopBrandPane.orbitYaw * 0.09
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.setupHeroPulseStroke
                    strokeWidth: 0.75
                    capStyle: ShapePath.RoundCap
                    PathAngleArc {
                        centerX: brandOrbit.width / 2
                        centerY: brandOrbit.height / 2
                        radiusX: brandOrbit.width * 0.48
                        radiusY: brandOrbit.height * 0.48
                        startAngle: 18
                        sweepAngle: 118
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.setupHeroTraceStroke
                    strokeWidth: 0.75
                    capStyle: ShapePath.RoundCap
                    PathAngleArc {
                        centerX: brandOrbit.width / 2
                        centerY: brandOrbit.height / 2
                        radiusX: brandOrbit.width * 0.48
                        radiusY: brandOrbit.height * 0.48
                        startAngle: 188
                        sweepAngle: 106
                    }
                }
                Superellipse {
                    readonly property real phase: desktopBrandPane.orbitYaw * Math.PI / 180
                    x: brandOrbit.width * (0.5 + 0.48 * Math.cos(phase)) - width / 2
                    y: brandOrbit.height * (0.5 + 0.48 * Math.sin(phase)) - height / 2
                    width: 5
                    height: 5
                    curveDepth: width / 2
                    exponent: 2
                    fillColor: Theme.setupHeroTitleInk
                }
            }

            // A second, offset orbit supplies depth without another shader,
            // texture capture, clock, or offscreen render target.
            Shape {
                id: brandSecondaryOrbit
                z: 1
                anchors.centerIn: parent
                width: brandOrbit.width * 0.79
                height: width
                preferredRendererType: Shape.CurveRenderer
                opacity: 0.38 + desktopBrandPane.tapImpulse * 0.15
                rotation: 36 - desktopBrandPane.orbitYaw * 0.13
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.setupHeroTraceStroke
                    strokeWidth: 1
                    capStyle: ShapePath.RoundCap
                    PathAngleArc {
                        centerX: brandSecondaryOrbit.width / 2
                        centerY: brandSecondaryOrbit.height / 2
                        radiusX: brandSecondaryOrbit.width * 0.47
                        radiusY: brandSecondaryOrbit.height * 0.47
                        startAngle: 204
                        sweepAngle: 78
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.setupHeroPulseStroke
                    strokeWidth: 0.8
                    capStyle: ShapePath.RoundCap
                    PathAngleArc {
                        centerX: brandSecondaryOrbit.width / 2
                        centerY: brandSecondaryOrbit.height / 2
                        radiusX: brandSecondaryOrbit.width * 0.47
                        radiusY: brandSecondaryOrbit.height * 0.47
                        startAngle: 32
                        sweepAngle: 54
                    }
                }
            }

            // A short glint acknowledges actual progress, never delays a step.
            Connections {
                target: root
                function onCurrentStepChanged() {
                    if (desktopBrandPane.motionAllowed) brandAccentTap.restart()
                }
            }

            // A softly graded boundary replaces the former hard 2px split.
            // The live stage-progress marker remains independently visible.
            Rectangle {
                z: 1
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 9
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Theme.setupHeroCardBorder }
                }
                opacity: 0.32
            }

            // Edge progression is connected to the actual Setup stage.
            // It follows the selected accent in light and dark themes.
            Rectangle {
                z: 1
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 1
                opacity: 0.63
                color: Theme.setupHeroCardBorder
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
                    } else if (desktopBrandPane.motionAllowed) {
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
                Accessible.name: qsTrId("genycaster.setup.logo_3d_accessible")
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
                    color: Theme.setupFooterDivider
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
                    Accessible.onPressAction: Qt.openUrlExternally((deploymentEndpoints.companyOrigin + "/"))
                    Keys.onReturnPressed: Qt.openUrlExternally((deploymentEndpoints.companyOrigin + "/"))
                    Keys.onEnterPressed: Qt.openUrlExternally((deploymentEndpoints.companyOrigin + "/"))
                    Keys.onSpacePressed: Qt.openUrlExternally((deploymentEndpoints.companyOrigin + "/"))

                    Text {
                        id: copyrightLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "© " + new Date().getFullYear() + " Genyleap"
                        color: Theme.setupFooterCopyrightInk
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Font.Medium
                        font.underline: genyleapCopyrightMouse.containsMouse || genyleapCopyrightLink.activeFocus
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: genyleapCopyrightMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally((deploymentEndpoints.companyOrigin + "/"))
                    }
                }
                Text {
                    width: parent.width
                    text: Qt.application.version ? "v" + LocaleFormat.localizeDigits(Qt.application.version) : ""
                    color: Theme.setupFooterVersionInk
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideRight
                }
            }
        }

        Item {
            id: desktopSetupNav
            visible: !root.phone && root.currentStep !== 0
            x: stagePanel.x
            width: stagePanel.width
            height: Theme.controlHeight + Theme.space4 * 2
            z: 5

            RowLayout {
                anchors.centerIn: parent
                layoutDirection: Theme.uiDirection
                spacing: Theme.space3
                SetupStepIndicator {
                    id: setupProgress
                    stepCount: root.progressCount
                    currentStep: root.currentStep
                    Accessible.ignored: true
                }
                DirectionAwareLabel {
                    id: stageCount
                    text: LocaleFormat.localizeDigits((root.currentStep + 1) + "/" + root.progressCount)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    forceLeftToRight: true
                    Accessible.name: root.stepTitle(root.currentStep) + " " + stageCount.text
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Theme.strokeHairline
                color: Theme.divider
            }
        }

        // On compact screens navigation occupies its own fixed header. The
        // Flickable starts below this lane, so a scrolled recovery-word field
        // can never appear beneath or collide with the Back button.
        Item {
            id: mobileSetupNav
            objectName: "mobileSetupNavigation"
            visible: root.phone && (root.currentStep > 0 || root.reviewMode)
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? Theme.controlHeight + Theme.space2 * 2 : 0
            z: 19
        }

        ActionButton {
            z: 20
            objectName: "setupBack"
            visible: root.currentStep > 0
            anchors.top: root.phone ? mobileSetupNav.top : parent.top
            anchors.topMargin: root.phone ? Theme.space2 : Theme.space4
            x: root.phone ? (Theme.rtl ? root.width - width - Theme.space2 : Theme.space2)
                          : (Theme.rtl ? stagePanel.x + stagePanel.width - width : stagePanel.x)
            compact: true
            quiet: true
            iconOnly: true
            text: qsTrId("genycaster.setup.back")
            iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
            onClicked: root.previousStep()
        }
        ActionButton {
            z: 20
            objectName: "setupCloseReview"
            visible: root.reviewMode
            anchors.top: root.phone ? mobileSetupNav.top : parent.top
            anchors.topMargin: root.phone ? Theme.space2 : Theme.space4
            x: root.phone ? (Theme.rtl ? Theme.space2 : root.width - width - Theme.space2)
                          : (Theme.rtl ? stagePanel.x : stagePanel.x + stagePanel.width - width)
            compact: true
            quiet: true
            iconOnly: true
            text: qsTrId("genycaster.locale.close")
            iconGlyph: Theme.iconClose
            onClicked: root.closeReviewRequested()
        }

        Flickable {
            id: stageScroll
            anchors.top: root.phone ? mobileSetupNav.bottom
                                    : desktopSetupNav.visible ? desktopSetupNav.bottom : parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            contentWidth: width
            contentHeight: Math.max(height, stagePanel.y + stagePanel.implicitHeight + Theme.space7)
            ScrollBar.vertical: GenyScrollBar {
                policy: stageScroll.contentHeight > stageScroll.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            Item {
                id: stagePanel
                objectName: "setupStagePanel"
                width: Math.max(0, Math.min(
                                   root.phone ? 440
                                   : root.currentStep === 4 ? (root.showRecovery || root.resumingRegistration ? 640 : 540)
                                   : root.currentStep === 3 && root.choosingRegistrationProvider ? 580
                                   : root.currentStep >= 3 && root.currentStep <= 6 ? 810 : 680,
                                   setupCanvas.formWidth - root.setupGutter * 2))
                x: setupCanvas.formStart + (setupCanvas.formWidth - width) / 2
                y: !root.phone && root.currentStep === 0
                   ? Math.max(Theme.space7,
                              (setupCanvas.height - stageLoader.implicitHeight
                               - Theme.space7 - stageAction.implicitHeight) * 0.47)
                   : !root.phone && root.currentStep > 0
                   ? Theme.space4
                   : root.currentStep === 0 || root.currentStep === 7
                   ? Math.max(root.phone ? 110 : 52, (stageScroll.height - stageLoader.implicitHeight) * 0.39)
                   : root.currentStep === 5
                   ? Math.max(96, (stageScroll.height - stageLoader.implicitHeight) * 0.38)
                   : root.currentStep === 1
                   ? Math.max(76, (stageScroll.height - stageLoader.implicitHeight) * 0.21)
                   : Math.max(root.phone ? Theme.space6 : 115,
                              Math.min(root.phone ? Theme.space7 : 125,
                                       stageScroll.height * 0.15))
                implicitHeight: stageLoader.implicitHeight
                                + (root.showsPrimaryCta
                                   ? Theme.space5 + stageAction.implicitHeight : 0)
                height: implicitHeight

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

                // Stage-owned action follows the current form in scroll content.
                SetupPrimaryButton {
            id: stageAction
            objectName: "setupStageAction"
            visible: root.showsPrimaryCta
            z: 10
            anchors.top: stageLoader.bottom
            anchors.topMargin: Theme.space5
            width: root.welcomeNativeAppsVisible
                   ? (Math.min(root.welcomeActionsWidth, stagePanel.width) - Theme.space2)
                     * (root.phone ? 0.42 : 0.47)
                   : root.phone ? stagePanel.width
                   : Math.min(Theme.scaleUi(330), stagePanel.width * 0.65)
            x: root.welcomeNativeAppsVisible
               ? (stagePanel.width - Math.min(root.welcomeActionsWidth, stagePanel.width)) / 2
                 + (Theme.rtl ? Math.min(root.welcomeActionsWidth, stagePanel.width) - width : 0)
               : root.phone ? 0 : Theme.rtl ? 0 : stagePanel.width - width
            text: root.actionText()
            // The primary stage action uses the same brand-filled surface as
            // the other prominent CTAs, rather than a flat white slab.
            neutralPrimary: false
            enabled: !root.stageTransitioning && root.actionEnabled()
            showArrow: root.currentStep === 0
            onClicked: root.nextStep()
        }

        // No banner, card or extra onboarding step: native builds remain one
        // directly accessible secondary action alongside Get Started.
        ActionButton {
            objectName: "setupWelcomeNativeApps"
            visible: root.welcomeNativeAppsVisible
            z: 10
            anchors.bottom: stageAction.bottom
            width: Math.max(0, root.welcomeActionsWidth - Theme.space2 - stageAction.width)
            height: stageAction.height
            x: Theme.rtl ? stageAction.x - Theme.space2 - width
                         : stageAction.x + stageAction.width + Theme.space2
            text: qsTrId("genycaster.apps.download_cta_button")
            iconGlyph: Theme.iconDownload
            wrapText: true
            elevated: false
            focusPolicy: Qt.StrongFocus
            onClicked: root.nativeAppDownloadsRequested()
        }
            }
        }
    }

    Component {
        id: welcomeStage
        ColumnLayout {
            id: welcomeContent
            spacing: Theme.space3
            LayoutMirroring.enabled: Theme.rtl

            // The icon, title, supporting copy and progress form one centered
            // block. The CTA is placed directly beneath it by stageAction.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.phone ? 62 : 70
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.phone ? (Theme.rtl ? parent.width - width : 0)
                                  : (parent.width - width) / 2
                    width: root.phone ? 54 : 58
                    height: width
                    source: (Qt.platform.os === "android" ? "image://genycaster-brand/mark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg")
                    fillMode: Image.PreserveAspectFit
                    sourceSize: Qt.size(160, 160)
                    smooth: true
                    mipmap: false
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space1
                text: qsTrId("genycaster.setup.welcome_title")
                font.pixelSize: root.phone ? 32 : 40
                font.weight: Theme.weightBold
                lineHeight: 1.14
                color: Theme.ink
                wrapMode: Text.WordWrap
                horizontalAlignment: root.phone ? Theme.textStartAlignment : Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space1
                text: qsTrId("genycaster.setup.design_welcome_description")
                color: Theme.inkMuted
                font.pixelSize: root.phone ? Theme.typeLabelSize
                                          : Theme.typeLabelSize + Theme.scaleUi(1)
                lineHeight: 1.45
                wrapMode: Text.WordWrap
                horizontalAlignment: root.phone ? Theme.textStartAlignment : Text.AlignHCenter
            }
            // Welcome and all later pages share marker geometry and count.
            SetupStepIndicator {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: root.phone ? Theme.space6 : Theme.space5
                markerSpacing: root.phone ? 8 : 10
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
            spacing: root.phone ? Theme.space2 : Theme.space4

            SetupPageHeader {
                visible: !root.phone
                Layout.fillWidth: true
                storyboardStyle: true
                compactDesktopStyle: true
                maximumTextWidth: 680
                iconGlyph: Theme.iconLanguage
                title: qsTrId("genycaster.setup.design_language_title")
            }

            GridLayout {
                objectName: "setupLanguageGrid"
                visible: !root.phone
                Layout.fillWidth: true
                columns: stagePanel.width >= Theme.scaleUi(620) * Math.max(1, Theme.fontScale) ? 3 : 2
                uniformCellWidths: true
                layoutDirection: Theme.uiDirection
                rowSpacing: Theme.space2
                columnSpacing: Theme.space2

                Repeater {
                    model: root.phone ? [] : root.greetings
                    delegate: SetupActionCard {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        compactCard: true
                        onboardingStyle: true
                        protocolChoice: true
                        showChevron: false
                        selected: index === root.greetingIndexForTag(localizationController.effectiveLanguageTag)
                        title: modelData.language
                        onClicked: localizationController.setLanguageTag(modelData.tag)
                    }
                }
            }

            SetupLanguageSelector {
                id: languageWheel
                visible: root.phone
                Layout.fillWidth: true
                storyboard: true
                model: root.phone ? root.greetings : []
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
                maximumTextWidth: root.phone ? 620 : 810
                Layout.fillWidth: true
                iconGlyph: "system/lock"
                title: qsTrId("genycaster.setup.design_terms_title")
                subtitle: qsTrId("genycaster.setup.design_terms_description")
            }
            Item { Layout.preferredHeight: Theme.space3 }
            GridLayout {
                Layout.fillWidth: true
                columns: root.wideStage ? 2 : 1
                uniformCellWidths: columns > 1
                layoutDirection: Theme.uiDirection
                rowSpacing: Theme.space3
                columnSpacing: Theme.space3
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
            }
            Button {
                id: consentCheck
                Layout.fillWidth: true
                Layout.topMargin: Theme.space3
                implicitHeight: Math.max(Theme.touchTarget, consentLabel.implicitHeight + Theme.space3 * 2)
                leftPadding: Theme.space3
                rightPadding: Theme.space3
                topPadding: Theme.space3
                bottomPadding: Theme.space3
                enabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: qsTrId("genycaster.setup.single_legal_consent")
                Accessible.checked: root.consentChecked
                onClicked: root.consentChecked = !root.consentChecked
                background: NeonSurface {
                    interactive: consentCheck.enabled
                    ambientNeon: false
                    deferredNeon: true
                    focused: Theme.keyboardFocusVisible(consentCheck)
                    externalHovered: consentCheck.hovered
                    neonStrength: Theme.neonControlStrength
                    fillColor: consentCheck.down ? Theme.surfacePressed
                               : root.consentChecked ? Theme.surfaceSelected
                               : consentCheck.hovered ? Theme.surfaceHover : Theme.surfaceMuted
                    borderWidth: Theme.keyboardFocusVisible(consentCheck) ? 2 : Theme.strokeHairline
                    borderColor: Theme.keyboardFocusVisible(consentCheck) ? Theme.focus
                               : root.consentChecked ? Theme.accent : Theme.borderMuted
                    curveDepth: Theme.curveContainer
                }
                contentItem: RowLayout {
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection
                    Superellipse {
                        Layout.preferredWidth: Theme.scaleUi(24)
                        Layout.preferredHeight: Theme.scaleUi(24)
                        fillColor: root.consentChecked ? Theme.accent : Theme.surface
                        borderColor: root.consentChecked ? Theme.accent : Theme.borderStrong
                        borderWidth: Theme.strokeStandard
                        curveDepth: Theme.scaleUi(6)
                        Icon {
                            anchors.centerIn: parent
                            visible: root.consentChecked
                            text: Theme.iconCheck
                            color: Theme.contrastInk(Theme.accent)
                            preferredSize: Theme.scaleUi(16)
                        }
                    }
                    DirectionAwareLabel {
                        id: consentLabel
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.single_legal_consent")
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
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
                maximumTextWidth: root.phone ? 620 : 810
                Layout.fillWidth: true
                showIcon: !root.choosingRegistrationProvider
                iconGlyph: root.accountFidRegistered
                    ? Theme.iconCheck : Theme.iconUser
                title: root.accountFidRegistered
                    ? qsTrId("genycaster.setup.account_verified")
                    : root.choosingRegistrationProvider
                      ? (root.registrationProviderId.length > 0
                          ? qsTrId("genycaster.setup.design_create_account")
                          : qsTrId("genycaster.setup.choose_protocol"))
                      : qsTrId("genycaster.setup.design_account_title")
                subtitle: root.accountFidRegistered
                    ? qsTrId("genycaster.setup.registration_next_device")
                    : root.choosingRegistrationProvider
                      ? root.registrationProviderId === "hypersnap-v2"
                        && nativeCustodyWallet.backupVerified
                        ? ""
                        : qsTrId("genycaster.setup.choose_protocol_description")
                      : qsTrId("genycaster.setup.design_account_description")
            }
            Item { Layout.preferredHeight: Theme.space2 }
            // A verified protocol FID is not automatically an OPP identity.
            // Show the independent proof result without obstructing the
            // already confirmed registration or repeating its chain write.
            ColumnLayout {
                objectName: "setupCentralAccountEnrollment"
                Layout.fillWidth: true
                spacing: Theme.space2
                visible: root.accountFidRegistered && root.authenticated
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: openProofConnections.nativeEnrollmentPhase === "linked"
                        ? qsTrId("genycaster.setup.opp_linked")
                        : openProofConnections.nativeEnrollmentPhase === "wallet-linked"
                          ? qsTrId("genycaster.setup.opp_wallet_linked")
                          : openProofConnections.nativeEnrollmentPhase === "pending"
                            || openProofConnections.nativeEnrollmentPhase === "idle"
                            ? root.registrationProviderId === "hypersnap-v2"
                              ? qsTrId("genycaster.setup.opp_wallet_pending")
                              : qsTrId("genycaster.setup.opp_pending")
                            : root.registrationProviderId === "hypersnap-v2"
                              ? qsTrId("genycaster.setup.opp_wallet_retry")
                              : qsTrId("genycaster.setup.opp_retry")
                    color: openProofConnections.nativeEnrollmentPhase === "linked"
                           ? Theme.success
                           : openProofConnections.nativeEnrollmentPhase === "retry"
                             ? Theme.warning : Theme.inkMuted
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.typeCaptionSize
                }
                ActionButton {
                    objectName: "setupRetryCentralAccount"
                    Layout.fillWidth: true
                    compact: true
                    visible: openProofConnections.nativeEnrollmentPhase === "retry"
                    text: qsTrId("genycaster.setup.opp_retry_action")
                    onClicked: openProofConnections.retryNativeEnrollment()
                }
            }
            GridLayout {
                visible: !root.choosingRegistrationProvider
                Layout.fillWidth: true
                columns: root.wideStage ? 2 : 1
                rowSpacing: Theme.space3
                columnSpacing: Theme.space3
                layoutDirection: Theme.uiDirection
                SetupActionCard {
                    Layout.fillWidth: true
                    onboardingStyle: true
                    objectName: "setupCreateAccount"
                    accountShowcase: true
                    title: qsTrId("genycaster.setup.design_create_account")
                    subtitle: qsTrId("genycaster.setup.new_profile_body")
                    iconGlyph: Theme.iconUser
                    showChevron: true
                    onClicked: {
                        root.accountIntent = "new"
                        if (!nativeRegistrationPublisher.pending && neynarRegistration.phase !== "reserved")
                            root.registrationProviderId = ""
                        root.choosingRegistrationProvider = true
                        stageScroll.contentY = 0
                        root.confirmWalletReplacement = false
                        root.registrationError = ""
                        nativeRegistrationStatus.refresh()
                    }
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    onboardingStyle: true
                    accountShowcase: true
                    title: qsTrId("genycaster.setup.design_existing_account")
                    subtitle: qsTrId("genycaster.setup.design_existing_description")
                    iconGlyph: Theme.iconLink
                    showChevron: true
                    enabled: !nativeRegistrationPublisher.pending
                             && !neynarRegistration.pending
                             && neynarRegistration.phase !== "reserved"
                    onClicked: {
                        neynarRegistration.reset()
                        nativeRegistrationPublisher.clear()
                        nativeRegistrationPreparation.cancel()
                        nativeRegistrationStatus.clearExistingLookup()
                        nativeCustodyWallet.discard()
                        root.accountIntent = "existing"
                        root.existingProtocolSelected = false
                        root.recoveryProtocol = ""
                        root.showRecovery = false
                        root.switchStep(4)
                    }
                }
            }
            ColumnLayout {
                visible: root.choosingRegistrationProvider
                Layout.fillWidth: true
                spacing: Theme.space3
                GridLayout {
                    visible: !root.accountFidRegistered
                             && !(root.registrationProviderId === "hypersnap-v2"
                                  && nativeCustodyWallet.backupVerified)
                    Layout.fillWidth: true
                    columns: root.wideStage ? 2 : 1
                    uniformCellWidths: columns > 1
                    rowSpacing: Theme.space3
                    columnSpacing: Theme.space3
                    layoutDirection: Theme.uiDirection
                    SetupActionCard {
                        objectName: "setupRegisterHyperSnap"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onboardingStyle: true
                        networkShowcase: true
                        protocolChoice: true
                        title: qsTrId("genycaster.settings.hypersnap_label")
                        subtitle: qsTrId("genycaster.setup.registration_hypersnap_description")
                        iconGlyph: "quilibrium/hypersnap"
                        connectionState: root.protocolConnectionState(false)
                        connectionText: root.protocolConnectionText(false, connectionState)
                        selected: root.registrationProviderId === "hypersnap-v2"
                        enabled: !neynarRegistration.pending
                                 && neynarRegistration.phase !== "reserved"
                        onClicked: {
                            neynarRegistration.reset()
                            nativeRegistrationPublisher.clear()
                            nativeRegistrationPreparation.cancel()
                            nativeRegistrationStatus.clearExistingLookup()
                            root.registrationProviderId = "hypersnap-v2"
                            root.registrationError = ""
                            nativeRegistrationStatus.refresh()
                        }
                    }
                    SetupActionCard {
                        objectName: "setupRegisterNeynar"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onboardingStyle: true
                        networkShowcase: true
                        protocolChoice: true
                        title: qsTrId("genycaster.setup.registration_neynar_label")
                        subtitle: qsTrId("genycaster.setup.registration_neynar_description")
                        iconGlyph: "brands/farcaster"
                        connectionState: root.protocolConnectionState(true)
                        connectionText: root.protocolConnectionText(true, connectionState)
                        selected: root.registrationProviderId === "farcaster-snapchain"
                        enabled: !nativeRegistrationPublisher.pending
                                 && !neynarRegistration.pending
                        onClicked: {
                            nativeRegistrationPublisher.clear()
                            nativeRegistrationPreparation.cancel()
                            nativeRegistrationStatus.clearExistingLookup()
                            root.registrationProviderId = "farcaster-snapchain"
                            root.registrationError = ""
                        }
                    }
                }
                SetupConnectionPanel {
                    objectName: "setupRegistrationConnections"
                    Layout.fillWidth: true
                    visible: root.showsSetupConnectivity
                             && !(root.registrationProviderId === "hypersnap-v2"
                                  && nativeCustodyWallet.backupVerified)
                }
                // Browser uses verified external identity paths until a
                // WebCrypto custody signer and native chain registration
                // have been implemented and tested. Do not solicit a seed
                // phrase in a non-operational registration controller.
                ColumnLayout {
                    objectName: "setupBrowserRegistrationPath"
                    visible: root.browserReadOnly && root.registrationProviderId.length > 0
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: root.registrationProviderId === "farcaster-snapchain"
                        text: qsTrId("genycaster.setup.neynar_registration_requirements")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    SetupActionCard {
                        objectName: "setupBrowserCreateHyperSnap"
                        visible: root.registrationProviderId === "hypersnap-v2"
                        Layout.fillWidth: true
                        compactCard: true
                        onboardingStyle: true
                        iconGlyph: "quilibrium/hypersnap"
                        title: qsTrId("genycaster.setup.design_create_account")
                        enabled: root.runtimeReady && !root.authenticationInProgress
                        onClicked: {
                            root.browserCreateAccount("hypersnap-v2")
                        }
                    }
                    SetupActionCard {
                        objectName: "setupBrowserExternalHyperSnap"
                        visible: root.registrationProviderId === "hypersnap-v2"
                        Layout.fillWidth: true
                        compactCard: true
                        onboardingStyle: true
                        title: qsTrId("genycaster.shell.continue_with_genyleap")
                        brandImageSource: (Qt.platform.os === "android" ? "image://genycaster-brand/mark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg")
                        enabled: root.runtimeReady && root.authenticationAvailable
                                 && !root.authenticationInProgress
                        onClicked: {
                            // External identity authorization is an existing
                            // account route. It cannot establish a new native
                            // protocol FID or satisfy new-account signer gates.
                            root.accountIntent = "existing"
                            root.choosingRegistrationProvider = false
                            root.existingProtocolSelected = true
                            root.recoveryProtocol = "hypersnap-v2"
                            root.continueWithGenyleap()
                            root.switchStep(5)
                        }
                    }
                    SetupActionCard {
                        objectName: "setupBrowserCreateFarcaster"
                        visible: root.registrationProviderId === "farcaster-snapchain"
                        Layout.fillWidth: true
                        compactCard: true
                        onboardingStyle: true
                        iconGlyph: "brands/farcaster"
                        title: qsTrId("genycaster.setup.design_create_account")
                        enabled: root.runtimeReady && !root.authenticationInProgress
                        onClicked: {
                            root.browserCreateAccount("farcaster-snapchain")
                        }
                    }
                    SetupActionCard {
                        objectName: "setupBrowserExternalFarcaster"
                        visible: root.registrationProviderId === "farcaster-snapchain"
                        Layout.fillWidth: true
                        compactCard: true
                        onboardingStyle: true
                        iconGlyph: "brands/farcaster"
                        title: qsTrId("genycaster.shell.sign_in_with_farcaster")
                        enabled: root.runtimeReady && root.authenticationAvailable
                                 && !root.authenticationInProgress
                        onClicked: {
                            root.accountIntent = "existing"
                            root.choosingRegistrationProvider = false
                            root.existingProtocolSelected = true
                            root.recoveryProtocol = "farcaster-snapchain"
                            root.continueWithFarcaster()
                            root.switchStep(5)
                        }
                    }
                }
                ColumnLayout {
                    objectName: "setupWalletReplacement"
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    visible: root.nativeDetailsExpanded
                             && nativeCustodyWallet.hasWallet
                             && nativeCustodyWallet.backupVerified
                             && root.registrationProviderId === "hypersnap-v2"
                             && !nativeRegistrationPublisher.confirmed
                             && nativeRegistrationStatus.existingNativeFid.length === 0
                    ActionButton {
                        objectName: "setupChooseDifferentWallet"
                        Layout.fillWidth: true
                        compact: true
                        enabled: !nativeRegistrationPublisher.busy
                        text: qsTrId("genycaster.setup.wallet_switch_action")
                        onClicked: root.confirmWalletReplacement = true
                    }
                    DirectionAwareLabel {
                        objectName: "setupWalletReplacementWarning"
                        visible: root.confirmWalletReplacement
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.wallet_switch_warning")
                        color: Theme.warning
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        layoutDirection: Theme.uiDirection
                        visible: root.confirmWalletReplacement
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        ActionButton {
                            Layout.fillWidth: true
                            compact: true
                            text: qsTrId("genycaster.shell.cancel")
                            onClicked: root.confirmWalletReplacement = false
                        }
                        ActionButton {
                            objectName: "setupConfirmDifferentWallet"
                            Layout.fillWidth: true
                            prominent: true
                            enabled: nativeCustodyWallet.backupVerified
                                     && !nativeRegistrationPublisher.busy
                            text: qsTrId("genycaster.setup.wallet_switch_confirm")
                            onClicked: {
                                // A prior pending registration remains on the
                                // network and is recoverable with its old
                                // phrase. This action changes only the wallet
                                // held locally by the onboarding UI.
                                if (!nativeRegistrationPublisher.detachWalletForNewRegistration())
                                    return
                                nativeRegistrationPreparation.cancel()
                                nativeRegistrationStatus.clearExistingLookup()
                                nativeCustodyWallet.discard()
                                root.confirmWalletReplacement = false
                                root.registrationError = ""
                                nativeRegistrationStatus.refresh()
                            }
                        }
                    }
                }
                ColumnLayout {
                    visible: !root.browserReadOnly && root.registrationProviderId.length > 0
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    DirectionAwareLabel {
                        visible: !nativeCustodyWallet.hasWallet
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.wallet_create_description")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkMuted
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "setupRestoreInterruptedWallet"
                        visible: !nativeCustodyWallet.hasWallet
                        Layout.fillWidth: true
                        compact: true
                        text: qsTrId("genycaster.setup.resume_registration_with_phrase")
                        onClicked: {
                            root.resumingRegistration = true
                            root.recoveryProtocol = root.registrationProviderId
                            root.showRecovery = true
                            root.switchStep(4)
                        }
                    }
                    ActionButton {
                        objectName: "setupGenerateCustodyWallet"
                        visible: !nativeCustodyWallet.hasWallet
                        Layout.fillWidth: true
                        prominent: true
                        text: qsTrId("genycaster.setup.wallet_create_label")
                        onClicked: {
                            nativeRegistrationStatus.clearExistingLookup()
                            nativeCustodyWallet.generate()
                        }
                    }
                    DirectionAwareLabel {
                        visible: nativeCustodyWallet.recoveryVisible
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.wallet_recovery_title")
                        font.pixelSize: Theme.typeTitle3Size
                        color: Theme.ink
                        font.weight: Theme.weightDemiBold
                    }
                    DirectionAwareLabel {
                        visible: nativeCustodyWallet.recoveryVisible
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.wallet_recovery_warning")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.warning
                        wrapMode: Text.WordWrap
                    }
                    NeonCard {
                        visible: nativeCustodyWallet.recoveryVisible
                        // The private phrase is display-only; the card has
                        // stationary material, no pointer-driven animation.
                        interactive: false
                        ambientNeon: true
                        neonStrength: Theme.neonPanelStrength
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.phone ? Theme.scaleUi(480) : Theme.scaleUi(260)
                        fillColor: Theme.surface
                        borderColor: Theme.borderMuted
                        borderWidth: Theme.strokeStandard
                        curveDepth: Theme.curveContainer
                        NativeRecoveryWords {
                            id: nativeWalletWords
                            objectName: "nativeRecoveryWordsDisplay"
                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            wallet: nativeCustodyWallet
                            inkColor: Theme.ink
                            Accessible.name: qsTrId("genycaster.setup.wallet_recovery_title")
                            Accessible.description: qsTrId("genycaster.setup.wallet_recovery_warning")
                        }
                    }
                    ActionButton {
                        objectName: "setupCopyRecoveryWords"
                        visible: nativeCustodyWallet.recoveryVisible
                        Layout.fillWidth: true
                        compact: true
                        text: root.recoveryWordsCopied
                            ? qsTrId("genycaster.setup.wallet_copy_success")
                            : qsTrId("genycaster.setup.wallet_copy_label")
                        onClicked: {
                            root.recoveryWordsCopied = nativeCustodyWallet.copyRecoveryWords()
                            if (root.recoveryWordsCopied)
                                recoveryCopyFeedbackTimer.restart()
                        }
                    }
                    DirectionAwareLabel {
                        visible: nativeCustodyWallet.recoveryVisible
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.wallet_copy_warning")
                        color: Theme.warning
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    GenyCheckRow {
                        id: walletBackupAcknowledgment
                        objectName: "walletBackupAcknowledgment"
                        visible: nativeCustodyWallet.recoveryVisible
                        Layout.fillWidth: true
                        label: qsTrId("genycaster.setup.wallet_backup_ack")
                    }
                    ActionButton {
                        objectName: "setupWalletBackupChallenge"
                        visible: nativeCustodyWallet.recoveryVisible
                        enabled: walletBackupAcknowledgment.checked
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.wallet_verify_start")
                        onClicked: {
                            if (nativeCustodyWallet.startBackupChallenge())
                                walletBackupAcknowledgment.checked = false
                        }
                    }
                    ColumnLayout {
                        visible: nativeCustodyWallet.hasWallet
                            && !nativeCustodyWallet.recoveryVisible
                            && !nativeCustodyWallet.backupVerified
                            && nativeCustodyWallet.firstChallengeIndex >= 0
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.wallet_challenge_explanation")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        GridLayout {
                            layoutDirection: Theme.uiDirection
                            columns: root.phone ? 1 : 2
                            Layout.fillWidth: true
                            columnSpacing: Theme.space2
                            rowSpacing: Theme.space2
                            ColumnLayout {
                                Layout.fillWidth: true
                                DirectionAwareLabel {
                                    text: qsTrId("genycaster.setup.wallet_challenge_word")
                                        .arg(nativeCustodyWallet.firstChallengeIndex + 1)
                                }
                                DirectionAwareTextField {
                                    id: walletChallengeFirst
                                    forceLeftToRight: true
                                    objectName: "walletChallengeFirst"
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    maximumLength: 24
                                    inputMethodHints: Qt.ImhSensitiveData
                                                      | Qt.ImhNoPredictiveText
                                                      | Qt.ImhNoAutoUppercase
                                    Accessible.name: qsTrId("genycaster.setup.wallet_challenge_word")
                                        .arg(nativeCustodyWallet.firstChallengeIndex + 1)
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                DirectionAwareLabel {
                                    text: qsTrId("genycaster.setup.wallet_challenge_word")
                                        .arg(nativeCustodyWallet.secondChallengeIndex + 1)
                                }
                                DirectionAwareTextField {
                                    id: walletChallengeSecond
                                    forceLeftToRight: true
                                    objectName: "walletChallengeSecond"
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    maximumLength: 24
                                    inputMethodHints: Qt.ImhSensitiveData
                                                      | Qt.ImhNoPredictiveText
                                                      | Qt.ImhNoAutoUppercase
                                    Accessible.name: qsTrId("genycaster.setup.wallet_challenge_word")
                                        .arg(nativeCustodyWallet.secondChallengeIndex + 1)
                                }
                            }
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            objectName: "setupVerifyWalletBackup"
                            text: qsTrId("genycaster.setup.wallet_verify")
                            enabled: walletChallengeFirst.length > 0
                                     && walletChallengeSecond.length > 0
                            onClicked: {
                                nativeCustodyWallet.verifyBackup(
                                    walletChallengeFirst.text, walletChallengeSecond.text)
                                walletChallengeFirst.clear()
                                walletChallengeSecond.clear()
                            }
                        }
                        ActionButton {
                            objectName: "setupRevealWalletBackup"
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.wallet_reveal")
                            onClicked: {
                                walletChallengeFirst.clear()
                                walletChallengeSecond.clear()
                                nativeCustodyWallet.revealForBackup()
                            }
                        }
                    }
                    DirectionAwareLabel {
                        objectName: "setupVerifiedCustodyWallet"
                        visible: nativeCustodyWallet.backupVerified
                                 && !nativeRegistrationPublisher.confirmed
                        Layout.fillWidth: true
                        text: root.nativeDetailsExpanded
                            ? qsTrId("genycaster.setup.wallet_verified")
                                .arg(nativeCustodyWallet.address)
                            : qsTr("Recovery words verified")
                        font.pixelSize: Theme.typeLabelSize
                        color: Theme.success
                        wrapMode: Text.WrapAnywhere
                    }
                    DirectionAwareLabel {
                        visible: nativeCustodyWallet.error.length > 0
                        Layout.fillWidth: true
                        text: nativeCustodyWallet.error
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.danger
                        wrapMode: Text.WordWrap
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: root.nativeDetailsExpanded
                             && !root.browserReadOnly
                             && root.registrationProviderId === "hypersnap-v2"
                             && !nativeRegistrationPublisher.confirmed
                             && nativeRegistrationStatus.existingNativeFid.length === 0
                    text: qsTrId("genycaster.setup.registration_storage_note")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                    wrapMode: Text.WordWrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: root.registrationError.length > 0
                    text: root.registrationError
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.danger
                    wrapMode: Text.WordWrap
                }
                // Exactly one network-specific advisory, not permanent
                // placeholder warnings about missing features. The separate
                // submit panels report pending writes and confirmed FIDs.
                DirectionAwareLabel {
                    objectName: "setupRegistrationReadiness"
                    Layout.fillWidth: true
                    visible: !root.browserReadOnly && root.registrationProviderId.length > 0
                        && (root.registrationProviderId === "hypersnap-v2"
                        ? root.nativeDetailsExpanded && !nativeRegistrationPublisher.pending
                          && !nativeRegistrationPublisher.busy
                          && !nativeRegistrationPublisher.confirmed
                          && nativeRegistrationStatus.existingNativeFid.length === 0
                        : neynarRegistration.phase === "idle")
                    text: root.registrationReadinessText()
                    color: root.registrationNodeUsable ? Theme.accent
                           : Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                ColumnLayout {
                    objectName: "neynarFarcasterRegistration"
                    visible: !root.browserReadOnly
                             && root.registrationProviderId === "farcaster-snapchain"
                             && nativeCustodyWallet.backupVerified
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: !neynarRegistration.confirmed
                        text: qsTrId("genycaster.setup.neynar_voucher_explanation")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    DirectionAwareTextField {
                        id: neynarEnrollmentVoucher
                        forceLeftToRight: true
                        objectName: "setupNeynarVoucher"
                        Layout.fillWidth: true
                        visible: !neynarRegistration.hasVoucher
                                 && !neynarRegistration.pending
                                 && !neynarRegistration.confirmed
                        placeholderText: qsTrId("genycaster.setup.neynar_voucher_placeholder")
                        echoMode: TextInput.Password
                        maximumLength: 1100
                        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                        Accessible.name: qsTrId("genycaster.setup.neynar_voucher_placeholder")
                    }
                    ActionButton {
                        objectName: "setupUseNeynarVoucher"
                        Layout.fillWidth: true
                        compact: true
                        visible: !neynarRegistration.hasVoucher
                                 && !neynarRegistration.pending
                                 && !neynarRegistration.confirmed
                        text: qsTrId("genycaster.setup.neynar_voucher_accept")
                        enabled: neynarEnrollmentVoucher.text.length > 60
                                 && !neynarRegistration.busy
                        onClicked: {
                            neynarRegistration.useEnrollmentVoucher(
                                neynarEnrollmentVoucher.text)
                            neynarEnrollmentVoucher.clear()
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: neynarRegistration.hasVoucher
                                 && neynarRegistration.phase === "ready"
                        text: qsTrId("genycaster.setup.neynar_voucher_ready")
                        color: Theme.success
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "setupReserveNeynarFid"
                        Layout.fillWidth: true
                        prominent: false
                        visible: neynarRegistration.phase === "ready"
                        text: qsTrId("genycaster.setup.neynar_reserve_fid")
                        busy: neynarRegistration.busy
                        enabled: root.runtimeReady && neynarRegistration.available
                                 && neynarRegistration.registrationSupported
                                 && !neynarRegistration.busy
                                 && nativeCustodyWallet.backupVerified
                        onClicked: neynarRegistration.reserveFid()
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: neynarRegistration.phase === "reserved"
                        text: qsTrId("genycaster.setup.neynar_reserved_fid")
                            .arg(neynarRegistration.reservedFid)
                        color: Theme.success
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    DirectionAwareTextField {
                        id: neynarRequestedUsername
                        forceLeftToRight: true
                        objectName: "setupNeynarUsername"
                        Layout.fillWidth: true
                        visible: neynarRegistration.phase === "reserved"
                        maximumLength: 16
                        placeholderText: qsTrId("genycaster.setup.neynar_username_placeholder")
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                        Accessible.name: qsTrId("genycaster.setup.neynar_username_placeholder")
                    }
                    GenyCheckRow {
                        id: neynarTransferConsent
                        objectName: "setupNeynarTransferConsent"
                        visible: neynarRegistration.phase === "reserved"
                        Layout.fillWidth: true
                        label: qsTrId("genycaster.setup.neynar_transfer_consent")
                    }
                    ActionButton {
                        objectName: "setupSubmitNeynarRegistration"
                        Layout.fillWidth: true
                        prominent: true
                        visible: neynarRegistration.phase === "reserved"
                        text: qsTrId("genycaster.setup.neynar_submit_registration")
                        enabled: root.runtimeReady && neynarTransferConsent.checked
                                 && neynarRegistration.canSubmit
                                 && /^[a-z0-9][a-z0-9-]{0,15}$/.test(
                                     neynarRequestedUsername.text.trim().toLowerCase())
                        onClicked: neynarRegistration.submit(
                            neynarRequestedUsername.text)
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: neynarRegistration.pending
                                 || neynarRegistration.confirmed
                                 || neynarRegistration.phase === "rejected"
                                 || neynarRegistration.phase === "reserved"
                        text: neynarRegistration.confirmed
                            ? qsTrId("genycaster.setup.neynar_fid_confirmed")
                                .arg(neynarRegistration.reservedFid)
                            : neynarRegistration.phase === "reserved"
                              ? qsTrId("genycaster.setup.neynar_reservation_time")
                              : neynarRegistration.phase === "rejected"
                                ? qsTrId("genycaster.setup.neynar_registration_rejected")
                                : qsTrId("genycaster.setup.neynar_waiting_confirmation")
                        color: neynarRegistration.confirmed
                            ? Theme.success : Theme.warning
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: neynarRegistration.error.length > 0
                        text: neynarRegistration.error === "registration_not_configured"
                            ? qsTrId("genycaster.setup.neynar_backend_unavailable")
                            : qsTrId("genycaster.setup.neynar_registration_error")
                        color: Theme.danger
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "setupCheckNeynarRegistration"
                        Layout.fillWidth: true
                        compact: true
                        visible: neynarRegistration.hasVoucher
                                 && !neynarRegistration.confirmed
                        text: qsTrId("genycaster.setup.neynar_check_again")
                        enabled: !neynarRegistration.busy
                        onClicked: neynarRegistration.checkConfirmation()
                    }
                }
                // ONE public action controls lookup -> proof -> submit -> FID.
                // No protocol-specific engineering steps are exposed as CTAs.
                ColumnLayout {
                    objectName: "nativeRegistrationBroadcastPanel"
                    visible: !root.browserReadOnly
                             && root.registrationProviderId === "hypersnap-v2"
                             && nativeCustodyWallet.backupVerified
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    DirectionAwareLabel {
                        objectName: "setupNativeCreationSummary"
                        visible: !nativeRegistrationPublisher.pending
                                 && !nativeRegistrationPublisher.confirmed
                                 && nativeRegistrationStatus.existingNativeFid.length === 0
                                 && (!nativeRegistrationStatus.syncedForRegistration
                                     || nativeRegistrationStatus.accountLookupBusy
                                     || !nativeRegistrationStatus.accountLookupChecked
                                     || nativeRegistrationStatus.accountLookupFailed)
                        Layout.fillWidth: true
                        text: nativeRegistrationStatus.shardZeroBlockDelay > 120
                              ? qsTr("HyperSnap is catching up. Account creation will be available when the network is ready.")
                              : (!nativeRegistrationStatus.active
                                 && !nativeRegistrationStatus.checking)
                                || nativeRegistrationStatus.accountLookupFailed
                                ? qsTr("Couldn't reach HyperSnap. Try again.")
                                : qsTr("Checking your account…")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "setupBeginRegistration"
                        Layout.fillWidth: true
                        prominent: true
                        visible: !nativeRegistrationPublisher.pending
                                 && !nativeRegistrationPublisher.confirmed
                                 && nativeRegistrationStatus.existingNativeFid.length === 0
                        text: qsTr("Create account")
                        busy: root.nativeCreationRequested
                              || nativeRegistrationPreparation.busy
                              || nativeRegistrationPublisher.busy
                              || nativeRegistrationStatus.accountLookupBusy
                        enabled: root.registrationNodeUsable
                                 && nativeRegistrationStatus.accountLookupChecked
                                 && !nativeRegistrationStatus.accountLookupFailed
                                 && !nativeRegistrationStatus.accountLookupBusy
                                 && !root.nativeCreationRequested
                                 && !nativeRegistrationPreparation.busy
                                 && !nativeRegistrationPublisher.busy
                                 && !nativeRegistrationPublisher.pending
                        // This single explicit click authorizes ONE irreversible
                        // registration. Async proof completion does not have
                        // permission to initiate a separate second request.
                        onClicked: root.startNativeAccountCreation()
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: !nativeRegistrationPublisher.pending
                                 && !nativeRegistrationPublisher.confirmed
                                 && nativeRegistrationStatus.existingNativeFid.length === 0
                                 && !root.nativeCreationRequested
                                 && !nativeRegistrationPreparation.busy
                                 && !nativeRegistrationPublisher.busy
                        text: qsTr("Creates one self-custody identity on HyperSnap.")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "setupRetryNativeAccountLookup"
                        visible: (nativeRegistrationStatus.accountLookupFailed
                                  || (!nativeRegistrationStatus.active
                                      && !nativeRegistrationStatus.checking))
                                 && !nativeRegistrationPublisher.pending
                                 && !nativeRegistrationPublisher.busy
                        Layout.fillWidth: true
                        compact: true
                        quiet: true
                        text: qsTrId("genycaster.empty.retry")
                        onClicked: {
                            nativeRegistrationStatus.clearExistingLookup()
                            nativeRegistrationStatus.refresh()
                            root.maybeLookupNativeFid()
                        }
                    }
                    DirectionAwareLabel {
                        objectName: "setupNativeAwaitingConfirmation"
                        Layout.fillWidth: true
                        visible: nativeRegistrationPublisher.pending
                        text: nativeRegistrationStatus.shardZeroBlockDelay > 120
                              ? qsTr("Waiting for HyperSnap to sync")
                              : qsTr("Creating your account…")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightDemiBold
                        wrapMode: Text.WordWrap
                    }
                    DirectionAwareLabel {
                        objectName: "setupNativePendingDescription"
                        Layout.fillWidth: true
                        visible: nativeRegistrationPublisher.pending
                        text: nativeRegistrationStatus.shardZeroBlockDelay > 120
                              ? qsTr("HyperSnap is catching up. Your registration is still unconfirmed; we'll keep checking without sending it again.")
                              : qsTr("Waiting for network confirmation. We'll continue automatically when your account is ready.")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }
                    DirectionAwareLabel {
                        objectName: "setupNativePreviouslyRegistered"
                        visible: nativeRegistrationStatus.existingNativeFid.length > 0
                                 && !nativeRegistrationPublisher.confirmed
                        Layout.fillWidth: true
                        text: qsTr("Account found. Signing you in…")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }
                    // When a confirmed account could not be admitted (e.g.
                    // secure-store error), show one recovery action, not the
                    // normal two-phase proof/submit interface.
                    ActionButton {
                        objectName: "setupContinueConfirmedNativeAccount"
                        visible: nativeRegistrationPublisher.confirmed
                                 && !root.authenticated
                                 && !nativeRegistrationPublisher.busy
                        Layout.fillWidth: true
                        prominent: true
                        text: qsTrId("genycaster.empty.retry")
                        enabled: root.runtimeReady
                                 && nativeRegistrationPublisher.registrationSupported
                        onClicked: nativeRegistrationPublisher.resumeConfirmedAccount()
                    }
                    ActionButton {
                        objectName: "setupRetryExistingNativeAccount"
                        visible: nativeRegistrationPublisher.existingVerificationFailed
                                 && nativeRegistrationStatus.existingNativeFid.length > 0
                                 && !nativeRegistrationPublisher.pending
                        Layout.fillWidth: true
                        prominent: true
                        text: qsTrId("genycaster.empty.retry")
                        onClicked: {
                            root.nativeExistingResumeRequested = false
                            root.resumeExistingNativeAccount(true)
                        }
                    }
                    // Diagnostics and abnormal recovery belong behind one
                    // optional, quiet disclosure; never in the main flow.
                    ActionButton {
                        objectName: "setupNativeTechnicalDetailsToggle"
                        visible: !root.authenticated
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: Theme.scaleUi(240)
                        quiet: true
                        compact: true
                        text: root.nativeDetailsExpanded
                              ? qsTr("Hide details") : qsTr("More options")
                        onClicked: root.nativeDetailsExpanded = !root.nativeDetailsExpanded
                    }
                    ColumnLayout {
                        objectName: "setupNativeTechnicalDetails"
                        Layout.fillWidth: true
                        visible: root.nativeDetailsExpanded
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: nativeRegistrationStatus.shardZeroBlockDelay >= 0
                            text: qsTr("Shard-0 block delay: %1 seconds")
                                  .arg(nativeRegistrationStatus.shardZeroBlockDelay)
                            color: nativeRegistrationStatus.shardZeroBlockDelay > 120
                                   ? Theme.warning : Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.wallet_verified")
                                  .arg(nativeCustodyWallet.address)
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WrapAnywhere
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: nativeRegistrationPublisher.pending
                                     || nativeRegistrationPublisher.failed
                                     || nativeRegistrationPublisher.outcomeUnknown
                            text: root.nativeSubmissionStatusText()
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            objectName: "setupNativeConfirmationDetails"
                            Layout.fillWidth: true
                            visible: nativeRegistrationPublisher.pending
                                     && nativeRegistrationPublisher.confirmationLookupChecks > 0
                            text: nativeRegistrationPublisher.confirmationLookupState === "not_found"
                                  ? qsTrId("genycaster.setup.native_confirmation_no_fid_yet")
                                  : qsTrId("genycaster.setup.native_existing_error")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            objectName: "setupNativeRetryWindow"
                            Layout.fillWidth: true
                            visible: nativeRegistrationPublisher.pending
                                     && nativeRegistrationPublisher.retryRemainingBlocks > 0
                            text: qsTrId("genycaster.setup.native_retry_wait_blocks")
                                  .arg(nativeRegistrationPublisher.retryRemainingBlocks)
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            objectName: "setupNativeStatusRefreshFailure"
                            Layout.fillWidth: true
                            visible: nativeRegistrationPublisher.pending
                                     && !nativeRegistrationStatus.checking
                                     && !nativeRegistrationStatus.active
                                     && nativeRegistrationStatus.error.length > 0
                            text: nativeRegistrationStatus.error
                            color: Theme.warning
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        ActionButton {
                            objectName: "setupCheckNativeRegistrationConfirmation"
                            Layout.fillWidth: true
                            compact: true
                            quiet: true
                            visible: nativeRegistrationPublisher.pending
                            busy: nativeRegistrationStatus.checking
                                  || nativeRegistrationPublisher.checkingConfirmation
                            enabled: !nativeRegistrationPublisher.busy
                                     && !nativeRegistrationStatus.checking
                            text: qsTrId("genycaster.setup.native_registration_check_again")
                            onClicked: nativeRegistrationStatus.refresh()
                        }
                        ActionButton {
                            objectName: "setupReleaseExpiredNativeAttempt"
                            Layout.fillWidth: true
                            visible: nativeRegistrationPublisher.retryReady
                                     && nativeRegistrationPublisher.pending
                            enabled: !nativeRegistrationPublisher.busy
                                     && !nativeRegistrationStatus.checking
                                     && nativeRegistrationStatus.active
                            compact: true
                            text: qsTrId("genycaster.setup.native_retry_start_over")
                            onClicked: {
                                if (nativeRegistrationPublisher.releaseExpiredAttempt()) {
                                    root.nativeCreationRequested = false
                                    nativeRegistrationPreparation.cancel()
                                    nativeRegistrationStatus.clearExistingLookup()
                                    nativeRegistrationStatus.refresh()
                                    root.registrationError = ""
                                }
                            }
                        }
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
                maximumTextWidth: root.phone ? 620 : 810
                Layout.fillWidth: true
                showIcon: false
                title: root.existingProtocolSelected || root.resumingRegistration || root.authenticated
                    ? qsTrId("genycaster.setup.design_signin_title")
                    : qsTrId("genycaster.setup.choose_protocol")
                subtitle: root.existingProtocolSelected
                    ? (root.recoveryProtocol === "hypersnap-v2" ? qsTrId("genycaster.settings.hypersnap_label")
                                                               : qsTrId("genycaster.settings.farcaster_label"))
                      + " · " + root.protocolConnectionText(root.recoveryProtocol === "farcaster-snapchain")
                    : root.resumingRegistration ? qsTrId("genycaster.setup.design_signin_description") : ""
            }
            Item { Layout.preferredHeight: Theme.space2 }
            // Only this picker chooses the protocol for Existing accounts.
            // The same stage is used on desktop, Android and WebAssembly.
            GridLayout {
                objectName: "setupExistingProtocolChoice"
                visible: !root.authenticated && !root.existingProtocolSelected
                         && !root.resumingRegistration
                Layout.fillWidth: true
                columns: root.wideStage ? 2 : 1
                uniformCellWidths: columns > 1
                layoutDirection: Theme.uiDirection
                rowSpacing: Theme.space3
                columnSpacing: Theme.space3
                SetupActionCard {
                    objectName: "setupExistingFarcaster"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onboardingStyle: true
                    networkShowcase: true
                    protocolChoice: true
                    iconGlyph: "brands/farcaster"
                    title: qsTrId("genycaster.settings.farcaster_label")
                    connectionState: root.protocolConnectionState(true)
                    connectionText: root.protocolConnectionText(true, connectionState)
                    onClicked: {
                        root.recoveryProtocol = "farcaster-snapchain"
                        root.existingProtocolSelected = true
                        root.showRecovery = false
                        stageScroll.contentY = 0
                    }
                }
                SetupActionCard {
                    objectName: "setupExistingHyperSnap"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onboardingStyle: true
                    networkShowcase: true
                    protocolChoice: true
                    iconGlyph: "quilibrium/hypersnap"
                    title: qsTrId("genycaster.settings.hypersnap_label")
                    connectionState: root.protocolConnectionState(false)
                    connectionText: root.protocolConnectionText(false, connectionState)
                    onClicked: {
                        root.recoveryProtocol = "hypersnap-v2"
                        root.existingProtocolSelected = true
                        root.showRecovery = false
                        stageScroll.contentY = 0
                    }
                }
            }
            // Local custody recovery is the preferred existing-account route.
            // Keep it first, with clear visual emphasis; the two external
            // identity methods are secondary alternatives.
            GridLayout {
                visible: !root.authenticated && root.existingProtocolSelected
                Layout.fillWidth: true
                columns: root.wideStage ? 2 : 1
                uniformCellWidths: columns > 1
                layoutDirection: Theme.uiDirection
                rowSpacing: Theme.space3
                columnSpacing: Theme.space3
                SetupActionCard {
                    Layout.fillWidth: true
                    compactCard: true
                    wrapTitle: true
                    onboardingStyle: true
                    setupEmphasis: true
                    iconGlyph: "system/lock"
                    title: qsTrId("genycaster.setup.design_recovery_title")
                    enabled: !root.authenticationInProgress && !root.recoverySubmitPending
                    onClicked: {
                        if (root.browserReadOnly) {
                            // The DOM recovery dialog is presented only on an
                            // explicit click. No phrase enters QML or C++.
                            root.recoverAccount(root.recoveryProtocol)
                        } else {
                            root.showRecovery = !root.showRecovery
                        }
                    }
                }
                SetupActionCard {
                    visible: root.recoveryProtocol === "hypersnap-v2" && !root.showRecovery
                    Layout.fillWidth: true
                    compactCard: true
                    wrapTitle: true
                    onboardingStyle: true
                    brandImageSource: (Qt.platform.os === "android" ? "image://genycaster-brand/mark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg")
                    title: qsTrId("genycaster.shell.continue_with_genyleap")
                    busy: root.authenticationInProgress && root.authenticationRoute === "openproof"
                    enabled: root.runtimeReady && root.authenticationAvailable && !root.authenticationInProgress
                    onClicked: {
                        root.continueWithGenyleap()
                        root.switchStep(5)
                    }
                }
                SetupActionCard {
                    Layout.fillWidth: true
                    compactCard: true
                    wrapTitle: true
                    onboardingStyle: true
                    visible: root.recoveryProtocol === "farcaster-snapchain" && !root.showRecovery
                    iconGlyph: "brands/farcaster"
                    title: qsTrId("genycaster.shell.sign_in_with_farcaster")
                    busy: root.authenticationInProgress && root.authenticationRoute === "farcaster"
                    enabled: root.runtimeReady && root.authenticationAvailable && !root.authenticationInProgress
                    onClicked: {
                        root.continueWithFarcaster()
                        root.switchStep(5)
                    }
                }
            }
            SetupConnectionPanel {
                objectName: "setupExistingConnections"
                protocol: root.existingProtocolSelected ? root.recoveryProtocol : ""
                Layout.fillWidth: true
                visible: root.showsSetupConnectivity && !root.showRecovery
            }
            ColumnLayout {
                visible: root.showRecovery && (root.existingProtocolSelected || root.resumingRegistration)
                    && !root.browserReadOnly && !root.authenticated
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                spacing: Theme.space3
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.recoveryProtocol === "hypersnap-v2"
                        ? qsTrId("genycaster.setup.native_recovery_privacy")
                        : qsTrId("genycaster.setup.recovery_privacy")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.native_recovery_placeholder")
                        color: Theme.ink
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Theme.textStartAlignment
                    }
                    DirectionAwareLabel {
                        text: LocaleFormat.localizeDigits(nativeRecoveryGrid.completeWordCount
                                                          + " / 24")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightMedium
                    }
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
                        deviceSignerConsent.checked = false
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
                            delegate: DirectionAwareTextField {
                                id: wordField
                                forceLeftToRight: true
                                required property int index
                                objectName: "nativeRecoveryWord" + (index + 1)
                                Layout.fillWidth: true
                                Layout.minimumWidth: root.phone ? 100 : 112
                                implicitHeight: root.phone ? 54 : 52
                                maximumLength: 4095
                                echoMode: revealWords.checked ? TextInput.Normal : TextInput.Password
                                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                                enabled: !root.authenticationInProgress && !root.recoverySubmitPending
                                horizontalAlignment: TextInput.AlignLeft
                                color: Theme.ink
                                font.pixelSize: Theme.typeBodySize
                                leftPadding: Theme.space6 + Theme.space3
                                rightPadding: Theme.space2
                                placeholderText: ""
                                Accessible.name: qsTrId("genycaster.setup.native_recovery_placeholder")
                                                 + " " + (index + 1)
                                HoverHandler {
                                    id: recoveryWordPointer
                                    enabled: Theme.neonEnabled && wordField.enabled
                                    blocking: false
                                }
                                background: NeonSurface {
                                    interactive: wordField.enabled
                                    deferredNeon: true
                                    ambientNeon: wordField.activeFocus
                                    focused: wordField.activeFocus
                                    externalHovered: wordField.hovered
                                    externalPointerX: recoveryWordPointer.hovered
                                        ? recoveryWordPointer.point.position.x : width / 2
                                    externalPointerY: recoveryWordPointer.hovered
                                        ? recoveryWordPointer.point.position.y : height / 2
                                    neonStrength: Theme.neonControlStrength
                                    fillColor: wordField.activeFocus
                                               ? Theme.surfaceHover : Theme.surface
                                    borderWidth: wordField.activeFocus ? 2 : Theme.strokeStandard
                                    borderColor: wordField.activeFocus ? Theme.focus : Theme.borderMuted
                                    curveDepth: Theme.curveControl
                                    // Unlike placeholder numbers, these remain visible
                                    // after entry and while the words are concealed.
                                    DirectionAwareLabel {
                                        anchors.left: parent.left
                                        anchors.leftMargin: Theme.space3
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: String(wordField.index + 1).padStart(2, "0")
                                        color: wordField.activeFocus ? Theme.ink : Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightMedium
                                    }
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
                // Recovery choices form one aligned vertical control group.
                // The full labels remain tap targets without selected cards.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space3
                    spacing: Theme.space1
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.strokeStandard
                        Layout.bottomMargin: Theme.space1
                        color: Theme.borderMuted
                    }
                    GenyCheckRow {
                        id: revealWords
                        objectName: "nativeRecoveryRevealWords"
                        Layout.fillWidth: true
                        label: qsTrId("genycaster.setup.show_recovery_words")
                        onVisibleChanged: if (!visible) checked = false
                    }
                    GenyCheckRow {
                        id: deviceSignerConsent
                        objectName: "nativeRecoveryDeviceSignerConsent"
                        Layout.fillWidth: true
                        label: qsTrId("genycaster.setup.recovery_signer_consent")
                        // Recovering an existing FID is independent of publishing authority.
                        // Device signer authorization is a separate, explicit
                        // opt-in, never a prerequisite for successful recovery.
                        checked: false
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
                        // Snapshot explicit device-signer consent BEFORE
                        // clearing the sensitive form: clearAll() resets its
                        // checkbox, and previously always sent false.
                        const authorizeDeviceSigner = deviceSignerConsent.checked
                        nativeRecoveryGrid.clearAll()
                        if (root.resumingRegistration) {
                            const restored =
                                nativeCustodyWallet.restoreForRegistration(words)
                            words = ""
                            if (restored) {
                                root.resumingRegistration = false
                                root.showRecovery = false
                                root.registrationError = ""
                                if (root.registrationProviderId === "hypersnap-v2")
                                    nativeRegistrationPublisher.recoverInterruptedRegistration()
                                root.switchStep(3)
                            } else {
                                root.registrationError = nativeCustodyWallet.error
                            }
                            return
                        }
                        root.recoverySubmitPending = true
                        if (root.recoveryProtocol === "hypersnap-v2")
                            root.nativeRecoverHyperSnapAccount(words, authorizeDeviceSigner)
                        else
                            root.nativeRecoverFarcasterAccount(words, authorizeDeviceSigner)
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
                visible: root.authenticated && root.authenticationRoute === "farcaster"
                         && root.accountIdentityIssuer === deploymentEndpoints.openProofIssuer
                Layout.fillWidth: true
                text: qsTrId("genycaster.shell.farcaster_sign_in_openproof_verification")
                color: Theme.accent
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.authenticated
                    ? qsTrId("genycaster.setup.account_verified_body")
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
                enabled: !root.recoverySubmitPending
                         && !root.authenticationInProgress
                onClicked: {
                    if (!root.recoverySubmitPending && !root.authenticationInProgress)
                        root.switchStep(6)
                }
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
            DirectionAwareLabel {
                objectName: "setupSignerVerificationStatus"
                visible: root.reviewMode && root.authenticated && !root.signerReady
                         && root.signerStatusText.length > 0
                Layout.fillWidth: true
                text: root.signerStatusText
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            SetupActionCard {
                visible: root.reviewMode && root.authenticated && !root.signerReady
                Layout.fillWidth: true
                compactCard: true
                title: qsTrId("genycaster.shell.enable_interactions")
                subtitle: qsTrId("genycaster.setup.recovery_signer_consent")
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
                    root.recoverySubmitPending = false
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
            // A rejected Farcaster request is NOT a prerequisite for using
            // a separately verified Genyleap account. Provide the direct
            // alternative here without silently bypassing either proof.
            SetupActionCard {
                objectName: "setupRetryWithGenyleapAfterFarcaster"
                visible: !root.authenticated && !root.authenticationInProgress
                         && root.authenticationRoute === "farcaster"
                         && root.authenticationPhase === "failed"
                Layout.fillWidth: true
                compactCard: true
                onboardingStyle: true
                brandImageSource: (Qt.platform.os === "android" ? "image://genycaster-brand/mark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg")
                title: qsTrId("genycaster.shell.continue_with_genyleap")
                enabled: root.runtimeReady && root.authenticationAvailable
                onClicked: root.continueWithGenyleap()
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
                maximumTextWidth: root.phone ? 620 : 810
                Layout.fillWidth: true
                iconGlyph: Theme.iconPalette
                title: qsTrId("genycaster.setup.design_appearance_title")
                subtitle: qsTrId("genycaster.setup.design_appearance_description")
            }
            Item { Layout.preferredHeight: Theme.space4 }
            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : root.wideStage ? 3 : 2
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
                        background: NeonCard {
                            interactive: appearanceChoice.enabled
                            ambientNeon: appearanceChoice.chosen
                            focused: Theme.keyboardFocusVisible(appearanceChoice)
                            externalHovered: appearanceChoice.hovered
                            neonStrength: Theme.neonCardStrength
                            fillColor: appearanceChoice.chosen ? Theme.surfaceMuted : Theme.surface
                            borderWidth: appearanceChoice.chosen ? 2 : 1
                            borderColor: appearanceChoice.chosen ? Theme.accent : Theme.borderMuted
                            curveDepth: Theme.curveContainer
                        }
                        contentItem: Item {
                            RowLayout {
                                layoutDirection: Theme.uiDirection
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

            // OS-branded accent is a separate choice from System/Dark/Light.
            // Only selecting it changes the color preset; appearance mode
            // remains independent and user overrides stay persistent.
            Button {
                id: setupPlatformColor
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                implicitHeight: 76
                focusPolicy: Qt.StrongFocus
                readonly property bool chosen: appearanceController.colorThemeId === "platform"
                Accessible.name: Theme.platformName + " " + Theme.platformAccentHex
                Accessible.checked: chosen
                onClicked: appearanceController.setColorThemeId("platform")
                background: NeonCard {
                    interactive: setupPlatformColor.enabled
                    ambientNeon: setupPlatformColor.chosen
                    focused: Theme.keyboardFocusVisible(setupPlatformColor)
                    externalHovered: setupPlatformColor.hovered
                    neonStrength: Theme.neonCardStrength
                    fillColor: setupPlatformColor.chosen ? Theme.surfaceSelected : Theme.surfaceMuted
                    borderWidth: setupPlatformColor.chosen ? 2 : Theme.strokeHairline
                    borderColor: setupPlatformColor.chosen ? Theme.accent : Theme.borderSubtle
                    curveDepth: Theme.curveContainer
                }
                contentItem: RowLayout {
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space3
                    Item {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        Superellipse {
                            anchors.fill: parent
                            curveDepth: 14
                            fillColor: Theme.platformAccentColor
                        }
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.platformIcon
                            color: Theme.contrastInk(Theme.platformAccentColor)
                            preferredSize: 24
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: Theme.platformName
                            color: Theme.ink
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: Theme.platformAccentHex.toUpperCase()
                            color: Theme.inkMuted
                            font.family: Theme.latinFontFamily
                            font.pixelSize: Theme.typeCaptionSize
                            forceLeftToRight: true
                        }
                    }
                    Icon {
                        visible: setupPlatformColor.chosen
                        text: Theme.iconCheck
                        color: Theme.accent
                        preferredSize: 21
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
                NeonSurface {
                    id: completionBadge
                    interactive: false
                    ambientNeon: true
                    inverted: true
                    neonStrength: Theme.neonCardStrength
                    width: 66
                    height: 66
                    anchors.centerIn: parent
                    curveDepth: 20
                    fillColor: Theme.accent
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconCheck
                        color: Theme.accentSurfaceInk
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
            DirectionAwareLabel {
                objectName: "registrationProviderRestartNotice"
                visible: root.accountIntent === "new" && providerProfiles.readRestartRequired
                Layout.fillWidth: true
                Layout.topMargin: Theme.space3
                text: qsTrId("genycaster.setup.registration_restart_notice")
                color: Theme.warning
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
