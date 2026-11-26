import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Farcaster.Client

Item {
    id: root
    objectName: "appShell"

    property var pendingBuzz: ({})
    property string lastDisplayedBuzzId: ""
    property double buzzPendingSendFid: 0
    property string buzzFeedbackCode: ""
    function showNextBuzz() {
        if (!root.visible || initialSetup.visible || genyBuzzToast.opened
            || !openProofConnections || !openProofConnections.cloudAuthorizationReady
            || !openProofConnections.genyBuzzPreferencesReady
            || openProofConnections.genyBuzzPreferences.enabled !== true)
            return
        const rows = openProofConnections.genyBuzzInbox || []
        for (const event of rows) {
            if (!event || !event.id || String(event.id) === root.lastDisplayedBuzzId)
                continue
            root.pendingBuzz = event
            root.lastDisplayedBuzzId = String(event.id)
            genyBuzzToast.open()
            // Explicitly acknowledge only an item shown to this user.
            openProofConnections.acknowledgeGenyBuzz(String(event.id))
            break
        }
    }

    function readingOverlayInset(surface) {
        if (mobileBottomNavigation.visible)
            return mobileBottomNavigation.height
        if (!floatingCompose.visible)
            return 0
        // Both controls need separate hit areas. Translate the compose top
        // into the reading viewport; shell footers and rails have other bounds.
        const composeTop = root.mapToItem(surface, floatingCompose.x, floatingCompose.y).y
        return Math.max(0, surface.height - composeTop)
    }

    // Signing in establishes an identity. Authorizing a device signer is a
    // separate consent boundary, and MUST use the account's registry origin.
    function requestInteractionAuthorization() {
        if (!presentationBridge.isAuthenticated || presentationBridge.signerBusy
            || presentationBridge.writeReady) return
        // Skip a doomed external approval attempt when this process no
        // longer holds the original local custody wallet.
        // The explicit recovery editor is shared with WASM. Its adapter
        // verifies the same signed-in principal before granting authority.
        interactionAuthorizationDialog.enteringNativeWords =
            Qt.platform.os === "wasm"
            || ((presentationBridge.nativeCustodyAccount
                 && !presentationBridge.nativeCustodySignerAvailable)
                || presentationBridge.farcasterCustodyAccount
                // OpenProof's Farcaster-linked login verifies identity, not
                // write authority. The already-entered/recovered custody
                // mnemonic can authorize a local signer here as well; it is
                // not necessary to redirect native users to the Farcaster app.
                || presentationBridge.authenticationRoute === "farcaster")
        interactionAuthorizationDialog.open()
    }

    // Native Back must dismiss the topmost transient surface before navigating
    // the content history. Reuse the same hierarchy on Android and future iOS
    // hosts; do not discard an in-progress Cast while a nested picker is open.
    function handlePlatformBack() {
        if (interactionAuthorizationDialog.opened) {
            interactionAuthorizationDialog.close()
            root.pendingComposerIntent = null
            return true
        }
        if (localeSettingsOpen) {
            const settings = localePopupLoader.item
            if (settings.mobilePresentation && !settings.mobileIndexOpen)
                settings.mobileIndexOpen = true
            else
                settings.close()
            return true
        }
        if (dataStorageSettingsOpen) { dataStoragePopupLoader.item.close(); return true }
        if (composerDrawer.opened) { composerDrawer.dismissTopLayer(); return true }
        if (mediaGallery.opened) { mediaGallery.close(); return true }
        if (mobileCreateMenu.opened) { mobileCreateMenu.close(); return true }
        if (searchPalette.opened) { searchPalette.close(); return true }
        if (accountPopup.opened) { accountPopup.close(); return true }
        if (genyProDetailsDrawer.opened) { genyProDetailsDrawer.close(); return true }
        if (contextDrawer.opened) { contextDrawer.close(); return true }
        if (renderingSettingsPopup.opened) { renderingSettingsPopup.close(); return true }
        if (taskManagerPopup.opened) { taskManagerPopup.close(); return true }
        if (feedbackDialog.opened) { feedbackDialog.close(); return true }
        if (signOutConfirm.opened) { signOutConfirm.close(); return true }
        if (releaseNotice.opened) { releaseNotice.close(); return true }
        if (root.initialSetupOpen) {
            if (root.setupReviewOpen)
                root.setupReviewOpen = false
            else
                initialSetup.previousStep()
            return true
        }
        if (root.topicBrowseActive) { root.closeTopicBrowser(); return true }
        if (presentationBridge.activeView === "profile") {
            root.returnFromProfileSurface()
            return true
        }
        if (presentationBridge.activeView === "cast") {
            root.returnFromCastDetail()
            return true
        }
        if (root.shellRoute.length > 0) { root.shellRoute = ""; return true }
        if (root.primaryDestination !== "home") {
            root.primaryDestination = "home"
            return true
        }
        return false
    }

    GenyModal {
        id: interactionAuthorizationDialog
        objectName: "interactionAuthorizationExplanation"
        parent: Overlay.overlay
        blurSourceItem: Qt.platform.os === "android" || Qt.platform.os === "ios"
            ? null : shellContent
        // Narrow native desktop windows are mobile layouts too. Present
        // this recovery editor edge-to-edge instead of a centered popup.
        fullScreen: root.compact
        slideFromBottom: fullScreen
        readonly property bool nativeAccount:
            presentationBridge.nativeCustodyAccount
        property bool enteringNativeWords: false
        property bool authorizationSubmitted: false
        property string authorizationPrincipal: ""
        property bool recoveryValidationVisible: false
        readonly property bool mobileRecoveryInput: Qt.platform.os === "android"
                                                   || Qt.platform.os === "ios"
        property bool showRecoveryWords: false
        property int recoveryRevision: 0
        readonly property bool acceptsLegacy12: !nativeAccount && enteringNativeWords
        property bool force24Words: false
        property int activeWordCount: 24
        function refreshRecoveryWordMode() {
            if (distributingRecoveryWords) return
            if (mobileRecoveryInput) {
                const raw = String(nativeSignerPhraseInput.text || "").trim()
                const words = raw ? raw.split(/\s+/) : []
                activeWordCount = acceptsLegacy12 && words.length === 12 ? 12 : 24
            } else {
                let first12 = true
                let trailing = false
                for (let i = 0; i < 24; ++i) {
                    const filled = recoveryWord(i).length > 0
                    if (i < 12 && !filled) first12 = false
                    if (i >= 12 && filled) trailing = true
                }
                activeWordCount = acceptsLegacy12 && !force24Words
                    && first12 && !trailing ? 12 : 24
            }
            ++recoveryRevision
        }
        function recoveryWord(index) {
            const input = nativeSignerWordGrid.itemAt(index)
            return input ? String(input.word || "").trim().toLowerCase() : ""
        }
        function recoveryPhrase() {
            if (mobileRecoveryInput)
                return String(nativeSignerPhraseInput.text || "")
                    .trim().toLowerCase().replace(/\s+/g, " ")
            const words = []
            for (let i = 0; i < activeWordCount; ++i) words.push(recoveryWord(i))
            return words.join(" ")
        }
        readonly property int enteredWordCount: {
            const revision = recoveryRevision
            if (mobileRecoveryInput) {
                const phrase = recoveryPhrase()
                return phrase.length ? phrase.split(" ").length : 0
            }
            let count = 0
            for (let i = 0; i < 24; ++i)
                if (recoveryWord(i).length > 0) ++count
            return count
        }
        readonly property string recoveryInstruction: acceptsLegacy12
            ? qsTrId("genycaster.setup.farcaster_recovery_placeholder")
            : qsTrId("genycaster.setup.native_recovery_placeholder")
        function recoveryReady() {
            // Bind the button to each field or the single secure mobile
            // phrase entry; never submit a partial recovery phrase.
            const revision = recoveryRevision
            if (mobileRecoveryInput) {
                const phrase = recoveryPhrase()
                const words = phrase.length > 0 ? phrase.split(" ") : []
                return (words.length === 24
                        || (acceptsLegacy12 && words.length === 12))
                    && words.every(word => /^[a-z]+$/.test(word))
            }
            for (let i = 0; i < activeWordCount; ++i)
                if (!/^[a-z]+$/.test(recoveryWord(i))) return false
            for (let i = activeWordCount; i < 24; ++i)
                if (recoveryWord(i).length > 0) return false
            return true
        }
        function clearRecoveryWords() {
            distributingRecoveryWords = true
            nativeSignerPhraseInput.cancelClipboardPaste()
            nativeSignerPhraseInput.text = ""
            for (let i = 0; i < 24; ++i) {
                const input = nativeSignerWordGrid.itemAt(i)
                if (input) {
                    input.cancelWordPaste()
                    input.word = ""
                }
            }
            distributingRecoveryWords = false
            force24Words = false
            activeWordCount = 24
            showRecoveryWords = false
            ++recoveryRevision
        }
        property bool distributingRecoveryWords: false
        function distributeRecoveryWords(start, pasted) {
            if (distributingRecoveryWords || start < 0 || start >= 24) return
            // Qt TextField may emit textChanged several times during a paste.
            // Parse only the settled value; never change focus while distributing.
            const incoming = String(pasted)
            const normalized = (typeof incoming.normalize === "function"
                ? incoming.normalize("NFKC") : incoming).toLowerCase()
                .replace(/[\u200b-\u200d\u2060\ufeff]/g, " ")
                .replace(/\b\d{1,2}\s*[.):\-]\s*(?=[a-z])/g, " ")
                .replace(/[^a-z]+/g, " ").trim()
            if (!normalized) return
            const words = normalized.split(/\s+/)
            if (words.length < 1 || words.length > 24) return
            // A complete phrase always starts at 01, regardless of which
            // numbered field received the clipboard event.
            const complete = words.length === 24
                || (acceptsLegacy12 && words.length === 12)
            const offset = complete ? 0 : start
            if (offset + words.length > 24) return
            distributingRecoveryWords = true
            try {
                if (complete) {
                    // Replace, never merge, a complete pasted phrase. This
                    // removes stale word slots from prior recovery attempts.
                    for (let i = 0; i < 24; ++i) {
                        const field = nativeSignerWordGrid.itemAt(i)
                        if (field) field.word = ""
                    }
                    force24Words = words.length === 24
                    activeWordCount = words.length
                }
                for (let i = 0; i < words.length; ++i) {
                    const field = nativeSignerWordGrid.itemAt(offset + i)
                    if (field) field.word = words[i]
                }
            } finally {
                distributingRecoveryWords = false
            }
            refreshRecoveryWordMode()
        }
        onOpened: {
            authorizationPrincipal = presentationBridge.accountIdentityIssuer + ":" + presentationBridge.accountFidText
            authorizationSubmitted = false
            recoveryValidationVisible = false
            if (presentationBridge.writeReady) {
                close()
                return
            }
            // Settings can open the recovery editor directly. Give the
            // newly mounted mobile input the focus after Popup relocation.
            if (enteringNativeWords && mobileRecoveryInput)
                Qt.callLater(function() {
                    if (!interactionAuthorizationDialog
                            || !interactionAuthorizationDialog.opened) return
                    nativeSignerPhraseInput.focusEditing()
                    if (Qt.platform.os !== "wasm") Qt.inputMethod.show()
                })
        }
        onAboutToHide: {
            nativeSignerPhraseInput.dismissSelectionActions()
            for (let i = 0; i < 24; ++i)
                nativeSignerWordGrid.itemAt(i)?.dismissWordEditor()
        }
        onClosed: {
            if (enteringNativeWords && !presentationBridge.writeReady)
                authorizationSubmitted = false
            if (!authorizationSubmitted && !presentationBridge.writeReady)
                root.pendingComposerIntent = null
            clearRecoveryWords()
            authorizationPrincipal = ""
            recoveryValidationVisible = false
            enteringNativeWords = false
        }
        width: fullScreen ? (parent ? parent.width : root.width)
              : (parent ? Math.min(760, parent.width - Theme.space4 * 2) : 720)
        height: fullScreen ? (parent ? parent.height : root.height)
               : (parent ? Math.min(parent.height - Theme.space4 * 2,
                                   confirmationContent.implicitHeight + Theme.scaleUi(94))
                         : confirmationContent.implicitHeight + Theme.scaleUi(94))
        padding: fullScreen ? Theme.mobileGutter : Theme.space4
        Item {
            anchors.fill: parent
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.space3
                ScrollView {
                    id: interactionAuthorizationScroller
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ColumnLayout {
                        id: confirmationContent
                        width: interactionAuthorizationScroller ? interactionAuthorizationScroller.availableWidth : 0
                        spacing: Theme.space3
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.shell.enable_interactions")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
                wrapMode: Text.Wrap
            }
            DirectionAwareLabel {
                visible: !interactionAuthorizationDialog.enteringNativeWords
                Layout.fillWidth: true
                text: interactionAuthorizationDialog.nativeAccount
                    ? qsTrId("genycaster.shell.native_signer_description")
                    : qsTrId("genycaster.shell.farcaster_sign_in_openproof_description")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeBodySize
                wrapMode: Text.WordWrap
            }
            // The phrase is a high-trust action, not another neutral form hint.
            // Use the same Neon material as App Center; warning is semantic,
            // not an always-running animation.
            NeonSurface {
                objectName: "signerRecoverySecurityNotice"
                visible: interactionAuthorizationDialog.enteringNativeWords
                Layout.fillWidth: true
                Layout.preferredHeight: signerSecurityContent.implicitHeight + Theme.space3 * 2
                fillColor: Theme.warningSurface
                borderColor: Qt.rgba(Theme.warning.r, Theme.warning.g,
                                     Theme.warning.b, 0.42)
                borderWidth: Theme.strokeStandard
                curveDepth: Theme.curveContainer
                ColumnLayout {
                    id: signerSecurityContent
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2
                        Icon {
                            text: Theme.iconWarning
                            color: Theme.warning
                            preferredSize: Theme.iconSize
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.recovery_notice_title")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            wrapMode: Text.WordWrap
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        selectableText: false
                        text: interactionAuthorizationDialog.nativeAccount
                            ? qsTrId("genycaster.setup.native_recovery_privacy")
                            : qsTrId("genycaster.setup.recovery_privacy")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
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
                    preferredSize: Theme.iconSize
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.recovery_signer_consent")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    wrapMode: Text.WordWrap
                }
            }
            ActionButton {
                objectName: "nativeSignerUseRecoveryWords"
                visible: !interactionAuthorizationDialog.enteringNativeWords
                Layout.fillWidth: true
                compact: true
                text: qsTrId("genycaster.shell.native_signer_use_words")
                onClicked: {
                    interactionAuthorizationDialog.enteringNativeWords = true
                    // The grid/field was hidden at pointer-down time. Wait
                    // until the visible editor has been laid out before
                    // focusing it, so Android's IME can actually open.
                    Qt.callLater(function() {
                        if (!interactionAuthorizationDialog.opened) return
                        if (interactionAuthorizationDialog.mobileRecoveryInput) {
                            nativeSignerPhraseInput.focusEditing()
                            if (Qt.platform.os !== "wasm") Qt.inputMethod.show()
                        } else {
                            const first = nativeSignerWordGrid.itemAt(0)
                            if (first) first.focusWord()
                        }
                    })
                }
            }
            DirectionAwareLabel {
                visible: interactionAuthorizationDialog.enteringNativeWords
                         && interactionAuthorizationDialog.nativeAccount
                Layout.fillWidth: true
                text: qsTrId("genycaster.shell.native_signer_words_note")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
            RowLayout {
                visible: interactionAuthorizationDialog.enteringNativeWords
                         && !interactionAuthorizationDialog.authorizationSubmitted
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: interactionAuthorizationDialog.recoveryInstruction
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                DirectionAwareLabel {
                    objectName: "signerRecoveryWordCount"
                    text: LocaleFormat.localizeDigits(interactionAuthorizationDialog.enteredWordCount
                          + " / " + interactionAuthorizationDialog.activeWordCount)
                    color: interactionAuthorizationDialog.recoveryReady() ? Theme.accent : Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }
            }
            // On phones a single, real TextInput accepts either keyboard
            // entry or pasting all 24 words. Nested ScrollViews containing 24
            // Password fields could steal the pointer/IME focus on Android.
            DirectionAwareTextField {
                id: nativeSignerPhraseInput
                objectName: "nativeSignerRecoveryPhraseInput"
                visible: interactionAuthorizationDialog.mobileRecoveryInput
                         && interactionAuthorizationDialog.enteringNativeWords
                Layout.fillWidth: true
                enabled: !interactionAuthorizationDialog.authorizationSubmitted
                forceLeftToRight: true
                placeholderText: interactionAuthorizationDialog.authorizationSubmitted
                    ? "" : interactionAuthorizationDialog.recoveryInstruction
                Layout.preferredHeight: Theme.scaleUi(44)
                font.pixelSize: Theme.typeCaptionSize
                echoMode: interactionAuthorizationDialog.showRecoveryWords
                    ? TextInput.Normal : TextInput.Password
                maximumLength: 512
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                                  | Qt.ImhNoAutoUppercase | Qt.ImhHiddenText
                onTextChanged: {
                    if (interactionAuthorizationDialog)
                        interactionAuthorizationDialog.refreshRecoveryWordMode()
                }
            }
            GridLayout {
                visible: interactionAuthorizationDialog.enteringNativeWords
                         && !interactionAuthorizationDialog.authorizationSubmitted
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                columns: width >= Theme.scaleUi(300) ? 2 : 1
                columnSpacing: Theme.space2
                rowSpacing: Theme.space1
                ActionButton {
                    objectName: "signerPasteRecoveryWords"
                    Layout.fillWidth: true
                    enabled: !interactionAuthorizationDialog.authorizationSubmitted && !presentationBridge.signerBusy
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.editor.paste")
                    onClicked: {
                        if (interactionAuthorizationDialog.mobileRecoveryInput)
                            nativeSignerPhraseInput.requestClipboardPaste()
                        else
                            nativeSignerWordGrid.itemAt(0)?.pasteWord()
                    }
                }
                ActionButton {
                    objectName: "signerToggleRecoveryVisibility"
                    Layout.fillWidth: true
                    enabled: !interactionAuthorizationDialog.authorizationSubmitted && !presentationBridge.signerBusy
                    compact: true
                    quiet: true
                    text: interactionAuthorizationDialog.showRecoveryWords
                          ? qsTrId("genycaster.shell.hide_recovery_words")
                          : qsTrId("genycaster.setup.show_recovery_words")
                    onClicked: interactionAuthorizationDialog.showRecoveryWords =
                        !interactionAuthorizationDialog.showRecoveryWords
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: interactionAuthorizationDialog.enteringNativeWords
                    && (nativeSignerPhraseInput.clipboardPasteFailed
                        || nativeSignerWordGrid.itemAt(0)?.pasteFailed === true)
                text: qsTrId("genycaster.setup.clipboard_paste_unavailable")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
            Item {
                id: nativeRecoveryScroll
                objectName: "nativeSignerRecoveryGrid"
                visible: !interactionAuthorizationDialog.mobileRecoveryInput
                         && interactionAuthorizationDialog.enteringNativeWords
                Layout.fillWidth: true
                Layout.preferredHeight: nativeSignerWordColumns.implicitHeight
                // One scrolling owner: the outer form. Never clip or scroll
                // the 24-word grid independently; all cells remain laid out.
                GridLayout {
                    id: nativeSignerWordColumns
                    layoutDirection: Theme.uiDirection
                    width: nativeRecoveryScroll ? nativeRecoveryScroll.width : 0
                    columns: !nativeRecoveryScroll || nativeRecoveryScroll.width < Theme.scaleUi(360) ? 2
                             : nativeRecoveryScroll.width < Theme.scaleUi(520) ? 3 : 4
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space2
                    Repeater {
                        id: nativeSignerWordGrid
                        model: 24
                        delegate: ColumnLayout {
                            required property int index
                            property alias word: recoveryWordInput.text
                            function focusWord() { recoveryWordInput.focusEditing() }
                            function pasteWord() { recoveryWordInput.requestClipboardPaste() }
                            function cancelWordPaste() { recoveryWordInput.cancelClipboardPaste() }
                            function dismissWordEditor() { recoveryWordInput.dismissSelectionActions() }
                            readonly property bool pasteFailed: recoveryWordInput.clipboardPasteFailed
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space1
                            DirectionAwareLabel {
                                text: String(index + 1).padStart(2, "0")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            DirectionAwareTextField {
                                id: recoveryWordInput
                                objectName: "nativeSignerRecoveryWord" + (index + 1)
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.compact ? Theme.scaleUi(38) : Theme.scaleUi(44)
                                // Popup parent reassignment can briefly destroy the
                                // dialog while repeater delegates still evaluate bindings.
                                enabled: !!interactionAuthorizationDialog
                                    && index < interactionAuthorizationDialog.activeWordCount
                                    && !interactionAuthorizationDialog.authorizationSubmitted
                                opacity: interactionAuthorizationDialog
                                    && index < interactionAuthorizationDialog.activeWordCount ? 1 : 0.38
                                forceLeftToRight: true
                                placeholderText: interactionAuthorizationDialog
                                                 && interactionAuthorizationDialog.authorizationSubmitted
                                    ? "•••••" : ""
                                echoMode: interactionAuthorizationDialog && interactionAuthorizationDialog.showRecoveryWords
                                          ? TextInput.Normal : TextInput.Password
                                maximumLength: 4095
                                clipboardPasteHandler: value => interactionAuthorizationDialog.distributeRecoveryWords(index, value)
                                inputMethodHints: Qt.ImhSensitiveData
                                                  | Qt.ImhNoPredictiveText
                                                  | Qt.ImhNoAutoUppercase
                                Accessible.name: qsTrId("genycaster.setup.native_recovery_placeholder")
                                                 + " " + (index + 1)
                                onTextChanged: {
                                    if (!interactionAuthorizationDialog) return
                                    interactionAuthorizationDialog.refreshRecoveryWordMode()
                                    if (interactionAuthorizationDialog.distributingRecoveryWords
                                        || !/[\s,;|]/.test(text)) return
                                    // Defer until Qt finishes the clipboard/IME insertion.
                                    // Running synchronously could redistribute a partial
                                    // paste and steal the focus from field 01.
                                    Qt.callLater(function() {
                                        if (!interactionAuthorizationDialog
                                            || !interactionAuthorizationDialog.opened
                                            || interactionAuthorizationDialog.distributingRecoveryWords
                                            || !/[\s,;|]/.test(recoveryWordInput.text)) return
                                        interactionAuthorizationDialog.distributeRecoveryWords(
                                            index, recoveryWordInput.text)
                                    })
                                }
                                Keys.onPressed: function(event) {
                                    if (!interactionAuthorizationDialog) return
                                    // Space/Return explicitly advances one word. Paste
                                    // must never advance focus to another numbered slot.
                                    if ((event.key === Qt.Key_Space || event.key === Qt.Key_Return)
                                        && index < interactionAuthorizationDialog.activeWordCount - 1
                                        && !/[\s,;|]/.test(recoveryWordInput.text)) {
                                        event.accepted = true
                                        const next = nativeSignerWordGrid.itemAt(index + 1)
                                        if (next) next.focusWord()
                                    } else if (event.key === Qt.Key_Backspace
                                               && !recoveryWordInput.text && index > 0) {
                                        const previous = nativeSignerWordGrid.itemAt(index - 1)
                                        if (previous) previous.focusWord()
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ActionButton {
                objectName: "farcasterSignerRecoveryExpand24"
                visible: interactionAuthorizationDialog.acceptsLegacy12
                    && !interactionAuthorizationDialog.mobileRecoveryInput
                    && interactionAuthorizationDialog.activeWordCount === 12
                    && !interactionAuthorizationDialog.authorizationSubmitted
                Layout.alignment: Qt.AlignRight
                compact: true
                quiet: true
                text: qsTrId("genycaster.setup.recovery_use_24_words")
                onClicked: {
                    interactionAuthorizationDialog.force24Words = true
                    interactionAuthorizationDialog.refreshRecoveryWordMode()
                    nativeSignerWordGrid.itemAt(12)?.focusWord()
                }
            }
                    }
                }
            RowLayout {
                objectName: "signerAuthorizationInlineError"
                visible: interactionAuthorizationDialog.authorizationSubmitted
                         && !presentationBridge.signerBusy
                         && !presentationBridge.writeReady
                         && presentationBridge.signerErrorText.length > 0
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Icon {
                    text: Theme.iconWarning
                    color: Theme.warning
                    preferredSize: Theme.iconSize
                    Layout.alignment: Qt.AlignTop
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: String(presentationBridge.signerErrorText || "")
                    color: Theme.warning
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
            }
            // Account/FID recovery has already completed. If the separate
            // KEY_ADD signer operation stalls, inspect its CURRENT network
            // state rather than requesting another irreversible signature.
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: interactionAuthorizationDialog.authorizationSubmitted
                         && presentationBridge.isAuthenticated
                         && !presentationBridge.signerBusy
                         && !presentationBridge.writeReady
                         && presentationBridge.signerErrorText.length > 0
                text: qsTrId("genycaster.setup.signed_in_signer_separate")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
            DirectionAwareLabel {
                objectName: "signerRecoveryValidationHint"
                visible: interactionAuthorizationDialog.recoveryValidationVisible
                         && !interactionAuthorizationDialog.authorizationSubmitted
                         && !interactionAuthorizationDialog.recoveryReady()
                Layout.fillWidth: true
                text: interactionAuthorizationDialog.acceptsLegacy12
                    ? interactionAuthorizationDialog.recoveryInstruction
                    : qsTrId("genycaster.setup.signer_enter_all_words")
                color: Theme.warning
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
            ActionButton {
                objectName: "signerReenterRecoveryWords"
                Layout.fillWidth: true
                compact: true
                quiet: true
                // An unsuccessful network operation must not strand the user
                // behind 24 disabled placeholders. Reentry is explicit, never
                // an automatic KEY_ADD replay (especially on rate limits).
                visible: interactionAuthorizationDialog.authorizationSubmitted
                         && !presentationBridge.signerBusy
                         && !presentationBridge.writeReady
                         && presentationBridge.signerErrorText.length > 0
                text: qsTrId("genycaster.shell.native_signer_use_words")
                onClicked: {
                    interactionAuthorizationDialog.authorizationSubmitted = false
                    interactionAuthorizationDialog.recoveryValidationVisible = false
                    interactionAuthorizationDialog.clearRecoveryWords()
                    Qt.callLater(function() {
                        if (!interactionAuthorizationDialog || !interactionAuthorizationDialog.opened)
                            return
                        if (interactionAuthorizationDialog.mobileRecoveryInput)
                            nativeSignerPhraseInput.focusEditing()
                        else {
                            const first = nativeSignerWordGrid.itemAt(0)
                            if (first) first.focusWord()
                        }
                    })
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                ActionButton {
                    Layout.fillWidth: true
                    quiet: true
                    compact: true
                    text: presentationBridge.signerBusy
                          ? qsTrId("genycaster.shell.close")
                          : qsTrId("genycaster.shell.cancel")
                    onClicked: {
                        interactionAuthorizationDialog.clearRecoveryWords()
                        interactionAuthorizationDialog.close()
                    }
                }
                ActionButton {
                    Layout.fillWidth: true
                    prominent: !presentationBridge.writeReady
                    positive: presentationBridge.writeReady
                    compact: true
                    objectName: "authorizeSelectedProtocolSigner"
                    // ActionButton already implements the App Center Neon rim
                    // and an accessible spinner. Only animate while a real
                    // network/custody operation is in flight.
                    busy: interactionAuthorizationDialog.authorizationSubmitted
                          && presentationBridge.signerBusy
                    Accessible.description: busy
                        ? qsTrId("genycaster.setup.signer_verification_title")
                        : interactionAuthorizationDialog.authorizationSubmitted
                          && !presentationBridge.writeReady
                          ? qsTrId("genycaster.setup.signer_read_only_check")
                          : ""
                    iconGlyph: presentationBridge.writeReady ? Theme.iconCheck
                        : interactionAuthorizationDialog.nativeAccount || interactionAuthorizationDialog.enteringNativeWords
                          ? "" : Theme.iconArrowUpRight
                    text: presentationBridge.writeReady
                          ? qsTrId("genycaster.setup.signer_verified_title")
                          : interactionAuthorizationDialog.authorizationSubmitted
                            && !presentationBridge.signerBusy
                            ? qsTrId("genycaster.setup.check_signer_status")
                            : interactionAuthorizationDialog.nativeAccount
                                  || interactionAuthorizationDialog.enteringNativeWords
                              ? qsTrId("genycaster.shell.native_signer_authorize")
                              : qsTrId("genycaster.shell.open_farcaster")
                    // Always keep an actionable CTA: incomplete words reveal
                    // a validation hint; failed submissions inspect signer
                    // state instead of silently replaying a rate-limited write.
                    enabled: !presentationBridge.signerBusy
                    onClicked: {
                        if (!presentationBridge.isAuthenticated
                            || interactionAuthorizationDialog.authorizationPrincipal !==
                               presentationBridge.accountIdentityIssuer + ":" + presentationBridge.accountFidText) {
                            interactionAuthorizationDialog.close()
                            return
                        }
                        if (presentationBridge.writeReady) {
                            interactionAuthorizationDialog.close()
                            return
                        }
                        if (interactionAuthorizationDialog.authorizationSubmitted) {
                            presentationBridge.refreshSigner()
                            return
                        }
                        if (interactionAuthorizationDialog.enteringNativeWords
                                && !interactionAuthorizationDialog.recoveryReady()) {
                            interactionAuthorizationDialog.recoveryValidationVisible = true
                            return
                        }
                        interactionAuthorizationDialog.recoveryValidationVisible = false
                        interactionAuthorizationDialog.authorizationSubmitted = true
                        if (interactionAuthorizationDialog.enteringNativeWords) {
                            // One-shot input to the platform adapter. Clear
                            // the form while registry verification is pending.
                            if (interactionAuthorizationDialog.nativeAccount)
                                presentationBridge.authorizeNativeHyperSnapSigner(
                                    interactionAuthorizationDialog.recoveryPhrase())
                            else
                                presentationBridge.authorizeNativeFarcasterSigner(
                                    interactionAuthorizationDialog.recoveryPhrase())
                            interactionAuthorizationDialog.clearRecoveryWords()
                        } else {
                            // Native: uses the currently verified local custody
                            // wallet. Farcaster: retains its separate approval.
                            presentationBridge.provisionSigner()
                            interactionAuthorizationDialog.close()
                        }
                    }
                }
            }
            }
        }
    }

    GenyProDetailsDrawer {
        id: genyProDetailsDrawer
        compact: root.compact
        onAccountRequested: root.openAccountPopup()
        onSignerRequested: {
            genyProDetailsDrawer.close()
            root.requestInteractionAuthorization()
        }
    }

    readonly property bool isReady: presentationBridge.architectureState.length > 0
    // A physical handset may report its display in device pixels rather than
    // logical pixels (high-DPI Android / iOS). Never treat it as a desktop
    // merely because the reported pixel width crosses a desktop breakpoint.
    readonly property bool phonePlatform: Qt.platform.os === "android"
                                          || Qt.platform.os === "ios"
    readonly property bool largeDesktop: !phonePlatform && width >= Theme.breakpointLargeDesktop
    readonly property bool desktop: !phonePlatform && width >= Theme.breakpointCompactDesktop
    readonly property bool compactDesktop: !phonePlatform && width >= Theme.breakpointNarrow
                                           && width < Theme.breakpointCompactDesktop
    readonly property bool narrow: !phonePlatform && width >= Theme.breakpointMobile
                                   && width < Theme.breakpointNarrow
    readonly property bool mobile: phonePlatform || width < Theme.breakpointMobile
    readonly property bool wide: root.desktop
    readonly property bool compact: phonePlatform || width < Theme.breakpointNarrow
    readonly property int socialMinimumReadingWidth: 680
    readonly property bool homeSurfaceActive: root.shellRoute.length === 0
                                               && presentationBridge.activeView === "feed"
                                               && root.primaryDestination === "home"
    readonly property bool homeComposerFocused: root.mobile && homeDashboard.visible
                                                  && homeDashboard.composerFocused
    property real preKeyboardViewportHeight: 0
    property bool composerKeyboardWasVisible: false

    // A browser keyboard's Done action can dismiss the DOM keyboard without
    // removing QML activeFocus from the TextArea. The WASM loader sizes the
    // Qt canvas from visualViewport.height; a large shrink followed by a
    // return to baseline is a reliable signal to restore the normal Home.
    onHomeComposerFocusedChanged: {
        if (homeComposerFocused) {
            preKeyboardViewportHeight = height
            composerKeyboardWasVisible = false
        } else {
            preKeyboardViewportHeight = 0
            composerKeyboardWasVisible = false
        }
    }
    onHeightChanged: {
        if (!homeComposerFocused || preKeyboardViewportHeight <= 0)
            return
        if (height < preKeyboardViewportHeight - 140) {
            composerKeyboardWasVisible = true
        } else if (composerKeyboardWasVisible
                   && height >= preKeyboardViewportHeight - 72) {
            // Defer focus changes until Qt has finished its geometry pass.
            Qt.callLater(() => {
                if (root.homeComposerFocused
                        && root.height >= root.preKeyboardViewportHeight - 72)
                    homeDashboard.finishComposerEditing()
            })
        }
    }
    readonly property bool socialSurfaceActive: root.shellRoute.length === 0
                                                 && !root.homeSurfaceActive
                                                 && (presentationBridge.activeView === "feed"
                                                     || presentationBridge.activeView === "profile"
                                                     || presentationBridge.activeView === "cast")
    readonly property bool contextRailFits: root.desktop
                                            && width >= root.desktopNavigationWidth
                                                       + 304
                                                       + root.socialMinimumReadingWidth
                                                       + Theme.space6
    readonly property bool showHomeContextRail: root.contextRailFits && root.homeSurfaceActive
    // Stats uses the full reading pane while keeping card-level gutters;
    // unrelated destinations retain their original margins.
    readonly property bool homeStatsWorkspace: root.homeSurfaceActive
                                               && homeDashboard.activeTab === "stats"
    // Explorer and Profile are also full desktop workspaces. Their content
    // should use the same outside gutters as Stats, not a centred 880px
    // island surrounded by extra shell padding.
    readonly property bool expandedSocialWorkspace: root.desktop
        && root.shellRoute.length === 0
        && (presentationBridge.activeView === "profile"
            || (presentationBridge.activeView === "feed"
                && root.primaryDestination === "explorer"))
    readonly property bool expandedWorkspace: root.homeStatsWorkspace || root.expandedSocialWorkspace
    readonly property bool showSocialContextRail: root.contextRailFits && root.socialSurfaceActive
    readonly property bool showContextRail: root.showHomeContextRail || root.showSocialContextRail
    readonly property bool identityAvailable: presentationBridge.identityFid > 0
    readonly property bool passiveSignedOutDiscoveryFailure: presentationBridge.activeView === "feed"
                                                 && (presentationBridge.activeFeed === "trending"
                                                     || presentationBridge.activeFeed === "recent")
                                                 && !presentationBridge.isAuthenticated
                                                 && presentationBridge.sourceUnavailable
    readonly property int desktopNavigationWidth: root.desktop
                                                  ? Math.round(Math.min(460, Math.max(264, appearanceController.socialNavigationWidth)))
                                                  : Theme.navigationWidth
    readonly property int desktopContextRailWidth: root.desktop
                                                   ? Math.round(Math.min(
                                                       Math.min(620, Math.max(304, appearanceController.socialContextWidth)),
                                                       Math.max(304, root.width - root.desktopNavigationWidth
                                                                    - root.socialMinimumReadingWidth - Theme.space6)))
                                                   : 336
    readonly property int navigationReservation: root.desktop ? root.desktopNavigationWidth : (root.compactDesktop || root.narrow ? 76 : 0)
    readonly property int railReservation: root.showContextRail ? root.desktopContextRailWidth : 0
    // Keep a real page gutter on phones. Media may fill the reading column, but
    // the reading column itself must not touch either viewport edge.
    readonly property int availableSocialWidth: Math.max(320, width - root.navigationReservation - root.railReservation
                                                         - (root.desktop ? Theme.space5
                                                            : root.mobile ? Theme.mobileGutter * 2 : 0))
    // On phones FeedSurface spans the viewport and each cast owns exactly one
    // symmetric content gutter. Reserving a second shell gutter here created a
    // visually wider trailing edge around media.
    readonly property int adaptiveFeedWidth: root.mobile
                                             ? Math.round(width)
                                             : Math.round(Math.min(880, root.availableSocialWidth))
    readonly property int adaptiveProfileWidth: Math.round(Math.min(920, root.availableSocialWidth))
    readonly property int adaptiveCastWidth: Math.round(Math.min(880, root.availableSocialWidth))
    readonly property int activeSocialReadingWidth: presentationBridge.activeView === "profile"
                                                     ? root.adaptiveProfileWidth
                                                     : presentationBridge.activeView === "cast"
                                                       ? root.adaptiveCastWidth : root.adaptiveFeedWidth
    readonly property int adaptiveReadingWidth: root.adaptiveFeedWidth
    readonly property int adaptiveDetailWidth: Math.round(Math.min(Theme.detailWidthWide, Math.max(320, width - root.navigationReservation - Theme.space4)))
    readonly property int adaptiveDashboardWidth: Math.max(320, width - root.navigationReservation - (root.desktop ? Theme.space4 : Theme.mobileGutter * 2))
    readonly property bool listViewActive: presentationBridge.activeView === "feed" || presentationBridge.activeView === "followers" || presentationBridge.activeView === "following"
    property var composerParent: null
    // A remembered signer may still be undergoing asynchronous network verification
    // when the user taps Compose. Keep the intent, not an authentication popup.
    property var pendingComposerIntent: null
    property string replyTargetErrorText: ""
    property string shellRoute: ""
    // Shell route (e.g. Notifications) that was visible when a cast or profile
    // opened above it. Back from that content restores this route instead of
    // falling through to the Home feed.
    property string pendingReturnShellRoute: ""
    // Explicit Cast -> mentioned public profile navigation retains a stable
    // source Cast even if an asynchronous provider callback drops C++ history.
    // The actual native breadcrumb is tried first; this is a bounded
    // correctness fallback for browser reentrancy, not a second history stack.
    property string pendingMentionOriginCastId: ""
    property string pendingMentionTargetFid: ""
    // Explicit Explorer tab navigation must reveal its real inline header
    // (including stories), even after returning from a nested detail view.
    property int explorerTopRevision: 0
    property int profileTopRevision: 0
    property string committedRichCastId: ""
    property string lastStreakCastId: ""
    property bool awaitingRichCommit: false
    property string homeDraftForCast: ""
    // A reply is pinned only after a verified server confirmation.
    property var confirmedReply: ({})
    property string submittedReplyParentId: ""
    property string submittedReplyText: ""
    property var submittedReplyEmbeds: []
    function requestSignOut() {
        if (!presentationBridge.isAuthenticated) return
        accountPopup.close()
        if (localePopupLoader.item !== null)
            localePopupLoader.item.close()
        signOutConfirm.open()
    }
    property string submittedCastBaseline: ""
    property var pendingCastTopics: []
    property string pendingTopicsBaseline: ""
    property real pendingTopicsOwner: 0
    property string pendingMiniAppComposeRequestId: ""
    property string pendingMiniAppComposeText: ""
    property var pendingMiniAppComposeEmbeds: []
    property string pendingMiniAppShareTaskId: ""
    property string homePresentationMode: "for-you"
    property string homeVisualFilter: "nfts"
    property string primaryDestination: "home"
    // A single reduced-motion-aware entrance for every main workspace route.
    // Apply to the shared reading surface, not individual network views, so
    // Feed, Explorer, profile, cast, bookmarks, apps and notifications share
    // one navigation rhythm without recreating their content or scroll state.
    property bool routeMotionArmed: false
    readonly property string navigationMotionKey: root.shellRoute + "|" + root.primaryDestination
        + "|" + presentationBridge.activeView + "|" + presentationBridge.activeFeed
    onNavigationMotionKeyChanged: {
        if (root.routeMotionArmed && !Theme.reduceMotion)
            sharedRouteEntrance.restart()
    }
    Timer {
        interval: 0
        running: true
        repeat: false
        onTriggered: root.routeMotionArmed = true
    }
    property var selectedChannelInfo: ({})
    property string explorerAudience: "all"
    property string explorerDisplayLayout: "magazine"
    property string profileDisplayLayout: "timeline"
    Settings {
        id: profileLayoutPreferences
        category: "profile-feed"
        property alias displayLayout: root.profileDisplayLayout
    }
    Settings {
        id: explorerAudiencePreferences
        category: "explorer-feed"
        property alias audience: root.explorerAudience
        property alias displayLayout: root.explorerDisplayLayout
    }
    function chooseExplorerAudience(audience) {
        const chosen = audience === "following"
                       && presentationBridge.isAuthenticated
                       && Number(presentationBridge.accountFid || 0) > 0
                       ? "following" : "all"
        if (root.explorerAudience === chosen) return
        root.explorerAudience = chosen
        root.selectExplorerPresentationMode(root.explorerPresentationMode, true)
    }
    property string bookmarkChoiceHash: ""
    property real bookmarkChoiceOwner: 0
    property string bookmarkAssignmentHash: ""
    property string bookmarkAssignmentCollection: ""
    property string bookmarkAssignmentCreateName: ""
    property int bookmarkAssignmentAttempts: 0

    function assignSavedBookmark(hash,collectionId) {
        if(!/^0x[0-9a-fA-F]{40}$/.test(String(hash))
           || !/^[0-9a-fA-F-]{36}$/.test(String(collectionId)))return
        root.bookmarkAssignmentHash=String(hash).toLowerCase()
        root.bookmarkAssignmentCollection=String(collectionId)
        root.bookmarkAssignmentAttempts=0
        bookmarkAssignmentFlush.restart()
    }
    function flushBookmarkAssignment() {
        if(!root.bookmarkAssignmentHash.length)return
        if(!presentationBridge.isAuthenticated
           || Number(presentationBridge.accountFid)!==root.bookmarkChoiceOwner) {
            root.bookmarkAssignmentHash=""
            root.bookmarkAssignmentCollection=""
            root.bookmarkAssignmentCreateName=""
            return
        }
        if(openProofConnections.bookmarkPendingCount===0
           && !openProofConnections.bookmarkCollectionsBusy
           && presentationBridge.bookmarkedCastIds.indexOf(root.bookmarkAssignmentHash)>=0) {
            openProofConnections.setBookmarkCollectionMembership(
                root.bookmarkAssignmentHash,root.bookmarkAssignmentCollection,false)
            root.bookmarkAssignmentHash=""
            root.bookmarkAssignmentCollection=""
            return
        }
        if(++root.bookmarkAssignmentAttempts < 40)
            bookmarkAssignmentFlush.restart()
    }

    Timer {
        id: bookmarkAssignmentFlush
        interval: 500
        repeat: false
        onTriggered: root.flushBookmarkAssignment()
    }
    Connections {
        target: openProofConnections
        function onBookmarkCollectionChoiceRequested(hash) {
            root.bookmarkChoiceHash=hash
            root.bookmarkChoiceOwner=Number(presentationBridge.accountFid)
            root.bookmarkAssignmentCreateName=""
            bookmarkCollectionChooser.castHash=hash
            bookmarkCollectionChooser.newName=""
            bookmarkCollectionChooser.open()
            openProofConnections.refreshBookmarkCollections()
        }
        function onBookmarkCollectionsChanged() {
            if(!root.bookmarkAssignmentCreateName.length
               || openProofConnections.bookmarkCollectionsBusy
               || openProofConnections.bookmarkCollectionsError.length)return
            if(!presentationBridge.isAuthenticated
               || Number(presentationBridge.accountFid)!==root.bookmarkChoiceOwner) {
                root.bookmarkAssignmentCreateName=""
                bookmarkCollectionChooser.close()
                return
            }
            const expected=root.bookmarkAssignmentCreateName
            const found=(openProofConnections.bookmarkCollections||[])
                .find(x=>String(x.name||"")===expected)
            if(found) {
                root.bookmarkAssignmentCreateName=""
                root.assignSavedBookmark(root.bookmarkChoiceHash,String(found.id))
                bookmarkCollectionChooser.close()
            }
        }
    }
    property bool topicBrowseActive: false
    property string topicBrowseScope: ""
    property string topicBrowseSlug: ""
    property string topicBrowseAuthorFid: ""
    // Server-confirmed topic memberships are projected into the existing
    // Explorer/Profile cast lists. They never replace the navigation surface.
    property var topicHydrationAttempted: ({})
    function canonicalTopicHash(value) {
        // Provider feeds may supply bare cast hashes, but the topic
        // index always supplies 0x-prefixed Farcaster hashes.
        const digits = String(value || "").toLowerCase().replace(/^0x/, "")
        return /^[0-9a-f]{40}$/.test(digits) ? "0x" + digits : ""
    }
    function topicInlineRows() {
        if (!root.topicBrowseActive) return []
        const available = Object.create(null)
        for (const source of [presentationBridge.topicPreviewCasts || [],
                              presentationBridge.rows || [],
                              presentationBridge.bookmarkedCasts || []]) {
            for (const cast of source) {
                const hash = root.canonicalTopicHash(cast.castId)
                if (hash.length > 0
                    && !available[hash] && (cast.richHydrated !== false))
                    available[hash] = cast
            }
        }
        const selected = []
        for (const membership of openProofConnections.topicCastItems || []) {
            const hash = root.canonicalTopicHash(membership.cast_hash)
            if (hash && available[hash]) selected.push(available[hash])
        }
        return selected
    }
    // App-owned Mute preferences affect viewing, not Farcaster relationships.
    // Apply at the surface boundary without rewriting cached protocol data.
    readonly property var mutedActorIds: openProofConnections ? openProofConnections.mutedProfileFids : []
    function withoutMutedActors(items) {
        if (!items || !root.mutedActorIds || !root.mutedActorIds.length) return items
        const selected=[]
        for(let index=0;index<items.length;++index) {
            const row=items[index] || ({})
            const fid=String(Number(row.authorFid || 0))
            if (root.mutedActorIds.indexOf(fid) < 0) selected.push(row)
        }
        return selected
    }
    Timer {
        id: mutedProfilesRefresh
        interval: 60_000
        repeat: true
        triggeredOnStart: true
        running: presentationBridge.isAuthenticated
                 && (openProofConnections ? openProofConnections.cloudAuthorizationReady : false)
        onTriggered: openProofConnections.fetchMutedProfiles()
    }
    function hydrateInlineTopicCasts() {
        if (!root.topicBrowseActive) return
        const pending = []
        for (const membership of openProofConnections.topicCastItems || []) {
            const hash = root.canonicalTopicHash(membership.cast_hash)
            if (!hash || root.topicHydrationAttempted[hash]) continue
            root.topicHydrationAttempted[hash] = true
            pending.push(hash)
            if (pending.length >= 8) break
        }
        if (pending.length > 0)
            presentationBridge.hydrateTopicCasts(pending)
    }
    Timer {
        id: inlineTopicHydration
        interval: 240
        repeat: false
        onTriggered: root.hydrateInlineTopicCasts()
    }
    Connections {
        target: openProofConnections
        function onTopicCastsChanged() { inlineTopicHydration.restart() }
    }
    Connections {
        target: presentationBridge
        function onTopicPreviewCastsChanged() {
            if (root.topicBrowseActive) inlineTopicHydration.restart()
        }
        function onStateChanged() {
            if (root.topicBrowseActive && root.topicBrowseScope==="profile") {
                const fid=Number(presentationBridge.detail.fid || 0)
                if (fid>0 && fid!==Number(root.topicBrowseAuthorFid || 0))
                    root.closeTopicBrowser()
            }
        }
        function onAuthenticationStateChanged() {
            if (!presentationBridge.isAuthenticated && root.topicBrowseActive)
                root.closeTopicBrowser()
        }
    }
    function openTopicBrowser(slug, scope, authorFid) {
        const chosen=String(slug || "")
        if(!chosen.length) {root.closeTopicBrowser();return}
        // Topic filtering is a read-only operation. An existing Farcaster
        // session must never be sent to the signing/account drawer just
        // because the account-scoped read grant is still being restored.
        if (!presentationBridge.isAuthenticated) {
            root.openAccountPopup()
            return
        }
        root.topicBrowseScope=String(scope || "explorer")
        root.topicBrowseSlug=chosen
        root.topicBrowseAuthorFid=Number(authorFid || 0)>0
                                 ? String(Math.floor(authorFid)) : ""
        root.topicBrowseActive=true
        root.topicHydrationAttempted=({})
        if (openProofConnections)
            openProofConnections.selectCastTopic(chosen,root.topicBrowseAuthorFid)
    }
    function closeTopicBrowser() {
        root.topicBrowseActive=false
        root.topicHydrationAttempted=({})
        inlineTopicHydration.stop()
        root.topicBrowseSlug=""
        root.topicBrowseScope=""
        root.topicBrowseAuthorFid=""
        openProofConnections.clearCastTopic()
    }
    onShellRouteChanged: {
        if (root.shellRoute.length > 0)
            presentationBridge.clearDetailHistory()
        if(root.shellRoute.length > 0 && root.topicBrowseActive)
            root.closeTopicBrowser()
    }
    onPrimaryDestinationChanged: {
        presentationBridge.clearDetailHistory()
        if (root.topicBrowseActive
            && ((root.topicBrowseScope==="explorer" && root.primaryDestination!=="explorer")
                || (root.topicBrowseScope==="profile" && root.primaryDestination==="explorer")))
            root.closeTopicBrowser()
    }
    property string explorerPresentationMode: "for-you"
    Binding {
        target: presentationBridge
        property: "followingArtworkScan"
        value: root.primaryDestination === "explorer" && root.shellRoute.length === 0
            && root.explorerPresentationMode === "visuals"
            && root.explorerAudience === "following"
            && presentationBridge.isAuthenticated
    }
    property string explorerVisualFilter: "nfts"
    // Keep the notifications busy flag as an imperative snapshot. Feeding the
    // bridge property directly into the lazily-instantiated route produced a QML
    // binding cycle during notificationsChanged delivery on Qt 6.11.
    property bool notificationBusySnapshot: false
    readonly property bool localeSettingsOpen: !!localePopupLoader && !!localePopupLoader.item
                                              && localePopupLoader.item.opened
    readonly property bool dataStorageSettingsOpen: !!dataStoragePopupLoader && !!dataStoragePopupLoader.item
                                                   && dataStoragePopupLoader.item.opened
    property bool setupReviewOpen: false
    readonly property bool initialSetupOpen: !!initialSetup && initialSetup.visible
    readonly property bool overlayOpen: root.initialSetupOpen || root.localeSettingsOpen || root.dataStorageSettingsOpen
                                         || (interactionAuthorizationDialog && interactionAuthorizationDialog.opened)
                                         || (composerDrawer && composerDrawer.opened)
                                         || (accountPopup && accountPopup.opened)
                                         || (renderingSettingsPopup && renderingSettingsPopup.opened)
                                         || (taskManagerPopup && taskManagerPopup.opened)
                                         || (feedbackDialog && feedbackDialog.opened)
                                         || (miniAppHost && miniAppHost.walletBrowserPickerActive)
                                         || (proWalletHost && proWalletHost.walletBrowserPickerActive)
                                         || (searchPalette && searchPalette.opened)
                                         || (mobileCreateMenu && mobileCreateMenu.opened)
                                         || (contextDrawer && contextDrawer.opened)
                                         || (mediaGallery && mediaGallery.opened)
                                         || (signOutConfirm && signOutConfirm.opened)

    Timer {
        id: notificationBadgePoll
        interval: 120000
        repeat: true
        running: presentationBridge.runtimeReady
                 && presentationBridge.isAuthenticated
                 && presentationBridge.accountFid > 0
        onTriggered: presentationBridge.refreshNotificationBadge()
    }

    function openSetupReview(step) {
        // Reopen the same eight stages without resetting account credentials
        // or claiming initial onboarding is incomplete.
        if (localePopupLoader.item !== null)
            localePopupLoader.item.close()
        accountPopup.close()
        setupReviewOpen = true
        Qt.callLater(() => initialSetup.switchStep(step === undefined ? 0 : Number(step)))
    }

    function openAccountPopup() {
        if (!presentationBridge.isAuthenticated) {
            root.openSetupReview(3)
            return
        }
        accountPopup.open()
    }

    function requestWriteInteraction() {
        // Follow/Like/Reply from an authenticated but signer-locked account
        // is an authority operation, NOT a new identity login. Never divert
        // this user to the Genyleap account/profile inspector.
        if (!presentationBridge.isAuthenticated)
            root.openAccountPopup()
        else if (!presentationBridge.writeReady)
            root.requestInteractionAuthorization()
    }

    function resolvePendingComposer() {
        const intent = root.pendingComposerIntent
        if (intent === null)
            return
        if (!presentationBridge.isAuthenticated) {
            root.pendingComposerIntent = null
            return
        }
        if (presentationBridge.signerBusy)
            return
        if (!presentationBridge.writeReady) {
            // Keep the explicit Cast intent during an asynchronous signer
            // check. Once it settles, open the device consent sheet exactly
            // once rather than redirecting the user to the profile inspector.
            if (!interactionAuthorizationDialog.opened)
                root.requestInteractionAuthorization()
            return
        }
        root.pendingComposerIntent = null
        // Recheck after the signer event has completed; never publish or
        // open an authorized editor from a stale account transition.
        Qt.callLater(function() {
            if (presentationBridge.isAuthenticated && presentationBridge.writeReady)
                root.openComposer(intent.parentCast, intent.initialAction,
                                  intent.suggestedText, intent.quoteCast,
                                  intent.structuredReport, intent.statsImageFileUrl)
        })
    }

    Connections {
        target: presentationBridge
        function onStateChanged() {
            if (interactionAuthorizationDialog && interactionAuthorizationDialog.visible
                && presentationBridge.writeReady)
                interactionAuthorizationDialog.close()
            root.resolvePendingComposer()
        }
        function onAuthenticationStateChanged() {
            if (interactionAuthorizationDialog.opened
                && (!presentationBridge.isAuthenticated
                    || interactionAuthorizationDialog.authorizationPrincipal !==
                       presentationBridge.accountIdentityIssuer + ":" + presentationBridge.accountFidText))
                interactionAuthorizationDialog.close()
            root.resolvePendingComposer()
        }
    }

    property bool pendingSettingsOpen: false
    property int pendingSettingsSection: -1

    function showLoadedSettings() {
        const popup = localePopupLoader.item
        if (!root.pendingSettingsOpen || popup === null)
            return
        root.pendingSettingsOpen = false
        if (root.pendingSettingsSection >= 0)
            popup.currentSection = root.pendingSettingsSection
        // Phone entry opens the grouped index; a deep link opens its detail.
        if (popup.mobilePresentation)
            popup.mobileIndexOpen = root.pendingSettingsSection < 0
        popup.open()
        popup.scrollToBeginning()
    }

    function openLocaleSettings(section) {
        root.pendingSettingsSection = section === undefined ? -1 : Number(section)
        root.pendingSettingsOpen = true
        localePopupLoader.active = true
        // WebAssembly may asynchronously incubate this Loader; a one-shot
        // callLater is not sufficient when the item is not yet available.
        Qt.callLater(function() { root.showLoadedSettings() })
    }

    function openDataStorageSettings() {
        const settingsPopup = localePopupLoader.item
        if (settingsPopup !== null)
            settingsPopup.close()
        dataStoragePopupLoader.active = true
        Qt.callLater(function() {
            if (dataStoragePopupLoader.item !== null)
                dataStoragePopupLoader.item.open()
        })
    }

    function rememberShellRouteForReturn() {
        if (root.shellRoute.length > 0)
            root.pendingReturnShellRoute = root.shellRoute
    }

    function clearPendingShellReturn() {
        root.pendingReturnShellRoute = ""
        root.pendingMentionOriginCastId = ""
        root.pendingMentionTargetFid = ""
    }

    function returnToPendingShellRoute() {
        // A nested Back may restore a parent cast or profile first. Only
        // surface the originating shell route after the full trail unwinds.
        if (root.pendingReturnShellRoute.length === 0
                || presentationBridge.activeView !== "feed")
            return
        root.shellRoute = root.pendingReturnShellRoute
        root.pendingReturnShellRoute = ""
    }

    function returnFromCastDetail() {
        presentationBridge.returnFromDetail()
        root.returnToPendingShellRoute()
    }

    function returnFromProfileSurface() {
        presentationBridge.returnFromDetail()
        root.returnToPendingShellRoute()
        // The last explicitly requested mention target is unchanged by Back.
        // A successful native breadcrumb advances to another FID or Cast.
        root.restoreMentionCastWhenBackDidNotNavigate(root.pendingMentionTargetFid)
    }
    function restoreMentionCastWhenBackDidNotNavigate(previousFid) {
        const sourceCast = root.pendingMentionOriginCastId
        if (sourceCast.length === 0) return
        // If Back really returned to a Cast or the originating shell route,
        // keep that transition. If it returned to an intermediate profile,
        // the FID changed and the native nested breadcrumb remains authoritative.
        const stuckOnTarget = root.shellRoute.length === 0
            && presentationBridge.activeView === "profile"
            && previousFid === root.pendingMentionTargetFid
            && String(presentationBridge.activeProfileFidText || "") === previousFid
        const fellPastOrigin = presentationBridge.activeView === "feed"
        if (stuckOnTarget || fellPastOrigin) {
            // An empty native history may jump straight to Feed/Notifications
            // instead of the mentioned Cast. Keep the originating shell route
            // for the *next* Back; this Back must first show the conversation.
            if (root.shellRoute.length > 0) {
                root.pendingReturnShellRoute = root.shellRoute
                root.shellRoute = ""
            }
            root.pendingMentionOriginCastId = ""
            root.pendingMentionTargetFid = ""
            presentationBridge.clearDetailHistory()
            presentationBridge.openCast(sourceCast)
        } else if (presentationBridge.activeView !== "profile") {
            root.pendingMentionOriginCastId = ""
            root.pendingMentionTargetFid = ""
        }
    }

    // A public profile read of our own FID is not the authenticated account
    // surface. Preserve the existing Cast -> Profile -> Cast breadcrumb while
    // requesting the account-bound read (and its correct signer/Pro state).
    // Numeric FIDs are protocol-scoped; never treat a matching HyperSnap FID
    // as our Farcaster account, or vice versa.
    function openProfileFromContent(fid) {
        const target = Number(fid)
        if (!Number.isSafeInteger(target) || target <= 0) return
        if (presentationBridge.activeView === "cast" && root.shellRoute.length === 0) {
            const sourceCast = String(presentationBridge.detail.castId || "")
            if (/^0x[0-9a-fA-F]{40}$/.test(sourceCast)) {
                root.pendingMentionOriginCastId = sourceCast
                root.pendingMentionTargetFid = String(target)
            }
        } else if (presentationBridge.activeView === "profile"
                   && root.pendingMentionOriginCastId.length > 0) {
            // Nested public profiles keep the same originating Cast, but
            // the immediate target must track the newest explicit click.
            root.pendingMentionTargetFid = String(target)
        }
        if (root.shellRoute.length > 0) {
            root.rememberShellRouteForReturn()
            presentationBridge.clearDetailHistory()
            root.shellRoute = ""
        }
        // Protocol and custody ownership are verified in C++, never QML.
        presentationBridge.openContentProfileFid(
            target, String(providerProfiles.activeReadContract || ""),
            root.pendingReturnShellRoute === "notifications")
    }

    property bool profileEditorRequestedFromSettings: false
    property int profileEditorAttempts: 0
    function openMyProfileEditorFromSettings() {
        if (!presentationBridge.isAuthenticated) return
        root.profileEditorRequestedFromSettings = true
        root.profileEditorAttempts = 0
        root.openMyProfileFromNavigation()
        profileEditorOpenTimer.restart()
    }
    Timer {
        id: profileEditorOpenTimer
        interval: 160
        repeat: true
        running: false
        onTriggered: {
            if (!root.profileEditorRequestedFromSettings
                || !presentationBridge.isAuthenticated
                || root.profileEditorAttempts++ > 75) {
                stop()
                root.profileEditorRequestedFromSettings = false
                return
            }
            const page = profileSurfaceLoader.item
            if (root.shellRoute === "" && presentationBridge.activeView === "profile"
                && page && page.isOwnProfile) {
                stop()
                root.profileEditorRequestedFromSettings = false
                page.openProfileEditor()
            }
        }
    }
    function openMyProfileFromNavigation() {
        ++root.profileTopRevision
        presentationBridge.clearDetailHistory()
        root.clearPendingShellReturn()
        root.shellRoute = ""
        presentationBridge.openMyProfile()
        Qt.callLater(() => {
            if (root.shellRoute === "" && presentationBridge.activeView === "profile"
                    && profileSurfaceLoader.item)
                profileSurfaceLoader.item.scrollToBeginning()
        })
    }

    Connections {
        target: miniAppHost
        function onProfileRequested(fid) {
            miniAppHost.close()
            root.clearPendingShellReturn()
            root.primaryDestination = "home"
            root.shellRoute = ""
            presentationBridge.openProfileFid(fid)
        }
        function onCastRequested(hash) {
            miniAppHost.close()
            root.clearPendingShellReturn()
            root.primaryDestination = "home"
            root.shellRoute = ""
            presentationBridge.openCast(hash)
        }
        function onShareRequested(url, title, taskId) {
            root.pendingMiniAppShareTaskId = String(taskId || "")
            root.openMiniAppComposer("", {"text": String(title || ""), "embeds": [String(url || "")]})
        }
        function onComposeCastRequested(requestId, options) {
            root.openMiniAppComposer(String(requestId || ""), options || ({}))
        }
    }

    function openShellRoute(route) {
        root.clearPendingShellReturn()
        const nextRoute = String(route || "")
        root.shellRoute = nextRoute
        if (nextRoute === "notifications")
            presentationBridge.openNotifications()
        if (nextRoute === "apps")
            Qt.callLater(() => {
                if (root.shellRoute === "apps" && appsSurfaceLoader.item)
                    appsSurfaceLoader.item.scrollToBeginning()
            })
    }

    function openHome() {
        root.clearPendingShellReturn()
        root.primaryDestination = "home"
        root.shellRoute = ""
        // Home is a standalone hub. Preserve its draft and the existing feed
        // cache; tapping Home still returns to the top of the dashboard.
        const source = presentationBridge.isAuthenticated && root.identityAvailable
                       ? "following" : "trending"
        if (presentationBridge.activeView !== "feed" || !presentationBridge.feedStarted
                || presentationBridge.activeFeed !== source) {
            presentationBridge.selectFeed(source)
        }
        Qt.callLater(() => {
            if (root.homeSurfaceActive)
                homeDashboard.scrollToBeginning()
        })
    }

    function selectHomePresentationMode(mode) {
        const nextMode = String(mode || "for-you")
        root.homePresentationMode = nextMode
        root.homeVisualFilter = "nfts"
    }

    function selectExplorerPresentationMode(mode, forceRefresh) {
        const nextMode = String(mode || "for-you")
        const modeChanged = root.explorerPresentationMode !== nextMode
        const refreshRequested = Boolean(forceRefresh)
        root.explorerPresentationMode = nextMode
        root.explorerVisualFilter = "nfts"

        // Explorer uses a separate confirmed chronological network feed.
        // Home's personal Following and ranked Trending timelines remain intact.
        // The provider owns cursor-based continuation and bulk hydration.
        // The content category and audience scope are orthogonal. Every
        // Explorer category projects from the selected provider feed; never
        // silently return to global Recent when the user selects Following.
        // The NFT adapter queries individual channel heads and merges their
        // timestamps; Following always keeps its account-scoped source.
        const globalSource = nextMode === "visuals" ? "nfts"
                           : nextMode === "devs" ? "developers"
                           : nextMode === "conversations" ? "conversations" : "recent"
        const source = root.explorerAudience === "following"
                       && presentationBridge.isAuthenticated
                       && Number(presentationBridge.accountFid || 0) > 0
                       ? "following" : globalSource
        const emptySettledSource = presentationBridge.feedStarted
                                   && !presentationBridge.isBusy
                                   && presentationBridge.rows.length === 0
        const sameSource = presentationBridge.activeView === "feed"
                           && presentationBridge.activeFeed === source
        const sourceFailed = presentationBridge.sourceUnavailable
                             || String(presentationBridge.errorText || "").length > 0
        const sameHealthySource = sameSource && presentationBridge.feedStarted
                                  && !emptySettledSource && !sourceFailed
        // Explorer projections are sparse. Re-entering a semantic tab must also
        // refresh the source first page; otherwise Devs/NFTs can keep projecting
        // an old cached timeline forever even though the tab itself changed.
        if (sameSource && !presentationBridge.isBusy
                && (sourceFailed || emptySettledSource
                    || (nextMode === "visuals" && modeChanged))) {
            // An explicitly selected sparse NFT tab gets a fresh first page
            // and one authorized recovery probe, rather than reusing a failed
            // Following stream or stale rows from another semantic category.
            presentationBridge.refreshLatestFeed()
        } else if (!sameHealthySource && !(sameSource && presentationBridge.isBusy)) {
            presentationBridge.selectFeed(source)
        } else if ((refreshRequested || (modeChanged && presentationBridge.isStale))
                   && !presentationBridge.isBusy) {
            // Explorer explicitly requests the latest page; ordinary background
            // refresh buffering must not leave newly arrived posts hidden.
            presentationBridge.refreshLatestFeed()
        }
    }

    function openExplorer() {
        const wasExplorer = root.primaryDestination === "explorer"
                            && root.shellRoute === ""
                            && presentationBridge.activeView === "feed"
        if (!wasExplorer)
            presentationBridge.refreshFollowingStories()
        root.clearPendingShellReturn()
        presentationBridge.clearDetailHistory()
        root.primaryDestination = "explorer"
        root.shellRoute = ""
        root.selectExplorerPresentationMode(root.explorerPresentationMode, false)
        // Whether the tab was already selected or opened from a nested
        // Cast/Profile, the explicit Explorer action goes to its REAL top:
        // the story strip is part of FeedSurface's inline header, not a row.
        // The revision also reaches a Loader that becomes active later.
        ++root.explorerTopRevision
    }

    function openTrends() {
        ++root.explorerTopRevision
        // Direct Home shortcut to the provider-backed Trending timeline.
        // It shares the existing Explorer surface, not a synthetic feed.
        root.clearPendingShellReturn()
        root.primaryDestination = "explorer"
        root.shellRoute = ""
        root.explorerPresentationMode = "for-you"
        if (presentationBridge.activeView !== "feed"
                || presentationBridge.activeFeed !== "trending"
                || !presentationBridge.feedStarted) {
            presentationBridge.selectFeed("trending")
        }
        Qt.callLater(() => {
            if (root.primaryDestination === "explorer" && root.shellRoute === ""
                    && feedSurfaceLoader.item)
                feedSurfaceLoader.item.scrollToBeginning()
        })
    }

    // Mentions are internal navigation. Never launch a browser or assume a
    // fixed data-provider hostname for @username links in Cast text.
    function openCastTextLink(url) {
        const target = String(url || "").trim()
        if (/^@fid:[1-9][0-9]*$/.test(target)) {
            if (presentationBridge.activeView !== "cast"
                    && presentationBridge.activeView !== "profile")
                root.clearPendingShellReturn()
            root.shellRoute = ""
            root.openProfileFromContent(Number(target.substring(5)))
            return
        }
        if (/^@[A-Za-z0-9_]+(?:[.-][A-Za-z0-9_]+)*$/.test(target)) {
            if (presentationBridge.activeView !== "cast"
                    && presentationBridge.activeView !== "profile")
                root.clearPendingShellReturn()
            root.shellRoute = ""
            presentationBridge.openContentProfileUsername(
                target.substring(1), String(providerProfiles.activeReadContract || ""),
                root.pendingReturnShellRoute === "notifications")
            return
        }
        presentationBridge.openExternalUrl(target)
    }

    function openSuggestedProfile(username) {
        root.clearPendingShellReturn()
        root.primaryDestination = "explorer"
        root.shellRoute = ""
        presentationBridge.openSearchQuery(String(username || ""))
    }

    function openQuoteComposer(cast) {
        const source = String(cast && (cast.castId || cast.hash) || "").trim()
        const bare = source.replace(/^0x/i, "")
        const hash = /^[0-9a-f]{40}$/i.test(bare) ? "0x" + bare.toLowerCase() : ""
        const authorFid = Number(cast && (cast.authorFid || cast.castAuthorFid || cast.fid) || 0)
        if (!hash || !Number.isSafeInteger(authorFid) || authorFid <= 0) {
            root.replyTargetErrorText = qsTrId("genycaster.cast.quote_reference_unavailable")
            replyTargetErrorLoader.active = true
            Qt.callLater(function() {
                if (replyTargetErrorLoader.item) replyTargetErrorLoader.item.open()
            })
            return
        }
        root.openComposer(null, "quote", "", Object.assign({}, cast,
                            { castId: hash, authorFid: authorFid }))
    }

    function openComposer(parentCast, initialAction, suggestedText, quoteCast, structuredReport, statsImageFileUrl) {
        if (!presentationBridge.isAuthenticated) {
            root.pendingComposerIntent = null
            accountPopup.open()
            return
        }
        if (!presentationBridge.writeReady) {
            // On Safari/PWA an imported Farcaster identity can be valid while
            // the per-device signer is still locked. Preserve the draft intent
            // and open the *authorization* sheet, not the account/profile drawer.
            root.pendingComposerIntent = {
                "parentCast": parentCast || null,
                "initialAction": initialAction || "",
                "suggestedText": suggestedText || "",
                "quoteCast": quoteCast || null,
                "structuredReport": Boolean(structuredReport),
                "statsImageFileUrl": String(statsImageFileUrl || "")
            }
            if (!presentationBridge.signerBusy)
                root.requestInteractionAuthorization()
            return
        }
        root.pendingComposerIntent = null
        presentationBridge.beginComposer()
        if (parentCast !== undefined && parentCast !== null) {
            // Nested replies are canonical Casts too, but are not always in
            // the top-level m_rows snapshot. Supply the provider author FID
            // from the selected row rather than looking it up in m_rows.
            const authorFid = Number(parentCast.authorFid || 0)
            const prepared = authorFid > 0
                             ? presentationBridge.prepareReplyToCast(String(parentCast.castId), authorFid)
                             : presentationBridge.prepareReply(String(parentCast.castId))
            if (!parentCast.castId || !prepared) {
                root.replyTargetErrorText = String(presentationBridge.writeErrorText
                    || qsTrId("genycaster.bridge.reply_target_not_available_as_canonical_cast"))
                replyTargetErrorLoader.active = true
                Qt.callLater(function() {
                    if (replyTargetErrorLoader.item !== null)
                        replyTargetErrorLoader.item.open()
                })
                return
            }
            composerParent = {
                "castId": String(parentCast.castId),
                "author": String(parentCast.author || parentCast.username || ""),
                "displayName": String(parentCast.displayName || parentCast.author || ""),
                "avatarUrl": String(parentCast.avatarUrl || ""),
                "text": String(parentCast.text || "")
            }
        } else {
            composerParent = null
        }
        composerDrawer.parentCast = composerParent
        composerDrawer.quotedCast = initialAction === "quote" ? (quoteCast || null) : null
        composerDrawer.initialAction = String(initialAction || "")
        // Stats handoffs are isolated from saved account drafts and never
        // grant media or signer authority merely by opening the editor.
        composerDrawer.transientDraft = Boolean(structuredReport)
        composerDrawer.suggestedText = String(suggestedText || "")
        composerDrawer.structuredReport = Boolean(structuredReport)
        composerDrawer.statsImageFileUrl = String(statsImageFileUrl || "")
        // Clear the Home quick draft only after a confirmed Cast, never
        // when the full editor is opened or dismissed without publishing.
        root.homeDraftForCast = !composerParent && composerDrawer.suggestedText.length > 0
                                ? composerDrawer.suggestedText : ""
        composerDrawer.suggestedEmbeds = []
        // Qt Drawer determines edge geometry when open() starts, not
        // when aboutToShow fires. Set mobile mode before that first frame.
        composerDrawer.compact = root.mobile
        composerDrawer.open()
        composerDrawer.focusVisualEditorNow()
    }

    function openMiniAppComposer(requestId, options) {
        const id = String(requestId || "")
        if (!presentationBridge.isAuthenticated || !presentationBridge.writeReady) {
            if (id.length > 0)
                miniAppHost.cancelComposeCast(id)
            accountPopup.open()
            return
        }
        presentationBridge.beginComposer()
        const parentHash = String(options.parentHash || "")
        if (parentHash.length > 0) {
            if (!presentationBridge.prepareReply(parentHash)) {
                if (id.length > 0)
                    miniAppHost.cancelComposeCast(id)
                return
            }
            root.composerParent = {
                "castId": parentHash,
                "author": "",
                "displayName": qsTrId("genycaster.shell.this_cast"),
                "avatarUrl": "",
                "text": ""
            }
        } else {
            root.composerParent = null
        }
        root.pendingMiniAppComposeRequestId = id
        root.pendingMiniAppComposeText = String(options.text || "")
        root.pendingMiniAppComposeEmbeds = options.embeds || []
        composerDrawer.parentCast = root.composerParent
        // An earlier Quote must never turn an unrelated Mini App reply into
        // a top-level post when the shared ComposerDrawer is reused.
        composerDrawer.quotedCast = null
        composerDrawer.initialAction = ""
        composerDrawer.transientDraft = true
        composerDrawer.structuredReport = false
        composerDrawer.suggestedText = root.pendingMiniAppComposeText
        composerDrawer.suggestedEmbeds = root.pendingMiniAppComposeEmbeds
        composerDrawer.compact = root.mobile
        composerDrawer.open()
    }

    function recentPosterProfiles(rows, limit) {
        const source = rows || []
        const result = []
        const seen = ({})
        const maximum = limit || 8
        for (let index = 0; index < source.length && result.length < maximum; ++index) {
            const row = source[index] || ({})
            const fid = Number(row.authorFid || 0)
            const username = String(row.author || row.username || "")
            const key = fid > 0 ? "fid:" + fid : "user:" + username.toLowerCase()
            if ((fid <= 0 && username.length === 0) || seen[key])
                continue
            seen[key] = true
            let activityKind = "cast"
            if (Boolean(row.isReply) || String(row.parentCastId || "").length > 0) {
                activityKind = "reply"
            } else {
                const embeds = row.embeds || []
                for (let embedIndex = 0; embedIndex < embeds.length; ++embedIndex) {
                    const embed = embeds[embedIndex] || ({})
                    const kind = String(embed.kind || "").toLowerCase()
                    const url = String(embed.url || embed.value || "").toLowerCase()
                    if (kind === "collectible" || kind === "nft"
                            || /(?:opensea|superrare|foundation|objkt|zora\.co\/(?:collect|collections))/.test(url)) {
                        activityKind = "nft"
                        break
                    }
                    if (kind === "image" || kind === "video" || kind === "media")
                        activityKind = "media"
                }
            }
            result.push({
                fid: fid,
                username: username,
                displayName: String(row.displayName || username),
                avatarUrl: String(row.avatarUrl || ""),
                createdAtMs: Number(row.createdAtMs || 0),
                activityKind: activityKind
            })
        }
        return result
    }

    // Channel browsing has a dedicated, scoped highlight strip. Never reuse
    // the signed-in account's Following stories in someone else's channel.
    function channelHighlights() {
        if (presentationBridge.activeFeed !== "channel") return []
        const channelId = String(root.selectedChannelInfo.id || "")
        if (!channelId) return []
        const source = presentationBridge.rows || []
        const result = []
        const authors = new Set()
        for (let i = 0; i < source.length && result.length < 10; ++i) {
            const row = source[i] || ({})
            const rowChannel = String(row.channelId || row.castChannel || "")
            const parentUrl = String(row.parentUrl || "")
            if (rowChannel !== channelId
                    && !parentUrl.includes("/channel/" + channelId)) continue
            const fid = Number(row.authorFid || 0)
            const castId = String(row.castId || "")
            if (!fid || !castId || authors.has(fid)) continue
            authors.add(fid)
            result.push({
                fid: fid, castId: castId,
                name: String(row.displayName || row.username || row.author || ""),
                avatar: String(row.avatarUrl || "")
            })
        }
        return result
    }

    Component {
        id: channelFeedHeaderComponent
        Column {
            width: parent ? parent.width : 0
            spacing: Theme.space2

            ActionButton {
                objectName: "channelReturnToDirectory"
                visible: Boolean(root.selectedChannelInfo.fromChannels)
                compact: true
                quiet: true
                iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                text: qsTrId("genycaster.side_nav.channels")
                onClicked: root.openShellRoute("channels")
            }

            FeedHeader {
                width: parent.width
                activeFeed: "channel"
                channelInfo: root.selectedChannelInfo
                identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
                compact: root.compact
                busy: presentationBridge.isBusy
                feedStarted: presentationBridge.feedStarted
                hasContent: presentationBridge.rows.length > 0
                sourceUnavailable: presentationBridge.sourceUnavailable
                errorText: presentationBridge.errorText
                onTrendingRequested: presentationBridge.selectFeed("trending")
                onFollowingRequested: presentationBridge.selectFeed("following")
                onSearchRequested: query => presentationBridge.openSearchQuery(query)
                onRefreshRequested: presentationBridge.retry()
            }

            // These are the latest contributors in THIS channel, backed only
            // by the channel-scoped cast rows. They are not global stories.
            Flickable {
                id: channelHighlightRail
                readonly property var channelItems: root.channelHighlights()
                width: parent.width
                height: channelItems.length > 0 ? Theme.scaleUi(84) : 0
                visible: height > 0
                contentWidth: highlightRow.implicitWidth + Theme.mobileGutter * 2
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    id: highlightRow
                    x: Theme.mobileGutter
                    spacing: Theme.space3
                    Repeater {
                        model: channelHighlightRail.channelItems
                        delegate: Item {
                            required property var modelData
                            width: Theme.scaleUi(68)
                            height: Theme.scaleUi(80)
                            Avatar {
                                anchors.top: parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                avatarSize: Theme.scaleUi(48)
                                sourceUrl: String(modelData.avatar || "")
                                displayName: String(modelData.name || "")
                            }
                            DirectionAwareLabel {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                text: String(modelData.name || "")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                            }
                            TapHandler {
                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                onTapped: presentationBridge.openCast(String(modelData.castId || ""))
                            }
                        }
                    }
                }
            }
            Item { width: parent.width; height: Theme.space2 }
        }
    }

    Component {
        id: explorerFeedExperienceHeaderComponent

        Column {
            width: parent ? parent.width : 0
            spacing: 0

            // Explore uses its existing account header action. The native
            // download choice belongs only beside Get Started on Welcome.
            // Explorer's brand/action bar is fixed by the shell. Everything from
            // the greeting downward belongs to the scrolling/pullable feed.
            HomeFeedHeader {
                width: parent.width
                presentationMode: root.explorerPresentationMode
                visualFilter: root.explorerVisualFilter
                audience: presentationBridge.isAuthenticated
                          && Number(presentationBridge.accountFid || 0) > 0
                          ? root.explorerAudience : "all"
                onAudienceRequested: audience => root.chooseExplorerAudience(audience)
                explorerLayout: root.explorerDisplayLayout
                onLayoutRequested: mode => root.explorerDisplayLayout = mode
                displayName: presentationBridge.isAuthenticated ? presentationBridge.accountName : ""
                avatarUrl: presentationBridge.accountAvatarUrl
                // StoryStrip is an account-only Follow graph, never public Recent authors.
                recentProfiles: presentationBridge.followingStoryProfiles
                ownAvatarUrl: String(presentationBridge.accountAvatarUrl
                                     || presentationBridge.accountProfile.avatarUrl || "")
                authenticated: presentationBridge.isAuthenticated
                interactionsAvailable: presentationBridge.isAuthenticated && presentationBridge.writeReady
                compact: root.compact
                filterAvailable: presentationBridge.isAuthenticated
                                 && (openProofConnections ? openProofConnections.topicCatalog : []).length > 0
                onFilterRequested: explorerFilterSheet.open()
                onModeRequested: mode => root.selectExplorerPresentationMode(mode)
                onVisualFilterRequested: filter => root.explorerVisualFilter = filter
                onComposeRequested: root.openComposer(null)
                onPhotoRequested: root.openComposer(null, "media")
                onSpaceRequested: {
                    if (!presentationBridge.isAuthenticated) root.openAccountPopup()
                    else root.shellRoute = "spaces"
                }
                onRecentProfileRequested: profile => {
                    const castId = String(profile && profile.latestCastId
                                          ? profile.latestCastId : "")
                    root.clearPendingShellReturn()
                    root.shellRoute = ""
                    if (castId.length > 0) {
                        // When a verified latest cast is available, open its
                        // conversation directly, including reply-only activity.
                        presentationBridge.openCast(castId)
                    } else if (Number(profile.fid || 0) > 0) {
                        // Less active followees stay useful, never dead taps.
                        presentationBridge.openProfileFid(Number(profile.fid))
                    }
                }
                // HomeComposer's "authenticationRequested" also means a
                // signed-in user lacks write authority. Never show a second
                // Genyleap account sheet for an existing verified identity.
                onAuthenticationRequested: root.requestWriteInteraction()
                onSearchRequested: searchPalette.open()
            }
            // Phone navigation has a bounded six-destination dock. Reach the
            // channel directory from Explore without compressing touch targets
            // or removing Widgets/Bookmarks from that dock.
            RowLayout {
                visible: root.mobile
                width: parent.width
                layoutDirection: Theme.uiDirection
                Item { Layout.fillWidth: true }
                ActionButton {
                    objectName: "mobileExplorerChannelsEntry"
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconGlobe
                    text: qsTrId("genycaster.side_nav.channels")
                    onClicked: root.openShellRoute("channels")
                }
            }
            Item { width: parent.width; height: Theme.space2 }
        }
    }


    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    RowLayout {
        id: shellContent
        objectName: "shellContent"
        // Do not render shell navigation or header behind full-window Setup.
        visible: presentationBridge.authenticationStateObserved && !root.initialSetupOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: root.mobile || root.initialSetupOpen ? parent.bottom : globalStatus.top
        spacing: 0
        layoutDirection: Theme.uiDirection

        SideNavigation {
            reflectionSource: workspaceReflection
            visible: root.desktop
            Layout.fillHeight: true
            Layout.preferredWidth: root.desktopNavigationWidth
            activeView: presentationBridge.activeView
            activeFeed: presentationBridge.activeFeed
            shellRoute: root.shellRoute
            primaryDestination: root.primaryDestination
            hasUnreadNotifications: presentationBridge.hasUnreadNotifications
            notificationUnreadCount: presentationBridge.notificationUnreadCount
            identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
            isAuthenticated: presentationBridge.isAuthenticated
            writeReady: presentationBridge.writeReady
            writeBusy: presentationBridge.writeBusy
            accountFid: presentationBridge.accountFid
            accountProtocol: presentationBridge.nativeCustodyAccount ? "hypersnap"
                : presentationBridge.farcasterCustodyAccount ? "farcaster" : ""
            accountName: presentationBridge.accountName
            accountUsername: presentationBridge.accountUsername
            accountAvatarUrl: presentationBridge.accountAvatarUrl
            profileDetail: presentationBridge.accountProfile
            profileLoading: presentationBridge.isAuthenticated
                            && Number(presentationBridge.accountProfile.fid || 0) !== Number(presentationBridge.accountFid)
            statusText: presentationBridge.statusText
            errorText: root.passiveSignedOutDiscoveryFailure ? "" : presentationBridge.errorText
            diagnosticText: presentationBridge.diagnosticText
            diagnosticsEnabled: presentationBridge.diagnosticsEnabled
            runtimeReady: presentationBridge.runtimeReady
            offline: presentationBridge.isOffline
            sourceUnavailable: root.passiveSignedOutDiscoveryFailure ? false : presentationBridge.sourceUnavailable
            networkKnown: presentationBridge.networkReachabilityKnown
            stale: presentationBridge.isStale
            busy: presentationBridge.isBusy
            readStarted: presentationBridge.hasReadActivity
            hasUsableContent: presentationBridge.rows.length > 0
                              || Boolean(presentationBridge.detail.castId)
                              || Number(presentationBridge.detail.fid || 0) > 0
            onRetryRequested: presentationBridge.retry()
            onTrendingRequested: root.openExplorer()
            onFollowingRequested: root.openHome()
            onProfileRequested: root.openMyProfileFromNavigation()
            onIdentityRequested: root.openAccountPopup()
            onAccountRequested: root.openAccountPopup()
            onSettingsRequested: root.openLocaleSettings()
            onProRequested: genyProDetailsDrawer.open()
            onRouteRequested: route => root.openShellRoute(route)
        }

        CompactNavigationRail {
            reflectionSource: workspaceReflection
            visible: root.compactDesktop || root.narrow
            Layout.fillHeight: true
            Layout.preferredWidth: 76
            activeView: presentationBridge.activeView
            activeFeed: presentationBridge.activeFeed
            shellRoute: root.shellRoute
            primaryDestination: root.primaryDestination
            hasUnreadNotifications: presentationBridge.hasUnreadNotifications
            notificationUnreadCount: presentationBridge.notificationUnreadCount
            isAuthenticated: presentationBridge.isAuthenticated
            identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
            onTrendingRequested: root.openExplorer()
            onFollowingRequested: root.openHome()
            onProfileRequested: root.openMyProfileFromNavigation()
            onIdentityRequested: root.openAccountPopup()
            onSettingsRequested: root.openLocaleSettings()
            onRouteRequested: route => root.openShellRoute(route)
        }

        Item {
            id: navigationResizeHandle
            visible: root.desktop
            Layout.fillHeight: true
            Layout.preferredWidth: visible ? 7 : 0
            z: 40

            readonly property real handleX: width / 2
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: navHover.hovered || navDrag.active ? 2 : 1
                height: parent.height
                color: navHover.hovered || navDrag.active ? Theme.accent : Theme.divider
                opacity: navHover.hovered || navDrag.active ? 0.72 : 0.30
                Behavior on width { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
                Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
            }
            HoverHandler { id: navHover; cursorShape: Qt.SplitHCursor }
            DragHandler {
                id: navDrag
                target: null
                property real startWidth: 0
                onActiveChanged: if (active) startWidth = root.desktopNavigationWidth
                onTranslationChanged: {
                    if (!active) return
                    const direction = Theme.rtl ? -1 : 1
                    appearanceController.setSocialNavigationWidth(startWidth + translation.x * direction)
                }
            }
        }

        Item {
            id: mobileWorkspace
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Exactly one GPU capture of the reading canvas feeds neighboring
            // navigation chrome and inspector surfaces. The captured subtree
            // excludes the sidebar, mobile header, and context rail; never
            // capture shellContent or this Item (self-capture would recurse).
            NeonReflectionSource {
                id: workspaceReflection
                objectName: "workspaceChromaticReflectionSource"
                sourceItem: readingBackdrop
                active: shellContent.visible && root.visible
                    && (root.desktop || root.compactDesktop || root.mobile)
                    && !root.initialSetupOpen
                blurRadius: root.mobile ? 24 : Theme.reflectionBlurRadius
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                AppHeader {
                    reflectionSource: workspaceReflection
                    // Breathing room between the device status-safe-area, brand
                    // chrome and the first content card. Safe area itself is
                    // already handled by the WASM viewport, so don't double it.
                    // Android already excludes the OS status bar from the Qt
                    // window. Do not stack an extra 24dp of header padding.
                    // The Qt/WASM viewport already includes its safe-area
                    // inset. Do not add a second top/bottom gap around the
                    // brand bar on compact desktop/mobile windows.
                    Layout.topMargin: 0
                    Layout.bottomMargin: 0
                    // Cast details already have their own Back/Refresh bar.
                    // Keeping the global brand bar above that contextual bar
                    // duplicates navigation and wastes space on narrow phones.
                    visible: root.mobile && (root.shellRoute.length > 0
                               || presentationBridge.activeView !== "cast")
                    Layout.fillWidth: true
                    compact: root.compact
                    showBrand: true
                    isAuthenticated: presentationBridge.isAuthenticated
                    writeReady: presentationBridge.writeReady
                    writeBusy: presentationBridge.writeBusy
                    notificationUnreadCount: presentationBridge.notificationUnreadCount
                    notificationsSelected: root.shellRoute === "notifications"
                    onAccountRequested: root.openAccountPopup()
                    onOwnProfileRequested: root.openMyProfileFromNavigation()
                    onNotificationsRequested: root.openShellRoute("notifications")
                    onFeedbackRequested: feedbackDialog.open()
                    onSettingsRequested: root.openLocaleSettings()
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.fill: parent
                        layoutDirection: Theme.uiDirection
                        anchors.leftMargin: root.wide && !root.expandedWorkspace ? Theme.space2 : 0
                        anchors.rightMargin: root.wide && !root.expandedWorkspace ? Theme.space2 : 0
                        anchors.topMargin: root.wide ? Theme.space3 : 0
                        spacing: root.wide ? (root.expandedWorkspace ? Theme.space1 : Theme.space3) : 0

                        Item {
                            id: readingPane
                            // The pane owns the remaining workspace. Individual social surfaces
                            // keep their own bounded reading widths and center themselves inside it.
                            // This mirrors the pre-V20 shell and avoids dead strips between content
                            // and the context rail when the window is wider than the reading column.
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            // The blur source must include the opaque canvas;
                            // otherwise sharp underlying text survives through
                            // the alpha of the blurred text. Keep the dock outside
                            // this subtree to avoid a recursive capture.
                            Rectangle {
                                id: readingBackdrop
                                anchors.fill: parent
                                color: Theme.canvas

                                // Actual background network activity, not a fake page load:
                                // remain unobtrusive and never steal taps or scroll gestures.
                                Item {
                                    id: workspaceActivity
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 3
                                    z: 9
                                    visible: presentationBridge.isBusy && !root.initialSetupOpen
                                    clip: true

                                    Superellipse {
                                        width: Math.max(44, parent.width * 0.24)
                                        height: parent.height
                                        curveDepth: height / 2
                                        exponent: 2
                                        fillColor: Theme.accent
                                        x: -width
                                        SequentialAnimation on x {
                                            running: workspaceActivity.visible && !Theme.reduceMotion
                                            loops: Animation.Infinite
                                            NumberAnimation {
                                                from: -workspaceActivity.width * 0.24
                                                to: workspaceActivity.width
                                                duration: 1050
                                                easing.type: Easing.InOutSine
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    id: readingContent
                                    anchors.fill: parent
                                    transform: Translate { id: sharedRouteOffset; y: 0 }
                                    ParallelAnimation {
                                        id: sharedRouteEntrance
                                        NumberAnimation {
                                            target: readingContent
                                            property: "opacity"
                                            from: 0.82
                                            to: 1
                                            duration: Theme.motionEmphasis
                                            easing.type: Easing.OutCubic
                                        }
                                        NumberAnimation {
                                            target: sharedRouteOffset
                                            property: "y"
                                            from: 7
                                            to: 0
                                            duration: Theme.motionEmphasis
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                    // No reserved/painted footer: content extends to the
                                    // screen edge *behind* the floating dock.
                                    // Each scrollable list provides its own trailing
                                    // clearance so the last row stays reachable.
                                    anchors.bottomMargin: 0
                                    // Avoid an extra ColumnLayout spacer above
                                    // the floating controls.
                                    spacing: root.mobile ? 0 : Theme.space3

                                    ComingSoonSurface {
                                        visible: root.shellRoute.length > 0 && root.shellRoute !== "notifications" && root.shellRoute !== "apps"
                                                 && root.shellRoute !== "bookmarks"
                                                 && root.shellRoute !== "channels"
                                        // Page switches fade + settle in instead of snapping.
                                        opacity: visible ? 1.0 : 0.0
                                        scale: visible ? 1.0 : 0.99
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter
                                        iconGlyph: root.shellRoute === "bookmarks" ? Theme.iconBookmark
                                                 : root.shellRoute === "store" ? Theme.iconStore
                                                 : root.shellRoute === "notifications" ? Theme.iconNotification
                                                 : root.shellRoute === "messages" ? Theme.iconMessage
                                                 : root.shellRoute === "channels" ? Theme.iconGlobe
                                                 : root.shellRoute === "stories" ? Theme.iconImage
                                                 : root.shellRoute === "spaces" ? Theme.iconMessage
                                                 : Theme.iconBusiness
                                        title: root.shellRoute === "bookmarks" ? qsTrId("genycaster.roadmap.bookmarks_title")
                                             : root.shellRoute === "store" ? qsTrId("genycaster.roadmap.store_title")
                                             : root.shellRoute === "notifications" ? qsTrId("genycaster.roadmap.notifications_title")
                                             : root.shellRoute === "messages" ? qsTrId("genycaster.roadmap.messages_title")
                                             : root.shellRoute === "channels" ? qsTrId("genycaster.roadmap.channels_title")
                                             : root.shellRoute === "stories" ? qsTrId("genycaster.roadmap.stories_title")
                                             : root.shellRoute === "spaces" ? qsTrId("genycaster.roadmap.spaces_title")
                                             : qsTrId("genycaster.roadmap.business_title")
                                        description: root.shellRoute === "bookmarks" ? qsTrId("genycaster.roadmap.bookmarks_description")
                                                   : root.shellRoute === "store" ? qsTrId("genycaster.roadmap.store_description")
                                                   : root.shellRoute === "notifications" ? qsTrId("genycaster.roadmap.notifications_description")
                                                   : root.shellRoute === "messages" ? qsTrId("genycaster.roadmap.messages_description")
                                                   : root.shellRoute === "channels" ? qsTrId("genycaster.roadmap.channels_description")
                                                   : root.shellRoute === "stories" ? qsTrId("genycaster.roadmap.stories_description")
                                                   : root.shellRoute === "spaces" ? qsTrId("genycaster.roadmap.spaces_description")
                                                   : qsTrId("genycaster.roadmap.business_description")

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    // Public Channel discovery is a real route; the
                                    // composer retains a separate joined-only picker.
                                    Loader {
                                        id: channelsSurfaceLoader
                                        active: root.shellRoute === "channels"
                                        visible: active
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter
                                        sourceComponent: Component {
                                            ChannelsSurface {
                                                channelSource: channelBrowser
                                                adminSource: channelAdmin
                                                readContract: providerProfiles.activeReadContract
                                                readEndpoint: providerProfiles.activeReadEndpoint
                                                appliedWriteProvider: providerProfiles.writeRestartRequired
                                                    ? "pending" : providerProfiles.selectedWriteProviderId
                                                accountFid: presentationBridge.isAuthenticated
                                                            ? presentationBridge.accountFid : 0
                                                authenticated: presentationBridge.isAuthenticated
                                                compact: root.compact
                                                onSignInRequested: root.openAccountPopup()
                                                onChannelRequested: info => {
                                                    const id = String(info && info.id || "")
                                                    if (!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(id))
                                                        return
                                                    root.clearPendingShellReturn()
                                                    root.selectedChannelInfo = Object.assign(
                                                        {}, info, {fromChannels: true})
                                                    root.primaryDestination = "explorer"
                                                    root.shellRoute = ""
                                                    presentationBridge.openChannel(id)
                                                }
                                            }
                                        }
                                    }

                                    Loader {
                                        active: root.shellRoute === "bookmarks"
                                        visible: active
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        // Wider desktop canvas allows two cast cards side by side.
                                        Layout.maximumWidth: root.compact
                                            ? root.adaptiveDetailWidth : root.adaptiveDashboardWidth
                                        Layout.alignment: Qt.AlignHCenter
                                        sourceComponent: Component {
                                            BookmarksSurface {
                                                rows: presentationBridge.bookmarkedCasts
                                                compact: root.compact
                                                authenticated: presentationBridge.isAuthenticated
                                                interactionsAvailable: presentationBridge.isAuthenticated
                                                                       && presentationBridge.writeReady
                                                accountFid: presentationBridge.accountFid
                                                refreshBusy: openProofConnections ? openProofConnections.bookmarkRefreshBusy : false
                                                accountError: openProofConnections ? openProofConnections.bookmarkError : ""
                                                pendingCount: openProofConnections ? openProofConnections.bookmarkPendingCount : 0
                                                collections: openProofConnections ? openProofConnections.bookmarkCollections : []
                                                collectionItems: openProofConnections ? openProofConnections.bookmarkCollectionItems : []
                                                membershipItems: openProofConnections ? openProofConnections.bookmarkMembershipItems : []
                                                selectedCollection: openProofConnections ? openProofConnections.activeBookmarkCollection : ""
                                                collectionsBusy: openProofConnections ? openProofConnections.bookmarkCollectionsBusy : false
                                                collectionsError: openProofConnections ? openProofConnections.bookmarkCollectionsError : ""
                                                onCollectionSelected: id => openProofConnections.selectBookmarkCollection(id)
                                                onCollectionCreateRequested: name => openProofConnections.createBookmarkCollection(name)
                                                onCollectionRenameRequested: (id,name,revision) =>
                                                    openProofConnections.renameBookmarkCollection(id,name,revision)
                                                onCollectionDeleteRequested: (id,revision) =>
                                                    openProofConnections.deleteBookmarkCollection(id,revision)
                                                onMembershipRequested: (hash,id,remove) =>
                                                    openProofConnections.setBookmarkCollectionMembership(hash,id,remove)
                                                onCollectionsRefreshRequested: openProofConnections.refreshBookmarkCollections()
                                                onRefreshRequested: {
                                                    openProofConnections.refreshAccountBookmarks()
                                                    openProofConnections.refreshBookmarkCollections()
                                                }
                                                onRemoveRequested: castId => presentationBridge.removeBookmark(castId)
                                                onPriorityRequested: (castId, priority) =>
                                                    presentationBridge.setBookmarkPriority(castId, priority)
                                                onHydrationRequested: castIds =>
                                                    presentationBridge.hydrateBookmarkedCasts(castIds)
                                                onProfileRequested: fid => {
                                                    root.rememberShellRouteForReturn()
                                                    presentationBridge.clearDetailHistory()
                                                    root.shellRoute = ""
                                                    ++root.profileTopRevision
                                                    root.openProfileFromContent(fid)
                                                }
                                                onReplyRequested: cast => root.openComposer(cast)
                                                onQuoteRequested: cast => root.openQuoteComposer(cast)
                                                onReactionRequested: (kind, cast, active) =>
                                                    presentationBridge.setReaction(kind, Number(cast.authorFid || 0),
                                                        String(cast.castId || ""), active)
                                                onRemoveCastRequested: cast =>
                                                    presentationBridge.removeCast(Number(cast.authorFid || 0),
                                                        String(cast.castId || ""))
                                                onExternalRequested: url => root.openCastTextLink(url)
                                                onMediaRequested: (mediaItems, index) =>
                                                    mediaGallery.openAt(mediaItems, index)
                                                Component.onCompleted: {
                                                    openProofConnections.refreshAccountBookmarks()
                                                    openProofConnections.refreshBookmarkCollections()
                                                }
                                                onCastRequested: castId => {
                                                    const hash = String(castId || "").trim()
                                                    if (!/^0x[0-9a-fA-F]{40}$/.test(hash)) return
                                                    root.rememberShellRouteForReturn()
                                                    root.shellRoute = ""
                                                    // Preserve the original destination on back.
                                                    presentationBridge.openCast(hash)
                                                }
                                                onAuthenticationRequested: root.requestWriteInteraction()
                                            }
                                        }
                                    }

                                    Loader {
                                        id: appsSurfaceLoader
                                        active: root.shellRoute === "apps"
                                        visible: active
                                        opacity: active ? 1.0 : 0.0
                                        scale: active ? 1.0 : 0.99
                                        transform: Translate {
                                            y: active ? 0 : (root.mobile ? 14 : 9)
                                            Behavior on y {
                                                NumberAnimation {
                                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter

                                        sourceComponent: Component {
                                            AppsSurface {
                                                compact: root.compact
                                                onAuthenticationRequested: root.openAccountPopup()
                                                onTaskManagerRequested: taskManagerPopup.open()
                                                onProDetailsRequested: genyProDetailsDrawer.open()
                                            }
                                        }

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    Loader {
                                        id: notificationsRouteLoader
                                        active: root.shellRoute === "notifications"
                                        visible: active
                                        opacity: active ? 1.0 : 0.0
                                        scale: active ? 1.0 : 0.99
                                        transform: Translate {
                                            y: active ? 0 : (root.mobile ? 14 : 9)
                                            Behavior on y {
                                                NumberAnimation {
                                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter

                                        onLoaded: {
                                            // Snapshot after incubation completes; never form a
                                            // cross-component QML binding on the loading flag.
                                            if (item !== null)
                                                item.loadingNotifications = root.notificationBusySnapshot
                                        }

                                        sourceComponent: Component {
                                            NotificationSurface {
                                                rows: presentationBridge.notificationRows
                                                mutedProfileFids: root.mutedActorIds
                                                loadingNotifications: false
                                                hasMore: presentationBridge.notificationsHasMore
                                                errorText: presentationBridge.notificationsErrorText
                                                unsupportedRawNotifications:
                                                    providerProfiles.activeReadContract === "snapchain-v1"
                                                    && !providerProfiles.appliedCompatibleReadFallback
                                                authenticated: presentationBridge.isAuthenticated
                                                compact: root.compact
                                                accountFid: presentationBridge.accountFid
                                                accountProfile: presentationBridge.accountProfile
                                                onRefreshRequested: presentationBridge.openNotifications()
                                                onLoadMoreRequested: presentationBridge.loadMoreNotifications()
                                                onSignInRequested: root.openAccountPopup()
                                                onProfileRequested: fid => {
                                                    root.rememberShellRouteForReturn()
                                                    presentationBridge.clearDetailHistory()
                                                    root.shellRoute = ""
                                                    ++root.profileTopRevision
                                                    root.openProfileFromContent(fid)
                                                }
                                                onCastRequested: castId => {
                                                    root.rememberShellRouteForReturn()
                                                    presentationBridge.clearDetailHistory()
                                                    root.shellRoute = ""
                                                    presentationBridge.openCast(castId)
                                                }
                                            }
                                        }

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    // Channel chrome lives inside the ListView inline header,
                                    // so it scrolls with channel posts rather than staying pinned.

                                    SectionHeader {
                                        visible: root.shellRoute.length === 0 && (presentationBridge.activeView === "followers"
                                                 || presentationBridge.activeView === "following")
                                        opacity: visible ? 1.0 : 0.0
                                        Layout.fillWidth: true
                                        Layout.maximumWidth: root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter
                                        Layout.leftMargin: root.compact ? Theme.mobileGutter : 0
                                        Layout.rightMargin: root.compact ? Theme.mobileGutter : 0
                                        compact: root.compact
                                        title: presentationBridge.activeView === "followers"
                                               ? qsTrId("genycaster.shell.followers") : qsTrId("genycaster.shell.following")
                                        subtitle: qsTrId("genycaster.shell.saved_locally_first_then_refreshed_from_the_network")
                                        onBackRequested: presentationBridge.returnFromSocial()

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    HomeDashboard {
                                        id: homeDashboard
                                        visible: root.shellRoute.length === 0 && presentationBridge.activeView === "feed"
                                                 && root.primaryDestination === "home"
                                        opacity: visible ? 1.0 : 0.0
                                        scale: visible ? 1.0 : 0.99
                                        transform: Translate {
                                            y: visible ? 0 : (root.mobile ? 14 : 9)
                                            Behavior on y {
                                                NumberAnimation {
                                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        // The dashboard owns its scrollable content; the shell
                                        // owns the viewport on both desktop and mobile.
                                        Layout.minimumHeight: 0
                                        Layout.preferredHeight: 0
                                        Layout.maximumWidth: 16777215
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                                        Layout.leftMargin: root.compact ? Theme.mobileGutter
                                                           : root.homeStatsWorkspace ? 0 : Theme.space2
                                        Layout.rightMargin: root.compact ? Theme.mobileGutter
                                                            : root.homeStatsWorkspace ? 0 : Theme.space2
                                        displayName: presentationBridge.isAuthenticated ? presentationBridge.accountName : ""
                                        username: presentationBridge.accountUsername
                                        avatarUrl: presentationBridge.accountAvatarUrl
                                        authenticated: presentationBridge.isAuthenticated
                                        accountFid: presentationBridge.isAuthenticated
                                                    ? String(presentationBridge.accountFid) : ""
                                        readContract: providerProfiles.activeReadContract
                                        readEndpoint: providerProfiles.activeReadEndpoint
                                        busy: presentationBridge.isBusy
                                        notificationUnreadCount: presentationBridge.notificationUnreadCount
                                        rows: presentationBridge.rows
                                        compact: root.compact
                                        bottomOverlayInset: mobileBottomNavigation.visible ? mobileBottomNavigation.height : 0
                                        onExploreRequested: root.openExplorer()
                                        onProRequested: genyProDetailsDrawer.open()
                                        onComposeRequested: text => root.openComposer(null, "", text)
                                        onStatsReportRequested: payload => root.openComposer(null, "", String(payload.text || ""), null, true, String(payload.imageFileUrl || ""))
                                        onPhotoRequested: root.openComposer(null, "media", homeDashboard.draftText)
                                        onSearchRequested: searchPalette.open()
                                        onNotificationsRequested: root.openShellRoute("notifications")
                                        onChannelsRequested: root.openShellRoute("channels")
                                        onAppsRequested: root.openShellRoute("apps")
                                        onSettingsRequested: root.openLocaleSettings()
                                        onProfileRequested: {
                                            if (presentationBridge.isAuthenticated) root.openMyProfileFromNavigation()
                                            else root.openAccountPopup()
                                        }
                                        onCastRequested: castId => presentationBridge.openCast(castId)

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    Loader {
                                        id: feedSurfaceLoader
                                        readonly property bool surfaceRequested: root.shellRoute.length === 0 && root.listViewActive
                                                                                 && !(presentationBridge.activeView === "feed"
                                                                                      && root.primaryDestination === "home")
                                        active: surfaceRequested || item !== null
                                        visible: surfaceRequested
                                        opacity: visible ? 1.0 : 0.0
                                        scale: visible ? 1.0 : 0.99
                                        transform: Translate {
                                            y: feedSurfaceLoader.surfaceRequested ? 0 : (root.mobile ? 14 : 9)
                                            Behavior on y {
                                                NumberAnimation {
                                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.expandedSocialWorkspace
                                                             && presentationBridge.activeView === "feed"
                                                             ? root.availableSocialWidth
                                                             : presentationBridge.activeView === "feed"
                                                               ? root.adaptiveFeedWidth
                                                               : root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter

                                        sourceComponent: Component {
                                            FeedSurface {
                                                explorerLayoutAvailable: root.primaryDestination === "explorer"
                                                    && presentationBridge.activeFeed !== "channel"
                                                explorerLayout: root.explorerDisplayLayout
                                                bottomOverlayInset: root.readingOverlayInset(feedSurfaceLoader)
                                                presentationActive: feedSurfaceLoader.surfaceRequested
                                                rows: root.withoutMutedActors(
                                                    root.topicBrowseActive && root.topicBrowseScope==="explorer"
                                                    ? root.topicInlineRows() : presentationBridge.rows)
                                                // Explorer semantics classify only the live primary feed.
                                                // The legacy art supplement can remain cached for a whole
                                                // session and is therefore intentionally excluded here.
                                                supplementalRows: []
                                                supplementalBusy: false
                                                runtimeReady: presentationBridge.runtimeReady
                                                // A hidden Explorer stays a cast feed while the detail is open.
                                                // Switching to socialMode here cleared stable rows on every Back.
                                                socialMode: presentationBridge.activeView === "followers"
                                                            || presentationBridge.activeView === "following"
                                                busy: root.topicBrowseActive && root.topicBrowseScope==="explorer"
                                                      ? (openProofConnections ? openProofConnections.topicCastsBusy : false) : presentationBridge.isBusy
                                                hasMore: root.topicBrowseActive && root.topicBrowseScope==="explorer"
                                                         ? (openProofConnections ? openProofConnections.topicCastsHasMore : false) : presentationBridge.hasMore
                                                cachedMore: presentationBridge.cachedFeedHasMore
                                                offline: presentationBridge.isOffline
                                                sourceUnavailable: presentationBridge.sourceUnavailable
                                                errorText: root.topicBrowseActive && root.topicBrowseScope==="explorer"
                                                           ? (openProofConnections ? openProofConnections.topicCastsError : "") : presentationBridge.errorText
                                                activeFeed: presentationBridge.activeFeed
                                                communityFallback: presentationBridge.communityFallback
                                                localDerivedRecent: presentationBridge.localDerivedRecent
                                                localDerivedFollowing: presentationBridge.localDerivedFollowing
                                                topicFilterActive: root.topicBrowseActive
                                                                   && root.topicBrowseScope==="explorer"
                                                topicFilterSlug: root.topicBrowseSlug
                                                presentationMode: presentationBridge.activeFeed === "channel"
                                                                  ? "for-you"
                                                                  : root.primaryDestination === "explorer"
                                                                    ? root.explorerPresentationMode : root.homePresentationMode
                                                visualFilter: presentationBridge.activeFeed === "channel"
                                                              ? "all"
                                                              : root.primaryDestination === "explorer"
                                                                ? root.explorerVisualFilter : root.homeVisualFilter
                                                // Keep the offscreen Explorer header mounted across
                                                // Cast -> Back: removing it changes ListView origin/geometry.
                                                headerContent: presentationBridge.activeView === "followers"
                                                               || presentationBridge.activeView === "following" ? null
                                                               : presentationBridge.activeFeed === "channel"
                                                                 ? channelFeedHeaderComponent
                                                                 : root.primaryDestination === "explorer"
                                                                   ? explorerFeedExperienceHeaderComponent : null
                                                scrollRequestRevision: root.primaryDestination === "explorer"
                                                    ? root.explorerTopRevision : 0
                                                identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
                                                feedStarted: presentationBridge.feedStarted
                                                compact: root.compact
                                                interactionsAvailable: presentationBridge.isAuthenticated
                                                                       && presentationBridge.writeReady
                                                accountFid: presentationBridge.accountFid
                                                pendingNewPostCount: presentationBridge.activeView === "feed"
                                                                     ? presentationBridge.pendingNewPostCount : 0
                                                onCastRequested: castId => presentationBridge.openCast(castId)
                                                onProfileRequested: fid => {
                                                    ++root.profileTopRevision
                                                    root.openProfileFromContent(fid)
                                                }
                                                onReplyRequested: cast => root.openComposer(cast)
                                                onQuoteRequested: cast => root.openQuoteComposer(cast)
                                                onReactionRequested: (kind, cast, active) =>
                                                    presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                                onRemoveCastRequested: cast =>
                                                    presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                                onLoadMoreRequested: {
                                                    if(root.topicBrowseActive && root.topicBrowseScope==="explorer")
                                                        openProofConnections.loadMoreTopicCasts()
                                                    else presentationBridge.loadMore()
                                                }
                                                onRefreshRequested: root.topicBrowseActive && root.topicBrowseScope==="explorer"
                                                    ? openProofConnections.selectCastTopic(root.topicBrowseSlug,
                                                        root.topicBrowseAuthorFid)
                                                    : root.primaryDestination === "explorer"
                                                    ? presentationBridge.refreshLatestFeed() : presentationBridge.retry()
                                                onRevealPendingPostsRequested: presentationBridge.revealPendingPosts()
                                                onExternalRequested: url => root.openCastTextLink(url)
                                                onMediaRequested: (mediaItems, index) => mediaGallery.openAt(mediaItems, index)
                                                onRetryRequested: root.topicBrowseActive && root.topicBrowseScope==="explorer"
                                                    ? openProofConnections.selectCastTopic(root.topicBrowseSlug,
                                                        root.topicBrowseAuthorFid)
                                                    : root.primaryDestination === "explorer"
                                                    ? presentationBridge.refreshLatestFeed() : presentationBridge.retry()
                                                onIdentityRequested: presentationBridge.selectFeed("trending")
                                                onFollowingRequested: presentationBridge.selectFeed("following")
                                                onTrendingRequested: root.openExplorer()
                                                onAuthenticationRequested: root.requestWriteInteraction()
                                            }
                                        }

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    Loader {
                                        id: topicBrowseLoader
                                        active: false && root.topicBrowseActive && root.shellRoute.length===0
                                                && (presentationBridge.activeView==="feed"
                                                    || presentationBridge.activeView==="profile")
                                        visible: active
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.adaptiveDetailWidth
                                        Layout.alignment: Qt.AlignHCenter
                                        sourceComponent: Component {
                                            TopicBrowseSurface {
                                                scopeName: root.topicBrowseScope==="profile" ? qsTrId("genycaster.side_nav.profile") : qsTrId("genycaster.side_nav.explorer")
                                                topicSlug: root.topicBrowseSlug
                                                topics: (openProofConnections ? openProofConnections.topicCatalog : [])
                                                items: openProofConnections ? openProofConnections.topicCastItems : []
                                                cachedRows: presentationBridge.rows
                                                hydratedRows: presentationBridge.topicPreviewCasts
                                                bookmarkedRows: presentationBridge.bookmarkedCasts
                                                busy: (openProofConnections ? openProofConnections.topicCastsBusy : false)
                                                hasMore: (openProofConnections ? openProofConnections.topicCastsHasMore : false)
                                                errorText: openProofConnections ? openProofConnections.topicCastsError : ""
                                                compact: root.compact
                                                accountFid: presentationBridge.accountFid
                                                onBackRequested: root.closeTopicBrowser()
                                                onTopicRequested: slug => root.openTopicBrowser(
                                                    slug,root.topicBrowseScope,
                                                    Number(root.topicBrowseAuthorFid || 0))
                                                onMoreRequested: openProofConnections.loadMoreTopicCasts()
                                                onHydrationRequested: castIds => presentationBridge.hydrateTopicCasts(castIds)
                                                onCastRequested: hash => {
                                                    root.closeTopicBrowser()
                                                    presentationBridge.openCast(hash)
                                                }
                                                onProfileRequested: fid => {
                                                    root.closeTopicBrowser()
                                                    root.openProfileFromContent(fid)
                                                }
                                                onMediaRequested: (items,index) => mediaGallery.openAt(items,index)
                                                onExternalRequested: url => root.openCastTextLink(url)
                                            }
                                        }
                                    }

                                    Loader {
                                        id: profileSurfaceLoader
                                        readonly property bool surfaceRequested: root.shellRoute.length === 0
                                                                                 && presentationBridge.activeView === "profile"
                                        active: surfaceRequested || item !== null
                                        visible: surfaceRequested
                                        opacity: visible ? 1.0 : 0.0
                                        scale: visible ? 1.0 : 0.99
                                        transform: Translate {
                                            y: profileSurfaceLoader.surfaceRequested ? 0 : (root.mobile ? 14 : 9)
                                            Behavior on y {
                                                NumberAnimation {
                                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.expandedSocialWorkspace
                                                             ? root.availableSocialWidth : root.adaptiveProfileWidth
                                        Layout.alignment: Qt.AlignHCenter

                                        sourceComponent: Component {
                                            ProfileSurface {
                                                bottomOverlayInset: root.readingOverlayInset(profileSurfaceLoader)
                                                detail: presentationBridge.detail
                                                navigationRevision: root.profileTopRevision
                                                rows: root.topicBrowseActive && root.topicBrowseScope==="profile"
                                                      ? root.topicInlineRows() : presentationBridge.rows
                                                mutedProfileFids: root.mutedActorIds
                                                muteSettingsReady: openProofConnections ? openProofConnections.mutedProfilesReady : false
                                                muteWriteBusy: openProofConnections ? openProofConnections.mutedProfilesBusy : false
                                                buzzAuthorizationReady: openProofConnections
                                                    ? openProofConnections.cloudAuthorizationReady : false
                                                buzzPreferencesReady: openProofConnections
                                                    ? openProofConnections.genyBuzzPreferencesReady : false
                                                buzzEnabled: openProofConnections
                                                    ? openProofConnections.genyBuzzPreferences.enabled === true : false
                                                buzzBusy: openProofConnections
                                                    ? openProofConnections.genyBuzzBusy : false
                                                onBuzzEnabledRequested: enabled => {
                                                    if (openProofConnections)
                                                        openProofConnections.setGenyBuzzEnabled(enabled)
                                                }
                                                onBuzzRequested: (fid,kind) => {
                                                    if (!openProofConnections) return
                                                    root.buzzPendingSendFid = Math.floor(fid)
                                                    root.buzzFeedbackCode = ""
                                                    openProofConnections.sendGenyBuzz(
                                                        Math.floor(fid),kind,"profile")
                                                }
                                                onMuteRequested: (fid,muted) => {
                                                    if (openProofConnections)
                                                        openProofConnections.setProfileMuted(Math.floor(fid), muted)
                                                }
                                                topicFilterActive: root.topicBrowseActive && root.topicBrowseScope==="profile"
                                                topicFilterSlug: root.topicBrowseSlug
                                                topicFilterBusy: (openProofConnections ? openProofConnections.topicCastsBusy : false)
                                                topicFilterHasMore: (openProofConnections ? openProofConnections.topicCastsHasMore : false)
                                                profileFeed: presentationBridge.profileFeed
                                                displayLayout: root.profileDisplayLayout
                                                onLayoutRequested: mode => root.profileDisplayLayout = mode
                                                topicCatalog: (openProofConnections ? openProofConnections.topicCatalog : [])
                                                onTopicRequested: (slug,fid) => {
                                                    if(slug) root.openTopicBrowser(slug,"profile",fid)
                                                    else if(root.topicBrowseActive && root.topicBrowseScope==="profile")
                                                        root.closeTopicBrowser()
                                                }
                                                runtimeReady: presentationBridge.runtimeReady
                                                compact: root.compact
                                                busy: root.topicBrowseActive && root.topicBrowseScope==="profile"
                                                      ? (openProofConnections ? openProofConnections.topicCastsBusy : false) : presentationBridge.isBusy
                                                hasMore: root.topicBrowseActive && root.topicBrowseScope==="profile"
                                                         ? (openProofConnections ? openProofConnections.topicCastsHasMore : false) : presentationBridge.hasMore
                                                offline: presentationBridge.isOffline
                                                sourceUnavailable: presentationBridge.sourceUnavailable
                                                errorText: root.topicBrowseActive && root.topicBrowseScope==="profile"
                                                           ? (openProofConnections ? openProofConnections.topicCastsError : "") : presentationBridge.errorText
                                                interactionsAvailable: presentationBridge.isAuthenticated
                                                                       && presentationBridge.writeReady
                                                accountFid: presentationBridge.accountFid
                                                accountFidText: presentationBridge.accountFidText
                                                ownProfileNavigation: presentationBridge.ownProfileNavigation
                                                accountIssuer: presentationBridge.accountIdentityIssuer
                                                accountIsNativeCustody: presentationBridge.nativeCustodyAccount
                                                onBackRequested: root.returnFromProfileSurface()
                                                onProRequested: genyProDetailsDrawer.open()
                                                onSignerAuthorizationRequested: root.requestInteractionAuthorization()
                                                onFollowersRequested: presentationBridge.openFollowers()
                                                onFollowingRequested: presentationBridge.openFollowing()
                                                onRefreshRequested: presentationBridge.selectProfileFeed(presentationBridge.profileFeed)
                                                onProfileFeedRequested: feed => {
                                                    if(root.topicBrowseActive && root.topicBrowseScope==="profile")
                                                        root.closeTopicBrowser()
                                                    presentationBridge.selectProfileFeed(feed)
                                                }
                                                onCastRequested: castId => presentationBridge.openCast(castId)
                                                onProfileRequested: fid => {
                                                    ++root.profileTopRevision
                                                    root.openProfileFromContent(fid)
                                                }
                                                onReplyRequested: cast => root.openComposer(cast)
                                                onQuoteRequested: cast => root.openQuoteComposer(cast)
                                                onReactionRequested: (kind, cast, active) =>
                                                    presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                                onFollowingMutationRequested: (targetFid, following) =>
                                                    presentationBridge.setFollowing(targetFid, following)
                                                onBlockingMutationRequested: (targetFid, blocked) =>
                                                    presentationBridge.setBlocked(targetFid, blocked)
                                                onRemoveCastRequested: cast =>
                                                    presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                                onExternalRequested: url => root.openCastTextLink(url)
                                                onMediaRequested: (mediaItems, index) => mediaGallery.openAt(mediaItems, index)
                                                onAuthenticationRequested: root.requestWriteInteraction()
                                                onLoadMoreRequested: {
                                                    if(root.topicBrowseActive && root.topicBrowseScope==="profile")
                                                        openProofConnections.loadMoreTopicCasts()
                                                    else presentationBridge.loadMore()
                                                }
                                            }
                                        }

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    Loader {
                                        id: castSurfaceLoader
                                        readonly property bool surfaceRequested: root.shellRoute.length === 0
                                                                                 && presentationBridge.activeView === "cast"
                                        active: surfaceRequested || item !== null
                                        visible: surfaceRequested
                                        opacity: visible ? 1.0 : 0.0
                                        scale: visible ? 1.0 : 0.99
                                        transform: Translate {
                                            y: castSurfaceLoader.surfaceRequested ? 0 : (root.mobile ? 14 : 9)
                                            Behavior on y {
                                                NumberAnimation {
                                                    duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.maximumWidth: root.adaptiveCastWidth
                                        Layout.alignment: Qt.AlignHCenter

                                        sourceComponent: Component {
                                            DetailSurface {
                                                activeView: presentationBridge.activeView
                                                detail: presentationBridge.detail
                                                compact: root.compact
                                                bottomOverlayInset: root.readingOverlayInset(castSurfaceLoader)
                                                busy: presentationBridge.isBusy
                                                interactionsAvailable: presentationBridge.isAuthenticated
                                                                       && presentationBridge.writeReady
                                                accountFid: presentationBridge.accountFid
                                                accountIsNativeCustody: presentationBridge.nativeCustodyAccount
                                                confirmedReply: root.confirmedReply
                                                onBackRequested: root.returnFromCastDetail()
                                                onComposeRequested: root.openComposer(null)
                                                onChannelRequested: channelId => {
                                                    root.selectedChannelInfo = { id: channelId }
                                                    root.clearPendingShellReturn()
                                                    root.primaryDestination = "explorer"
                                                    root.shellRoute = ""
                                                    presentationBridge.openChannel(channelId)
                                                    ++root.explorerTopRevision
                                                }
                                                onRefreshRequested: presentationBridge.retry()
                                                onCastRequested: castId => presentationBridge.openCast(castId)
                                                onProfileRequested: fid => {
                                                    ++root.profileTopRevision
                                                    root.openProfileFromContent(fid)
                                                }
                                                onReplyRequested: cast => root.openComposer(cast)
                                                onQuoteRequested: cast => root.openQuoteComposer(cast)
                                                onReactionRequested: (kind, cast, active) =>
                                                    presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                                onRemoveCastRequested: cast =>
                                                    presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                                onExternalRequested: url => root.openCastTextLink(url)
                                                onMediaRequested: (mediaItems, index) => mediaGallery.openAt(mediaItems, index)
                                                onAuthenticationRequested: root.requestWriteInteraction()
                                            }
                                        }

                                        Behavior on opacity {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                        Behavior on scale {
                                            NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                        }
                                    }
                                }
                            }


                            ComposerDrawer {
                                id: composerDrawer
                                onProRequested: genyProDetailsDrawer.open()
                                // On phones the composer must be positioned against the
                                // live Qt viewport, not the readingPane whose geometry
                                // may extend beyond the visible keyboard viewport.
                                // The readingPane changes width and sometimes
                                // position when desktop side rails reflow. A
                                // modal hosted there used to switch into phone
                                // layout mid-resize and jump across the screen.
                                hostItem: root
                                requestedCompact: root.mobile
                                blurSourceItem: shellContent
                                parentCast: root.composerParent
                                onPublishRequested: (text, mentionFids, mentionPositions, embeds, parentUrl) => {
                                    root.pendingCastTopics = composerDrawer.selectedTopics.slice()
                                    root.pendingTopicsBaseline = String(presentationBridge.lastPublishedCastId || "")
                                    root.pendingTopicsOwner = Number(presentationBridge.accountFid)
                                    root.submittedReplyParentId = ""
                                    root.submittedReplyText = ""
                                    root.awaitingRichCommit = true
                                    root.submittedCastBaseline = String(presentationBridge.lastPublishedCastId || "")
                                    if (root.pendingMiniAppComposeRequestId.length > 0) {
                                        root.pendingMiniAppComposeText = text
                                        root.pendingMiniAppComposeEmbeds = embeds
                                    }
                                    presentationBridge.publishPreparedCast(text, mentionFids, mentionPositions, embeds, parentUrl)
                                }
                                onReplyRequested: (text, mentionFids, mentionPositions, embeds) => {
                                    root.pendingCastTopics = composerDrawer.selectedTopics.slice()
                                    root.pendingTopicsBaseline = String(presentationBridge.lastPublishedCastId || "")
                                    root.pendingTopicsOwner = Number(presentationBridge.accountFid)
                                    root.submittedReplyParentId = root.composerParent
                                        ? String(root.composerParent.castId || "") : ""
                                    root.submittedReplyText = String(text || "")
                                    root.submittedReplyEmbeds = []
                                    for (const item of (embeds || [])) {
                                        const url = String(item && (item.value || item.url) || "")
                                        if (url.length === 0) continue
                                        const isHostedImage = url.startsWith(
                                            (deploymentEndpoints.mediaPublicOrigin + "/m/v1/"))
                                        root.submittedReplyEmbeds.push({
                                            "kind": isHostedImage ? "image" : "url",
                                            "url": url, "value": url
                                        })
                                    }
                                    root.awaitingRichCommit = true
                                    root.submittedCastBaseline = String(presentationBridge.lastPublishedCastId || "")
                                    if (root.pendingMiniAppComposeRequestId.length > 0) {
                                        root.pendingMiniAppComposeText = text
                                        root.pendingMiniAppComposeEmbeds = embeds
                                    }
                                    presentationBridge.publishPreparedReply(text, mentionFids, mentionPositions, embeds)
                                }
                                onThreadRequested: casts => {
                                    // A multi-cast thread's root alone receives the selected topics.
                                    root.pendingCastTopics = composerDrawer.selectedTopics.slice()
                                    root.pendingTopicsBaseline = String(presentationBridge.lastPublishedCastId || "")
                                    root.pendingTopicsOwner = Number(presentationBridge.accountFid)
                                    root.submittedReplyParentId = ""
                                    root.submittedReplyText = ""
                                    root.awaitingRichCommit = true
                                    root.submittedCastBaseline = String(presentationBridge.lastPublishedCastId || "")
                                    presentationBridge.publishPreparedThread(casts)
                                }
                                onAbandoned: {
                                    // User dismissal cancels the *editor* association;
                                    // a late network confirmation may not close a new draft.
                                    root.awaitingRichCommit = false
                                    root.composerParent = null
                                    composerDrawer.quotedCast = null
                                    if (root.pendingMiniAppComposeRequestId.length > 0 && !root.awaitingRichCommit) {
                                        miniAppHost.cancelComposeCast(root.pendingMiniAppComposeRequestId)
                                        root.pendingMiniAppComposeRequestId = ""
                                        root.pendingMiniAppComposeText = ""
                                        root.pendingMiniAppComposeEmbeds = []
                                    }
                                    if (root.pendingMiniAppShareTaskId.length > 0) {
                                        miniAppTaskManager.focusTask(root.pendingMiniAppShareTaskId)
                                        root.pendingMiniAppShareTaskId = ""
                                    }
                                    composerDrawer.transientDraft = false
                                    composerDrawer.suggestedText = ""
                                    composerDrawer.suggestedEmbeds = []
                                    root.homeDraftForCast = ""
                                }
                            }

                        // Floating dock with no full-width footer paint.
                        // Keep real content clearance for touch targets and
                        // the device's bottom gesture-navigation safe area.
                        BottomNavigation {
                            id: mobileBottomNavigation
                            // This stays under readingPane; never reparent a
                            // Layout-owned item after layout has run.
                            visible: root.mobile && presentationBridge.authenticationStateObserved
                                     && !root.overlayOpen
                                     && !root.homeComposerFocused
                            z: 800
                            // Dock reserves the system gesture safe area
                            // and adapts independently of framebuffer DPR.
                            height: implicitHeight
                            anchors.left: readingPane.left
                            anchors.right: readingPane.right
                            anchors.bottom: readingPane.bottom
                            blurSourceItem: readingBackdrop
                            reflectionSource: workspaceReflection
                            glassEnabled: true
                            activeView: presentationBridge.activeView
                            activeFeed: presentationBridge.activeFeed
                            shellRoute: root.shellRoute
                            primaryDestination: root.primaryDestination
                            widgetsOpen: !!contextDrawer && contextDrawer.visible
                            onWidgetsRequested: {
                                if (contextDrawer.visible) contextDrawer.close()
                                else contextDrawer.open()
                            }
                            onTrendingRequested: root.openExplorer()
                            onFollowingRequested: root.openHome()
                            onRouteRequested: route => root.openShellRoute(route)
                            // Primary compose bypasses the secondary action sheet.
                            onCreateRequested: root.openComposer(null)
                            onPhotoRequested: root.openComposer(null, "media")
                            onSearchRequested: searchPalette.open()
                            onProfileRequested: {
                                if (presentationBridge.isAuthenticated)
                                    root.openMyProfileFromNavigation()
                                else
                                    root.openAccountPopup()
                            }
                            onSettingsRequested: root.openLocaleSettings()
                        }
                        }

                        Item {
                            id: contextResizeHandle
                            visible: root.showContextRail
                            Layout.fillHeight: true
                            Layout.preferredWidth: visible ? 7 : 0
                            z: 40

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: contextHover.hovered || contextDrag.active ? 2 : 1
                                height: parent.height
                                color: contextHover.hovered || contextDrag.active ? Theme.accent : Theme.divider
                                opacity: contextHover.hovered || contextDrag.active ? 0.72 : 0.30
                            }
                            HoverHandler { id: contextHover; cursorShape: Qt.SplitHCursor }
                            DragHandler {
                                id: contextDrag
                                target: null
                                property real startWidth: 0
                                onActiveChanged: if (active) startWidth = root.desktopContextRailWidth
                                onTranslationChanged: {
                                    if (!active) return
                                    const direction = Theme.rtl ? 1 : -1
                                    appearanceController.setSocialContextWidth(startWidth + translation.x * direction)
                                }
                            }
                        }

                        // Preserve the widget tree across responsive rail
                        // visibility changes, so live resize does not tear down
                        // and recreate artwork, timers and drag state.
                        Loader {
                            active: root.desktop && root.homeSurfaceActive
                            visible: root.showHomeContextRail
                            Layout.fillHeight: true
                            Layout.preferredWidth: root.showHomeContextRail ? root.desktopContextRailWidth : 0
                            Layout.maximumWidth: root.showHomeContextRail ? root.desktopContextRailWidth : 0

                            sourceComponent: Component {
                                ContextRail {
                                    reflectionSource: workspaceReflection
                                    identityName: presentationBridge.isAuthenticated ? presentationBridge.identityName : ""
                                    accountName: presentationBridge.accountName
                                    accountFid: presentationBridge.accountFid
                                    accountUsername: presentationBridge.accountUsername
                                    accountAvatarUrl: presentationBridge.accountAvatarUrl
                                    profileDetail: presentationBridge.accountProfile
                                    feedRows: presentationBridge.rows
                                    activeFeed: presentationBridge.activeFeed
                                    statusText: presentationBridge.statusText
                                    identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
                                    isAuthenticated: presentationBridge.isAuthenticated
                                    offline: presentationBridge.isOffline
                                    busy: presentationBridge.isBusy
                                    stale: presentationBridge.isStale
                                    sourceUnavailable: presentationBridge.sourceUnavailable
                                    onAccountRequested: root.openAccountPopup()
                                    onRefreshRequested: presentationBridge.retry()
                                    onProfileRequested: username => root.openSuggestedProfile(username)
                                    onProfileFidRequested: fid => root.openProfileFromContent(fid)
                                    onOwnProfileRequested: root.openMyProfileFromNavigation()
                                    onMarketSettingsRequested: { root.openLocaleSettings(2) }
                                }
                            }
                        }

                        Loader {
                            active: root.desktop && root.socialSurfaceActive
                            visible: root.showSocialContextRail
                            Layout.fillHeight: true
                            Layout.preferredWidth: root.showSocialContextRail ? root.desktopContextRailWidth : 0
                            Layout.maximumWidth: root.showSocialContextRail ? root.desktopContextRailWidth : 0

                            sourceComponent: Component {
                                SocialSidebar {
                                    reflectionSource: workspaceReflection
                                    mode: presentationBridge.activeView === "profile" ? "profile"
                                          : presentationBridge.activeView === "cast" ? "cast" : "feed"
                                    authenticated: presentationBridge.isAuthenticated
                                    busy: presentationBridge.isBusy
                                    offline: presentationBridge.isOffline
                                    stale: presentationBridge.isStale
                                    notificationUnreadCount: presentationBridge.notificationUnreadCount
                                    accountFid: presentationBridge.accountFid
                                    accountName: presentationBridge.accountName
                                    accountUsername: presentationBridge.accountUsername
                                    accountAvatarUrl: presentationBridge.accountAvatarUrl
                                    accountProfile: presentationBridge.accountProfile
                                    currentProfile: presentationBridge.activeView === "profile" ? presentationBridge.detail : ({})
                                    feedRows: presentationBridge.rows
                                    onSearchRequested: searchPalette.open()
                                    onAccountRequested: root.openAccountPopup()
                                    onOwnProfileRequested: root.openMyProfileFromNavigation()
                                    onProfileRequested: fid => root.openProfileFromContent(fid)
                                    onNotificationsRequested: root.openShellRoute("notifications")
                                    onExploreRequested: root.openExplorer()
                                }
                            }
                        }
                    }
                }

            }

        }
    }

    // Modal popups live in Overlay.overlay, above the normal item tree. This
    // guard sits above every shell surface but below that popup overlay, so a
    // press/release/wheel sequence can never fall through to a cast, link, or
    // navigation control while any modal surface is open.
    MouseArea {
        id: modalInputGuard
        objectName: "modalInputGuard"
        anchors.fill: parent
        z: 900
        visible: root.overlayOpen
        enabled: visible
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
        propagateComposedEvents: false
        onPressed: mouse => mouse.accepted = true
        onReleased: mouse => mouse.accepted = true
        onClicked: mouse => mouse.accepted = true
        onDoubleClicked: mouse => mouse.accepted = true
        onWheel: wheel => wheel.accepted = true
    }

    StatusFooter {
        id: globalStatus
        z: 40
        visible: !root.mobile && presentationBridge.authenticationStateObserved
                 && !root.initialSetupOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        compactMode: root.mobile
        statusText: presentationBridge.statusText
        errorText: root.passiveSignedOutDiscoveryFailure ? "" : presentationBridge.errorText
        runtimeReady: presentationBridge.runtimeReady
        offline: presentationBridge.isOffline
        sourceUnavailable: root.passiveSignedOutDiscoveryFailure ? false : presentationBridge.sourceUnavailable
        networkKnown: presentationBridge.networkReachabilityKnown
        stale: presentationBridge.isStale
        busy: presentationBridge.isBusy
        hasUsableContent: presentationBridge.rows.length > 0
                          || Boolean(presentationBridge.detail.castId)
                          || Number(presentationBridge.detail.fid || 0) > 0
        onRetryRequested: presentationBridge.retry()
        onRenderingRequested: renderingSettingsPopup.open()
        onTaskManagerRequested: taskManagerPopup.open()
        onFeedbackRequested: feedbackDialog.open()
    }


    ActionButton {
        z: 31
        visible: root.compactDesktop && root.primaryDestination === "explorer" && root.shellRoute.length === 0
                 && presentationBridge.activeView === "feed" && !root.overlayOpen
        compact: true
        quiet: false
        iconOnly: true
        iconGlyph: Theme.iconInfo
        text: qsTrId("genycaster.context.activity")
        anchors.top: parent.top
        anchors.topMargin: Theme.space4
        anchors.right: Theme.rtl ? undefined : parent.right
        anchors.left: Theme.rtl ? parent.left : undefined
        anchors.rightMargin: Theme.rtl ? 0 : Theme.space4
        anchors.leftMargin: Theme.rtl ? Theme.space4 : 0
        onClicked: contextDrawer.open()
    }

    GenyDrawer {
        id: contextDrawer
        objectName: "responsiveWidgetsDrawer"
        bottomSheet: root.mobile
        fullScreen: root.mobile
        showHandle: false
        blurSourceItem: shellContent
        dimBackground: root.mobile
        // A mobile widget is a full-height inspector. Swiping within its
        // ScrollView must never drag or dismiss the entire drawer.
        interactive: !root.mobile && root.compactDesktop
        width: parent ? (root.mobile ? parent.width
            : Math.min(root.desktopContextRailWidth, parent.width * 0.52))
            : root.desktopContextRailWidth
        height: parent ? parent.height : 720
        // The mobile inspector is already a full-width canvas. Avoid a
        // second outer gutter on top of ContextRail's own card gutters.
        contentPadding: 0

        contentItem: ColumnLayout {
            spacing: 0
            RowLayout {
                visible: root.mobile
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? Theme.scaleUi(48) : 0
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                Layout.leftMargin: root.mobile ? Theme.space3 : 0
                Layout.rightMargin: root.mobile ? Theme.space3 : 0
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Icon {
                    text: Theme.iconChart
                    color: Theme.ink
                    preferredSize: Theme.iconSizeLarge
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.context.widgets")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                }
                ActionButton {
                    Layout.alignment: Qt.AlignVCenter
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.close")
                    onClicked: contextDrawer.close()
                }
            }

            ContextRail {
            Layout.fillWidth: true
            Layout.fillHeight: true
            identityName: presentationBridge.isAuthenticated ? presentationBridge.identityName : ""
            accountName: presentationBridge.accountName
            accountFid: presentationBridge.accountFid
            accountUsername: presentationBridge.accountUsername
            accountAvatarUrl: presentationBridge.accountAvatarUrl
            profileDetail: presentationBridge.accountProfile
            feedRows: presentationBridge.rows
            activeFeed: presentationBridge.activeFeed
            statusText: presentationBridge.statusText
            identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
            isAuthenticated: presentationBridge.isAuthenticated
            offline: presentationBridge.isOffline
            busy: presentationBridge.isBusy
            stale: presentationBridge.isStale
            sourceUnavailable: presentationBridge.sourceUnavailable
            onAccountRequested: root.openAccountPopup()
            onRefreshRequested: presentationBridge.retry()
            onProfileRequested: username => root.openSuggestedProfile(username)
            onProfileFidRequested: fid => root.openProfileFromContent(fid)
            onOwnProfileRequested: root.openMyProfileFromNavigation()
            onMarketSettingsRequested: { root.openLocaleSettings(2) }
            }
        }
    }

    FloatingComposeButton {
        id: floatingCompose
        z: 30
        visible: !root.overlayOpen && root.shellRoute.length === 0 && !root.mobile
                 && !(presentationBridge.activeView === "feed" && root.primaryDestination === "home")
        expanded: root.largeDesktop && hovered
        labelText: presentationBridge.activeView === "cast" ? qsTrId("genycaster.shell.reply") : qsTrId("genycaster.header.new_cast")
        iconGlyph: presentationBridge.activeView === "cast" ? Theme.iconReply : Theme.iconCompose
        replyMode: presentationBridge.activeView === "cast" && Boolean(presentationBridge.detail.castId)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.wide ? Theme.space5 + 36 : 78
        // Pin to the physical viewport edge, not the edge of the reading
        // column (which is displaced by a large context sidebar).
        x: Theme.rtl ? Theme.space5 : Math.max(Theme.space5, root.width - Theme.space5 - width)
        onComposeRequested: root.openComposer(null)
        onReplyRequested: root.openComposer(presentationBridge.detail)
    }

    UpdateNoticeDrawer {
        id: releaseNotice
        z: 200
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+K" : "Ctrl+K"
        context: Qt.ApplicationShortcut
        enabled: !root.overlayOpen && !miniAppHost.active
        onActivated: searchPalette.open()
    }

    SearchPalette {
        id: searchPalette
        compact: root.mobile
        blurSourceItem: shellContent
        recentProfiles: root.recentPosterProfiles(presentationBridge.rows, 6)
        results: presentationBridge.searchResults
        resultsBusy: presentationBridge.searchBusy
        resultsErrorText: presentationBridge.searchErrorText
        onSuggestionsRequested: query => presentationBridge.requestSearchSuggestions(query)
        onSuggestionsCleared: presentationBridge.clearSearchSuggestions()
        onProfileRequested: fid => {
            root.clearPendingShellReturn()
            root.shellRoute = ""
            root.openProfileFromContent(fid)
        }
        onChannelRequested: (channelId, channelInfo) => {
            root.selectedChannelInfo = channelInfo || ({})
            root.clearPendingShellReturn()
            // Channel feeds live in the reading surface, not the Home dashboard.
            root.primaryDestination = "explorer"
            root.shellRoute = ""
            presentationBridge.openChannel(channelId)
            ++root.explorerTopRevision
        }
        onSearchRequested: query => {
            const scopedChannel = String(query || "").trim().match(/^(?:channel:|\/|#)([a-zA-Z0-9_-]+)/i)
            root.selectedChannelInfo = scopedChannel ? ({ id: scopedChannel[1] }) : ({})
            root.clearPendingShellReturn()
            root.primaryDestination = "explorer"
            root.shellRoute = ""
            presentationBridge.openSearchQuery(query)
        }
    }

    BookmarkCollectionChooser {
        id: bookmarkCollectionChooser
        collections: openProofConnections ? openProofConnections.bookmarkCollections : []
        busy: openProofConnections ? openProofConnections.bookmarkCollectionsBusy : false
        errorText: openProofConnections ? openProofConnections.bookmarkCollectionsError : ""
        onClosed: root.bookmarkAssignmentCreateName=""
        onProDetailsRequested: genyProDetailsDrawer.open()
        onCollectionPicked: id => root.assignSavedBookmark(
            root.bookmarkChoiceHash,id)
        onCreateRequested: name => {
            root.bookmarkAssignmentCreateName=name
            openProofConnections.createBookmarkCollection(name)
        }
        onUnfiledRequested: {
            root.bookmarkAssignmentHash=""
            root.bookmarkAssignmentCollection=""
            root.bookmarkAssignmentCreateName=""
        }
    }

    ContentFilterSheet {
        id: explorerFilterSheet
        heading: qsTrId("genycaster.filters.explorer")
        sections: {
            const topicOptions=[{id:"",label:qsTrId("genycaster.filters.all_topics")}]
            for (const entry of (openProofConnections ? (openProofConnections ? openProofConnections.topicCatalog : []) : [])) {
                const slug=String(entry.slug || "")
                if (!/^[a-z][a-z0-9-]{1,63}$/.test(slug)) continue
                const names={
                    art:qsTrId("genycaster.topic.art"),
                    development:qsTrId("genycaster.topic.development"),
                    photography:qsTrId("genycaster.topic.photography"),
                    design:qsTrId("genycaster.topic.design"),
                    technology:qsTrId("genycaster.topic.technology"),
                    music:qsTrId("genycaster.topic.music"),
                    writing:qsTrId("genycaster.topic.writing"),
                    science:qsTrId("genycaster.topic.science"),
                    business:qsTrId("genycaster.topic.business"),
                    gaming:qsTrId("genycaster.topic.gaming"),
                    trading:qsTrId("genycaster.topic.trading")
                }
                topicOptions.push({id:slug,label:names[slug]
                    || slug.replace(/-/g," ").replace(/\b\w/g,c=>c.toUpperCase())})
            }
            return [
                {id:"audience",title:qsTrId("genycaster.filters.audience"),
                 hint:qsTrId("genycaster.filters.audience_hint"),value:root.explorerAudience,defaultValue:"all",
                 options:[{id:"all",label:qsTrId("genycaster.home.scope_everyone")},{id:"following",label:qsTrId("genycaster.home.scope_following")}]},
                {id:"topic",title:qsTrId("genycaster.topics.label"),value:root.topicBrowseActive
                    && root.topicBrowseScope==="explorer" ? root.topicBrowseSlug : "",
                 defaultValue:"",options:topicOptions}
            ]
        }
        onApplied: values => {
            // The topic index has no social-graph join. Do not imply
            // Following has narrowed its server-confirmed results.
            root.chooseExplorerAudience(values.topic ? "all"
                                        : String(values.audience || "all"))
            if (values.topic)
                root.openTopicBrowser(String(values.topic),"explorer",0)
            else if (root.topicBrowseActive && root.topicBrowseScope==="explorer")
                root.closeTopicBrowser()
        }
    }

    MediaGallery {
        id: mediaGallery
    }

    MobileCreateMenu {
        id: mobileCreateMenu
        blurSourceItem: shellContent
        onCastRequested: root.openComposer(null)
        onPhotoRequested: root.openComposer(null, "media")
        onSpaceRequested: {
            if (!presentationBridge.isAuthenticated) root.openAccountPopup()
            else root.shellRoute = "spaces"
        }
    }

    Loader {
        id: localePopupLoader
        active: false
        onLoaded: root.showLoadedSettings()
        sourceComponent: Component {
            LocaleSettingsPopup {
                parent: root
                // Settings covers the full viewport on every platform;
                // do not capture a hidden workspace for an invisible blur.
                blurSourceItem: null
                onRecoverySignerRequested: {
                    if (!presentationBridge.isAuthenticated || presentationBridge.signerBusy
                        || presentationBridge.writeReady) return
                    interactionAuthorizationDialog.enteringNativeWords = true
                    interactionAuthorizationDialog.open()
                }
                onCustomAccentStudioRequested: color => accentColorStudio.openWith(color)
                onDataStorageRequested: root.openDataStorageSettings()
                onProDetailsRequested: genyProDetailsDrawer.open()
                onSignOutConfirmationRequested: root.requestSignOut()
                onProfileRequested: {
                    close()
                    root.openMyProfileEditorFromSettings()
                }
            }
        }
    }

    Loader {
        id: dataStoragePopupLoader
        active: false
        sourceComponent: Component {
            DataStoragePopup {
                parent: root
                blurSourceItem: shellContent
                onBackRequested: {
                    close()
                    root.openLocaleSettings()
                }
            }
        }
    }

    // Keep nested settings tools as top-level overlay siblings. A Popup nested
    // inside another Drawer can lose stacking/focus ownership while the parent
    // overlay handles the triggering pointer release.
    AccentColorStudio {
        id: accentColorStudio
        parent: Overlay.overlay
        onColorAccepted: color => {
            if (localePopupLoader.item !== null)
                localePopupLoader.item.applyCustomAccentColor(color)
        }
    }

    GenyDrawer {
        id: accountPopup
        property bool existingMethodsExpanded: false
        onClosed: existingMethodsExpanded = false
        bottomSheet: root.compact
        blurSourceItem: shellContent
        dimBackground: true
        width: parent ? (bottomSheet ? parent.width : Math.min(440, parent.width * 0.88)) : 440
        // A content-sized mobile account sheet must not leave a huge blank slab
        // between the identity summary and the actions. ScrollView handles
        // exceptionally long error messages or large accessibility fonts.
        height: parent ? (bottomSheet
                          ? Math.min(parent.height * 0.82,
                                     Math.max(355, accountActions.implicitHeight
                                              + (presentationBridge.isAuthenticated
                                                 ? accountSummary.implicitHeight + 112 : 205)))
                          : parent.height) : 660
        contentPadding: 0

        contentItem: Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: accountPopup && accountPopup.bottomSheet ? Theme.mobileGutter : Theme.space5
                anchors.rightMargin: accountPopup && accountPopup.bottomSheet ? Theme.mobileGutter : Theme.space5
                anchors.topMargin: accountPopup && accountPopup.bottomSheet ? Theme.space3 : Theme.space5
                anchors.bottomMargin: accountPopup && accountPopup.bottomSheet ? Theme.space3 : Theme.space5
                spacing: Theme.space3

                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection

                    SetupPageHeader {
                        Layout.fillWidth: true
                        showIcon: true
                        iconGlyph: Theme.iconUser
                        title: presentationBridge.isAuthenticated
                           ? (presentationBridge.accountIdentityIssuer === "genycaster:device-local"
                              || presentationBridge.accountIdentityIssuer === "farcaster:optimism-id-registry"
                              || presentationBridge.authenticationRoute === "farcaster"
                              || presentationBridge.authenticationRoute === "recovery"
                              || presentationBridge.accountIdentityIssuer.length === 0
                              ? qsTrId("genycaster.shell.your_farcaster_account")
                              : qsTrId("genycaster.shell.your_genyleap_account"))
                           : presentationBridge.authenticationPhase === "linking-farcaster"
                             ? qsTrId("genycaster.shell.link_your_farcaster_account")
                             : presentationBridge.authenticationPhase === "awaiting-approval"
                               ? qsTrId("genycaster.shell.approve_sign_in")
                               : qsTrId("genycaster.shell.sign_in_to_interact")
                    subtitle: presentationBridge.isAuthenticated
                              ? (presentationBridge.accountIdentityIssuer === "genycaster:device-local"
                                 ? qsTrId("genycaster.setup.remembered_device_label")
                                 : presentationBridge.accountIdentityIssuer === "farcaster:optimism-id-registry"
                                 ? qsTrId("genycaster.shell.this_verified_session_identifies_your_account_protocol")
                                 : presentationBridge.authenticationRoute === "recovery"
                                 ? qsTrId("genycaster.setup.recovery_verified")
                                 : presentationBridge.authenticationRoute === "farcaster"
                                   && presentationBridge.accountIdentityIssuer === deploymentEndpoints.openProofIssuer
                                 ? qsTrId("genycaster.shell.farcaster_sign_in_openproof_verification")
                                   + "\n" + qsTrId("genycaster.shell.farcaster_sign_in_openproof_description")
                                 : presentationBridge.authenticationRoute === "farcaster"
                                 ? qsTrId("genycaster.shell.this_verified_session_identifies_your_account_protocol")
                                 : presentationBridge.accountIdentityIssuer.length > 0
                                   ? qsTrId("genycaster.shell.openproof_account_farcaster_protocol_identity")
                                   : qsTrId("genycaster.shell.this_verified_session_identifies_your_account_protocol"))
                              : qsTrId("genycaster.setup.subtitle")
                    }

                    ActionButton {
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconClose
                        text: qsTrId("genycaster.locale.close")
                        onClicked: accountPopup.close()
                    }
                }

                ScrollView {
                    id: accountScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                    ScrollBar.vertical: GenyScrollBar {}

                    ColumnLayout {
                        width: accountScroll ? accountScroll.availableWidth : 0
                        spacing: Theme.space3

                        Superellipse {
                            visible: presentationBridge.isAuthenticated
                            Layout.fillWidth: true
                            implicitHeight: accountSummary.implicitHeight + Theme.space4 * 2
                            curveDepth: Theme.curveContainer
                            fillColor: Theme.surfaceMuted
                            borderWidth: 0

                            ColumnLayout {
                                id: accountSummary
                                anchors.fill: parent
                                anchors.margins: Theme.space4
                                spacing: Theme.space1

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space3
                                    layoutDirection: Qt.LeftToRight

                                    Avatar {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        avatarSize: 48
                                        sourceUrl: presentationBridge.accountAvatarUrl
                                        displayName: presentationBridge.accountName
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Layout.alignment: Qt.AlignVCenter

                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            forceLeftToRight: true
                                            horizontalAlignment: Text.AlignLeft
                                            text: presentationBridge.accountName.length > 0
                                                  ? presentationBridge.accountName
                                                  : qsTrId("genycaster.shell.signed_in_account")
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeTitle3Size
                                            font.weight: Theme.weightDemiBold
                                            elide: Text.ElideRight
                                        }

                                        DirectionAwareLabel {
                                            visible: presentationBridge.accountUsername.length > 0
                                            Layout.fillWidth: true
                                            forceLeftToRight: true
                                            horizontalAlignment: Text.AlignLeft
                                            text: "@" + presentationBridge.accountUsername
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeCaptionSize
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                DirectionAwareLabel {
                                    visible: presentationBridge.accountIdentityIssuer.length > 0
                                    Layout.fillWidth: true
                                    text: presentationBridge.accountIdentityBadge
                                    color: Theme.accent
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightDemiBold
                                    wrapMode: Text.Wrap
                                }

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: presentationBridge.signerStatusText
                                    color: presentationBridge.writeReady ? Theme.success : Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }

                                DirectionAwareLabel {
                                    visible: presentationBridge.signerErrorText.length > 0
                                    Layout.fillWidth: true
                                    text: presentationBridge.signerErrorText
                                    color: Theme.warning
                                    font.pixelSize: Theme.typeCaptionSize
                                    lineHeight: 1.3
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        Superellipse {
                            visible: !presentationBridge.isAuthenticated
                                     && !presentationBridge.authenticationAvailable
                            Layout.fillWidth: true
                            implicitHeight: unavailableText.implicitHeight + Theme.space4 * 2
                            curveDepth: Theme.curveContainer
                            fillColor: Theme.warningSurface

                            DirectionAwareLabel {
                                id: unavailableText
                                anchors.fill: parent
                                anchors.margins: Theme.space4
                                text: qsTrId("genycaster.runtime.sign_in_could_not_be_completed")
                                color: Theme.warning
                                font.pixelSize: Theme.typeCaptionSize
                                lineHeight: 1.3
                                wrapMode: Text.Wrap
                            }
                        }

                        DirectionAwareLabel {
                            visible: presentationBridge.authenticationPhase === "starting"
                            Layout.fillWidth: true
                            text: presentationBridge.authenticationRoute === "openproof"
                                  ? qsTrId("genycaster.shell.connecting_securely_to_genyleap")
                                  : qsTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            wrapMode: Text.Wrap
                        }

                        ColumnLayout {
                            visible: presentationBridge.authenticationPhase === "awaiting-approval"
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: presentationBridge.authenticationRoute === "openproof"
                                      ? qsTrId("genycaster.shell.approve_genyleap_sign_in_in_your_browser")
                                      : qsTrId("genycaster.shell.approve_the_request_in_a_farcaster_compatible_client_t")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                lineHeight: 1.35
                                wrapMode: Text.Wrap
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.the_approval_link_is_kept_transient_and_opens_through_")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                lineHeight: 1.3
                                wrapMode: Text.Wrap
                            }
                        }

                        DirectionAwareLabel {
                            visible: presentationBridge.authenticationPhase === "linking-farcaster"
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.shell.genyleap_verified_link_farcaster_to_continue")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            lineHeight: 1.35
                            wrapMode: Text.Wrap
                        }

                        DirectionAwareLabel {
                            visible: presentationBridge.authenticationErrorText.length > 0
                            Layout.fillWidth: true
                            text: presentationBridge.authenticationErrorText
                            color: Theme.warning
                            font.pixelSize: Theme.typeCaptionSize
                            lineHeight: 1.3
                            wrapMode: Text.Wrap
                        }

                ColumnLayout {
                    id: accountActions
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    SetupActionCard {
                        visible: !presentationBridge.isAuthenticated
                                 && presentationBridge.authenticationPhase !== "starting"
                                 && presentationBridge.authenticationPhase !== "awaiting-approval"
                                 && presentationBridge.authenticationPhase !== "linking-farcaster"
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.setup.new_profile")
                        subtitle: qsTrId("genycaster.setup.new_profile_body")
                        trailingText: qsTrId("genycaster.shell.coming_soon")
                        enabled: false
                    }

                    SetupActionCard {
                        visible: !presentationBridge.isAuthenticated
                                 && presentationBridge.authenticationPhase !== "starting"
                                 && presentationBridge.authenticationPhase !== "awaiting-approval"
                                 && presentationBridge.authenticationPhase !== "linking-farcaster"
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.setup.existing_profile")
                        subtitle: qsTrId("genycaster.setup.existing_profile_body")
                        primary: accountPopup.existingMethodsExpanded
                        showSelectionMark: false
                        enabled: !presentationBridge.signerBusy
                        onClicked: accountPopup.existingMethodsExpanded = !accountPopup.existingMethodsExpanded
                    }

                    SetupPrimaryButton {
                        visible: presentationBridge.authenticationPhase === "awaiting-approval"
                                 || presentationBridge.authenticationPhase === "linking-farcaster"
                        Layout.fillWidth: true
                        showArrow: false
                        text: presentationBridge.authenticationPhase === "linking-farcaster"
                              || presentationBridge.authenticationRoute === "farcaster"
                              ? qsTrId("genycaster.shell.open_farcaster")
                              : qsTrId("genycaster.shell.open_genyleap")
                        enabled: presentationBridge.authenticationApprovalAvailable
                        onClicked: presentationBridge.openAuthenticationApproval()
                    }

                    ActionButton {
                        visible: presentationBridge.authenticationPhase === "awaiting-approval"
                                 || presentationBridge.authenticationPhase === "linking-farcaster"
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.cancel")
                        quiet: true
                        onClicked: {
                            presentationBridge.cancelSignIn()
                            accountPopup.close()
                        }
                    }

                    SetupActionCard {
                        visible: accountPopup.existingMethodsExpanded && !presentationBridge.isAuthenticated
                                 && presentationBridge.authenticationPhase !== "starting"
                                 && presentationBridge.authenticationPhase !== "awaiting-approval"
                                 && presentationBridge.authenticationPhase !== "linking-farcaster"
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.shell.continue_with_genyleap")
                        subtitle: qsTrId("genycaster.setup.account_genyleap_body")
                        iconGlyph: "brands/geny"
                        primary: false
                        enabled: presentationBridge.runtimeReady && presentationBridge.authenticationAvailable
                        onClicked: presentationBridge.beginSignIn()
                    }

                    SetupActionCard {
                        visible: accountPopup.existingMethodsExpanded && !presentationBridge.isAuthenticated
                                 && presentationBridge.authenticationPhase !== "starting"
                                 && presentationBridge.authenticationPhase !== "awaiting-approval"
                                 && presentationBridge.authenticationPhase !== "linking-farcaster"
                        Layout.fillWidth: true
                        title: qsTrId("genycaster.shell.sign_in_with_farcaster")
                        subtitle: qsTrId("genycaster.setup.account_farcaster_body")
                        iconGlyph: "brands/farcaster"
                        enabled: presentationBridge.runtimeReady && presentationBridge.authenticationAvailable
                        onClicked: presentationBridge.beginFarcasterSignIn()
                    }

                    ActionButton {
                        visible: presentationBridge.isAuthenticated && !presentationBridge.writeReady
                        readonly property bool canRetryVerification:
                            presentationBridge.signerPhase === "unavailable"
                            || presentationBridge.signerPhase === "failed"
                        Layout.fillWidth: true
                        text: canRetryVerification ? qsTrId("genycaster.empty.retry")
                              : presentationBridge.signerBusy
                              ? qsTrId("genycaster.shell.enabling")
                              : wasmReadOnly
                                ? qsTrId("genycaster.shell.enable_publishing_with_recovery_phrase")
                                : qsTrId("genycaster.shell.enable_interactions")
                        prominent: true
                        // WASM uses its own asynchronous wallet/WebCrypto/Snapchain
                        // signer path. An unavailable *native* keychain must not
                        // disable the browser custody approval workflow.
                        enabled: !presentationBridge.signerBusy
                        onClicked: canRetryVerification ? presentationBridge.refreshSigner()
                                                        : root.requestInteractionAuthorization()
                    }

                    ActionButton {
                        visible: false // Session security actions live in Settings.
                        Layout.fillWidth: true
                        compact: true
                        quiet: true
                        text: presentationBridge.signerBusy
                              ? qsTrId("genycaster.shell.revoking")
                              : qsTrId("genycaster.shell.revoke_signer")
                        destructive: true
                        enabled: !presentationBridge.signerBusy
                        onClicked: presentationBridge.revokeSigner()
                    }

                    ActionButton {
                        id: accountSignOutButton
                        objectName: "accountSignOutButton"
                        visible: false // Sign Out is available in Settings / Account.
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.sign_out")
                        onClicked: root.requestSignOut()
                    }

                    ActionButton {
                        visible: !presentationBridge.isAuthenticated
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.close")
                        quiet: true
                        onClicked: accountPopup.close()
                    }
                }

                        Item { Layout.preferredHeight: Theme.space3 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.divider
                }

                ActionButton {
                    visible: presentationBridge.isAuthenticated
                    Layout.fillWidth: true
                    quiet: true
                    iconGlyph: Theme.iconPalette
                    text: qsTrId("genycaster.setup.title")
                    onClicked: root.openSetupReview(6)
                }

                // Keep profile navigation and session management reachable
                // without scrolling through a long signer/account summary.
                ActionButton {
                    objectName: "mobileAccountProfileButton"
                    visible: root.mobile && presentationBridge.isAuthenticated
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.home.view_profile")
                    quiet: true
                    onClicked: {
                        accountPopup.close()
                        root.openMyProfileFromNavigation()
                    }
                }

                ActionButton {
                    objectName: "mobileAccountBookmarksButton"
                    visible: root.mobile && presentationBridge.isAuthenticated
                    Layout.fillWidth: true
                    quiet: true
                    iconGlyph: Theme.iconBookmark
                    text: qsTrId("genycaster.side_nav.bookmarks")
                    onClicked: {
                        accountPopup.close()
                        root.openShellRoute("bookmarks")
                    }
                }

                // A persistent mobile exit action must not be hidden below
                // long account details or signer recovery messages.
                ActionButton {
                    id: mobileAccountSignOutButton
                    objectName: "mobileAccountSignOutButton"
                    visible: false // Sign Out is available in Settings / Account.
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.shell.sign_out")
                    destructive: true
                    onClicked: root.requestSignOut()
                }
            }
        }
    }

    // Invalid or unavailable reply targets must never fail silently. Instantiate
    // this rare error surface only after an actual failed reply preparation.
    Loader {
        id: replyTargetErrorLoader
        active: false
        sourceComponent: Component {
            GenyAdaptiveDialog {
                id: replyTargetErrorDialog
                objectName: "replyTargetErrorDialog"
                parent: Overlay.overlay
                blurSourceItem: shellContent
                popupZ: 20000
                mobileHeightRatio: 1.0
                width: root.mobile ? (parent ? parent.width : 420) : 440
                height: root.mobile ? 230 : 215
                onDidClose: Qt.callLater(function() { replyTargetErrorLoader.active = false })
                contentItem: ColumnLayout {
                    spacing: Theme.space3
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: root.replyTargetErrorText
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.close")
                        onClicked: replyTargetErrorDialog.close()
                    }
                }
            }
        }
    }

    // One confirmation, two native presentation patterns: on phones a true
    // bottom drawer; on desktop a centered modal with a live blurred scene.
    GenyAdaptiveDialog {
        id: signOutConfirm
        objectName: "signOutConfirmation"
        parent: Overlay.overlay
        blurSourceItem: shellContent
        popupZ: 20000
        dimBackground: true
        showHandle: root.mobile
        contentPadding: Theme.space4
        mobileHeightRatio: 1.0
        width: root.mobile ? (parent ? parent.width : 420) : 440
        height: root.mobile ? 298 : 272
        contentItem: ColumnLayout {
            spacing: Theme.space3
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.session.logout_confirm_title")
                color: Theme.ink
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
                wrapMode: Text.WordWrap
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.session.logout_confirm_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeBodySize
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.shell.cancel")
                    quiet: false
                    elevated: false
                    onClicked: signOutConfirm.close()
                }
                ActionButton {
                    objectName: "confirmSignOutButton"
                    Layout.fillWidth: true
                    destructive: true
                    destructiveFilled: true
                    prominent: false
                    elevated: false
                    iconGlyph: Theme.iconBack
                    text: qsTrId("genycaster.shell.sign_out")
                    onClicked: {
                        signOutConfirm.close()
                        presentationBridge.signOut()
                    }
                }
            }
        }
    }

    // App-owned GenyBuzz inbox. Polling is bounded and only runs during an
    // authenticated visible session. A future DM view can reuse the same
    // controller without owning delivery, sound or a second nudge backend.
    Timer {
        id: genyBuzzPoll
        interval: 30_000
        repeat: true
        running: root.visible && openProofConnections
            && openProofConnections.cloudAuthorizationReady
        onRunningChanged: {
            if (running) {
                openProofConnections.fetchGenyBuzzPreferences()
                openProofConnections.fetchGenyBuzzInbox()
            }
        }
        onTriggered: {
            openProofConnections.fetchGenyBuzzPreferences()
            openProofConnections.fetchGenyBuzzInbox()
            root.showNextBuzz()
        }
    }
    Connections {
        target: openProofConnections
        function onGenyBuzzChanged() {
            if (!openProofConnections || !openProofConnections.cloudAuthorizationReady) {
                genyBuzzToast.close()
                genyBuzzFeedbackToast.close()
                root.pendingBuzz = ({})
                root.lastDisplayedBuzzId = ""
                root.buzzPendingSendFid = 0
                return
            }
            if (openProofConnections.genyBuzzPreferencesReady
                && openProofConnections.genyBuzzPreferences.enabled !== true) {
                genyBuzzToast.close()
                root.pendingBuzz = ({})
                root.lastDisplayedBuzzId = ""
            }
            // Only a reply to an actual Send Buzz gesture can display this
            // feedback. Other account/polling changes must never do so.
            if (root.buzzPendingSendFid > 0
                && !openProofConnections.genyBuzzBusy) {
                const code = String(openProofConnections.genyBuzzError || "")
                const delivered = Number(openProofConnections.genyBuzzLastSentFid || 0)
                    === root.buzzPendingSendFid
                if (code.length > 0 || delivered) {
                    root.buzzPendingSendFid = 0
                    root.buzzFeedbackCode = delivered ? "sent" : code
                    genyBuzzFeedbackToast.open()
                }
            }
            root.showNextBuzz()
        }
    }
    Popup {
        id: protocolWriteFeedback
        objectName: "protocolWriteFeedback"
        parent: Overlay.overlay
        modal: false
        focus: false
        z: 15002
        padding: Theme.space3
        width: parent ? Math.min(400, parent.width - Theme.space3 * 2) : 330
        x: parent ? (Theme.rtl ? Theme.space3 : Math.max(Theme.space3, parent.width - width - Theme.space3)) : 0
        y: Theme.space3
        property string message: ""
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface { curveDepth: Theme.curveContainer }
        Timer {
            id: protocolWriteFeedbackTimer
            interval: 6500
            onTriggered: protocolWriteFeedback.close()
        }
        contentItem: DirectionAwareLabel {
            text: protocolWriteFeedback.message
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            font.pixelSize: Theme.typeBodySize
        }
        Connections {
            target: presentationBridge
            function onStateChanged() {
                const error = String(presentationBridge.writeErrorText || "")
                if ((presentationBridge.writePhase === "failed" || presentationBridge.writePhase === "blocked")
                        && error.length > 0 && error !== protocolWriteFeedback.message) {
                    protocolWriteFeedback.message = error
                    protocolWriteFeedback.open()
                    protocolWriteFeedbackTimer.restart()
                } else if (error.length === 0) {
                    protocolWriteFeedback.message = ""
                    protocolWriteFeedback.close()
                }
            }
        }
    }

    Popup {
        id: genyBuzzFeedbackToast
        objectName: "genyBuzzSendFeedback"
        parent: Overlay.overlay
        z: 15001
        modal: false
        focus: false
        padding: Theme.space3
        width: parent ? Math.min(360, parent.width-Theme.space3*2) : 330
        x: parent ? (Theme.rtl ? Theme.space3
            : Math.max(Theme.space3,parent.width-width-Theme.space3)) : 0
        y: Theme.space3
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface { curveDepth: Theme.curveContainer }
        Timer {
            interval: 4600
            repeat: false
            running: genyBuzzFeedbackToast.opened
            onTriggered: genyBuzzFeedbackToast.close()
        }
        contentItem: RowLayout {
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            Icon {
                text: root.buzzFeedbackCode === "sent"
                    ? Theme.iconSpark : Theme.iconWarning
                color: root.buzzFeedbackCode === "sent"
                    ? Theme.positive : Theme.negative
                preferredSize: Theme.iconSize
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.buzzFeedbackCode === "sent"
                    ? qsTrId("genycaster.buzz.sent")
                    : root.buzzFeedbackCode === "buzz_rate_limited"
                      ? qsTrId("genycaster.buzz.slow_down")
                      : root.buzzFeedbackCode === "buzz_not_available"
                        ? qsTrId("genycaster.buzz.not_available")
                        : root.buzzFeedbackCode === "buzz_unavailable"
                          ? qsTrId("genycaster.buzz.service_unavailable")
                          : qsTrId("genycaster.buzz.error")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
            }
        }
    }
    Popup {
        id: genyBuzzToast
        objectName: "genyBuzzIncomingToast"
        parent: Overlay.overlay
        z: 15000
        modal: false
        focus: false
        padding: Theme.space3
        width: parent ? Math.min(340, parent.width-Theme.space3*2) : 320
        x: parent ? (Theme.rtl ? Theme.space3
            : Math.max(Theme.space3,parent.width-width-Theme.space3)) : 0
        y: Theme.space3
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface { curveDepth: Theme.curveContainer }
        onOpened: {
            // One lightweight, non-repeating attention pulse; no scene blur,
            // window shake, CPU-heavy shadow or persistent animation.
            if (!Theme.reduceMotion && !Theme.powerSave)
                genyBuzzIconPulse.restart()
        }
        onClosed: root.showNextBuzz()
        SequentialAnimation {
            id: genyBuzzIconPulse
            running: false
            NumberAnimation {
                target: genyBuzzToastGlyph
                property: "scale"
                from: 0.88
                to: 1.12
                duration: 110
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: genyBuzzToastGlyph
                property: "scale"
                to: 1.0
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Timer {
            interval: 4800
            repeat: false
            running: genyBuzzToast.opened
            onTriggered: genyBuzzToast.close()
        }
        contentItem: RowLayout {
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            Icon {
                id: genyBuzzToastGlyph
                text: Theme.iconNotification
                color: Theme.accent
                preferredSize: Theme.iconSizeLarge
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.buzz.received")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.buzz.from_fid").arg(
                        String(root.pendingBuzz.sender_fid || ""))
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
            }
            ActionButton {
                compact: true
                quiet: true
                text: qsTrId("genycaster.buzz.view")
                onClicked: {
                    const fid=Number(root.pendingBuzz.sender_fid || 0)
                    genyBuzzToast.close()
                    if (fid>0 && isFinite(fid)) {
                        root.clearPendingShellReturn()
                        root.shellRoute = ""
                        presentationBridge.openProfileFid(fid)
                    }
                }
            }
        }
    }

    InitialSetup {
        id: initialSetup
        anchors.fill: parent
        z: 10_000
        visible: root.setupReviewOpen || (presentationBridge.authenticationStateObserved
                 && presentationBridge.initialSetupRequired)
        reviewMode: root.setupReviewOpen && !presentationBridge.initialSetupRequired
        onCloseReviewRequested: root.setupReviewOpen = false
        runtimeReady: presentationBridge.runtimeReady
        authenticationAvailable: presentationBridge.authenticationAvailable
        authenticated: presentationBridge.isAuthenticated
        setupCompleted: presentationBridge.initialSetupCompleted
                        && !presentationBridge.initialSetupRequired
        returningAfterLogout: presentationBridge.initialSetupCompleted
                             && presentationBridge.initialSetupRequired
        legalConsentAccepted: presentationBridge.legalConsentAccepted
        authenticationPhase: presentationBridge.authenticationPhase
        authenticationRoute: presentationBridge.authenticationRoute
        authenticationApprovalAvailable: presentationBridge.authenticationApprovalAvailable
        authenticationErrorText: presentationBridge.authenticationErrorText
        setupConnections: presentationBridge.setupConnections
        onCheckSetupConnections: presentationBridge.checkSetupConnections()
        signerStatusText: presentationBridge.signerStatusText
        signerErrorText: presentationBridge.signerErrorText
        signerBusy: presentationBridge.signerBusy
        signerReady: presentationBridge.writeReady
        accountName: presentationBridge.accountName
        accountUsername: presentationBridge.accountUsername
        accountAvatarUrl: presentationBridge.accountAvatarUrl
        accountIdentityIssuer: presentationBridge.accountIdentityIssuer
        onContinueWithGenyleap: presentationBridge.beginSignIn()
        onContinueWithFarcaster: presentationBridge.beginFarcasterSignIn()
        onOpenApproval: presentationBridge.openAuthenticationApproval()
        onCancelAuthentication: presentationBridge.cancelSignIn()
        onAuthorizeSigner: root.requestInteractionAuthorization()
        onNativeAppDownloadsRequested: presentationBridge.openBrowserNativeDownloads()
        onRecoverAccount: (protocol) =>
            presentationBridge.beginBrowserProtocolSetup(protocol, false)
        onBrowserCreateAccount: (protocol) =>
            presentationBridge.beginBrowserProtocolSetup(protocol, true)
        onNativeRecoverAccount: (recoveryWords) =>
            presentationBridge.beginNativeRecoverySetup(recoveryWords)
        onNativeRecoverHyperSnapAccount: (recoveryWords, authorizeSigner) =>
            presentationBridge.beginNativeHyperSnapRecoverySetup(recoveryWords, authorizeSigner)
        onNativeRecoverFarcasterAccount: (recoveryWords, authorizeSigner) =>
            presentationBridge.beginNativeFarcasterRecoverySetup(recoveryWords, authorizeSigner)
        onAcceptLegalConsent: (termsAccepted, privacyAcknowledged, reviewCompleted) =>
            presentationBridge.acceptLegalConsent(termsAccepted, privacyAcknowledged, reviewCompleted)
        onFinishSetup: presentationBridge.completeInitialSetup()
    }

    // Compiled only for Android. The Android resource imports QtWebView,
    // so desktop/WASM do not acquire an unavailable platform dependency.
    Loader {
        id: androidMiniAppHostLoader
        active: Qt.platform.os === "android"
        source: active ? "qrc:/qt/qml/Farcaster/Client/components/AndroidMiniAppWebView.qml" : ""
    }

    // SDK swap requests never share the untrusted embedded browser surface.
    MiniAppSwapPreflight {
        id: miniAppSwapPreflight
        parent: Overlay.overlay
        blurSourceItem: shellContent
        compact: root.mobile
    }

    // A third-party Mini App may only authenticate as the active Farcaster FID
    // when the connected EVM wallet is that FID's custody or active auth address.
    // Expose an actionable first-party explanation instead of pretending that
    // an unverifiable signature succeeded or only logging a generic rejection.
    GenyAdaptiveDialog {
        id: miniAppSignInFailureDialog
        parent: Overlay.overlay
        blurSourceItem: shellContent
        popupZ: 22000
        contentPadding: Theme.space4
        dimBackground: true
        property string suspendedTaskId: ""
        width: root.mobile ? (parent ? parent.width : 440) : 490
        height: root.mobile ? 300 : 270
        mobileHeightRatio: 0.82
        closePolicy: Popup.CloseOnEscape
        onDidClose: {
            const taskId = suspendedTaskId
            suspendedTaskId = ""
            miniAppHost.dismissMiniAppSignInVerificationFailed()
            // Native macOS drawers are AppKit NSViews above the Qt scene.
            // Restore the SAME Mini App after the QML modal has dismissed,
            // never navigate to an unrelated app or retain wallet authority.
            if (taskId.length > 0)
                miniAppTaskManager.focusTask(taskId)
        }

        contentItem: ColumnLayout {
            spacing: Theme.space3
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.miniapp.signin_wallet_title")
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
                wrapMode: Text.WordWrap
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTrId("genycaster.miniapp.signin_wallet_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeLabelSize
                wrapMode: Text.WordWrap
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTrId("genycaster.locale.close")
                onClicked: miniAppHost.dismissMiniAppSignInVerificationFailed()
            }
        }

        Connections {
            target: miniAppHost
            function onMiniAppSignInVerificationFailedChanged() {
                if (miniAppHost.miniAppSignInVerificationFailed) {
                    // A QML Popup cannot cover the native WKWebView drawer.
                    // Minimize that exact runtime-owned session first, then
                    // open the first-party explanation on the visible shell.
                    const current = (miniAppTaskManager.tasks || []).find(
                        task => task.foreground === true)
                    if (current && String(current.taskId || "").length > 0) {
                        miniAppSignInFailureDialog.suspendedTaskId = String(current.taskId)
                        miniAppTaskManager.minimizeTask(String(current.taskId))
                    }
                    miniAppSignInFailureDialog.open()
                } else {
                    miniAppSignInFailureDialog.close()
                }
            }
        }
    }

    WalletBrowserPicker {
        id: walletBrowserPicker
        parent: Overlay.overlay
    }

    WalletBrowserPicker {
        id: proWalletBrowserPicker
        parent: Overlay.overlay
        walletHost: proWalletHost
    }

    TaskManagerPopup {
        id: taskManagerPopup
        parent: Overlay.overlay
    }

    RenderingSettingsPopup {
        id: renderingSettingsPopup
        parent: Overlay.overlay
    }

    FeedbackDialog {
        id: feedbackDialog
        parent: Overlay.overlay
        reportRoute: String(root.shellRoute || root.primaryDestination || "")
    }

    Component.onCompleted: {
        root.notificationBusySnapshot = presentationBridge.notificationsBusy
        presentationBridge.refreshFollowingStories()
    }

    Connections {
        target: presentationBridge

        property string lastAuthenticatedPrincipal: ""
        function onAuthenticationStateChanged() {
            const principal = presentationBridge.isAuthenticated
                ? String(presentationBridge.accountIdentityIssuer) + ":" + String(presentationBridge.accountFid) : ""
            if (principal !== lastAuthenticatedPrincipal) {
                lastAuthenticatedPrincipal = principal
                root.clearPendingShellReturn()
                root.shellRoute = ""
                root.primaryDestination = "home"
                homeDashboard.draftText = ""
                homeDashboard.scrollToBeginning()
            }
            presentationBridge.refreshFollowingStories()
            if (!presentationBridge.isAuthenticated) {
                root.confirmedReply = ({})
                root.submittedReplyParentId = ""
                root.submittedReplyText = ""
            }
        }

        function onNotificationsChanged() {
            root.notificationBusySnapshot = presentationBridge.notificationsBusy
            if (notificationsRouteLoader.item !== null)
                notificationsRouteLoader.item.loadingNotifications = root.notificationBusySnapshot
        }

        // Queued means accepted for background delivery, not published.
        // Keep the draft visible until Snapchain confirms the cast.

        function onStateChanged() {
            const removedId = String(presentationBridge.lastRemovedCastId || "")
            if (presentationBridge.writePhase === "confirmed"
                    && removedId.length > 0
                    && root.confirmedReply
                    && String(root.confirmedReply.castId || "").replace(/^0x/i, "").toLowerCase()
                        === removedId.replace(/^0x/i, "").toLowerCase())
                root.confirmedReply = ({})
            const castId = String(presentationBridge.lastPublishedCastId || "")
            // Streak is not a rich-composer feature: simple casts, replies
            // and scheduled posts use the same confirmed protocol hash.
            // Cloud persistence is queued independently and the backend
            // verifies the hash/FID from the selected protocol before credit.
            if (presentationBridge.isAuthenticated
                    && presentationBridge.writePhase === "confirmed"
                    && /^0x[0-9a-fA-F]{40}$/.test(castId)
                    && root.lastStreakCastId !== String(presentationBridge.accountFid) + ":" + castId) {
                root.lastStreakCastId = String(presentationBridge.accountFid) + ":" + castId
                openProofConnections.recordStreakCast(castId)
            }
            if (presentationBridge.writePhase === "confirmed"
                    && /^0x[0-9a-fA-F]{40}$/.test(castId)
                    && castId !== root.pendingTopicsBaseline
                    && root.pendingCastTopics.length > 0
                    && root.pendingTopicsOwner === Number(presentationBridge.accountFid)) {
                // The protocol write is confirmed before GenyCaster stores its
                // separate, optional topic association. No synthetic cast data.
                openProofConnections.saveCastTopics(castId,root.pendingCastTopics)
                root.pendingCastTopics = []
                root.pendingTopicsBaseline = castId
            }
            if (presentationBridge.writePhase === "failed"
                    || presentationBridge.writePhase === "blocked") {
                root.pendingCastTopics = []
            }
            if (root.awaitingRichCommit
                    && presentationBridge.writePhase === "confirmed"
                    && castId.length > 0
                    && castId !== root.submittedCastBaseline
                    && castId !== root.committedRichCastId) {
                richComposer.commitPublishedCast(castId)
                // Streak writes are handled once above for every confirmed
                // protocol cast, not only this rich-composer completion.
                if (root.homeDraftForCast.length > 0
                        && homeDashboard.draftText === root.homeDraftForCast)
                    homeDashboard.draftText = ""
                root.homeDraftForCast = ""
                root.committedRichCastId = castId
                root.awaitingRichCommit = false
                if (root.submittedReplyParentId.length > 0) {
                    const parentId = root.submittedReplyParentId
                    root.confirmedReply = {
                        "parentCastId": parentId,
                        "castId": castId,
                        "text": root.submittedReplyText,
                        "embeds": root.submittedReplyEmbeds,
                        "authorFid": Number(presentationBridge.accountFid),
                        "author": String(presentationBridge.accountUsername || ""),
                        "displayName": String(presentationBridge.accountName || ""),
                        "avatarUrl": String(presentationBridge.accountAvatarUrl || ""),
                        "createdAtMs": Date.now()
                    }
                    root.shellRoute = ""
                    // Open the original conversation, not the newly authored
                    // reply itself. The confirmed reply is pinned there.
                    presentationBridge.openCast(parentId)
                }
                root.submittedReplyParentId = ""
                root.submittedReplyText = ""
                root.submittedCastBaseline = castId
                if (root.pendingMiniAppComposeRequestId.length > 0) {
                    miniAppHost.completeComposeCast(root.pendingMiniAppComposeRequestId, castId,
                                                    root.pendingMiniAppComposeText,
                                                    root.pendingMiniAppComposeEmbeds)
                    root.pendingMiniAppComposeRequestId = ""
                    root.pendingMiniAppComposeText = ""
                    root.pendingMiniAppComposeEmbeds = []
                }
                if (composerDrawer.opened)
                    composerDrawer.close()
            } else if (root.awaitingRichCommit
                       && (presentationBridge.writePhase === "failed"
                           || presentationBridge.writePhase === "blocked")) {
                // Preserve the editor and draft so the user can retry.
                root.awaitingRichCommit = false
                root.submittedReplyParentId = ""
                root.submittedReplyText = ""
                root.submittedReplyEmbeds = []
            }
        }
    }
}
