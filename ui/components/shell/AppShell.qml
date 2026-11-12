import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Farcaster.Client

Item {
    id: root
    objectName: "appShell"

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

    GenyProDetailsDrawer {
        id: genyProDetailsDrawer
        compact: root.compact
        onAccountRequested: root.openAccountPopup()
    }

    readonly property bool isReady: presentationBridge.architectureState.length > 0
    readonly property bool largeDesktop: width >= Theme.breakpointLargeDesktop
    readonly property bool desktop: width >= Theme.breakpointCompactDesktop
    readonly property bool compactDesktop: width >= Theme.breakpointNarrow && width < Theme.breakpointCompactDesktop
    readonly property bool narrow: width >= Theme.breakpointMobile && width < Theme.breakpointNarrow
    readonly property bool mobile: width < Theme.breakpointMobile
    readonly property bool wide: root.desktop
    readonly property bool compact: width < Theme.breakpointNarrow
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
    property string explorerVisualFilter: "nfts"
    // Keep the notifications busy flag as an imperative snapshot. Feeding the
    // bridge property directly into the lazily-instantiated route produced a QML
    // binding cycle during notificationsChanged delivery on Qt 6.11.
    property bool notificationBusySnapshot: false
    readonly property bool localeSettingsOpen: localePopupLoader.item !== null && localePopupLoader.item.opened
    readonly property bool dataStorageSettingsOpen: dataStoragePopupLoader.item !== null && dataStoragePopupLoader.item.opened
    property bool setupReviewOpen: false
    readonly property bool initialSetupOpen: initialSetup.visible
    readonly property bool overlayOpen: root.initialSetupOpen || root.localeSettingsOpen || root.dataStorageSettingsOpen
                                        || composerDrawer.opened || accountPopup.opened
                                        || renderingSettingsPopup.opened || taskManagerPopup.opened || feedbackDialog.opened
                                        || miniAppHost.walletBrowserPickerActive
                                        || proWalletHost.walletBrowserPickerActive
                                             || searchPalette.opened || mobileCreateMenu.opened || contextDrawer.opened
                                             || mediaGallery.opened || signOutConfirm.opened

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
        root.pendingComposerIntent = null
        if (!presentationBridge.writeReady) {
            accountPopup.open()
            return
        }
        // Recheck after the signer event has completed; never publish or
        // open an authorized editor from a stale account transition.
        Qt.callLater(function() {
            if (presentationBridge.isAuthenticated && presentationBridge.writeReady)
                root.openComposer(intent.parentCast, intent.initialAction,
                                  intent.suggestedText, intent.quoteCast,
                                  intent.structuredReport)
        })
    }

    Connections {
        target: presentationBridge
        function onStateChanged() { root.resolvePendingComposer() }
        function onAuthenticationStateChanged() { root.resolvePendingComposer() }
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
        const sameHealthySource = presentationBridge.activeView === "feed"
                                  && presentationBridge.activeFeed === source
                                  && presentationBridge.feedStarted
                                  && !emptySettledSource
        // Explorer projections are sparse. Re-entering a semantic tab must also
        // refresh the source first page; otherwise Devs/NFTs can keep projecting
        // an old cached timeline forever even though the tab itself changed.
        if (!sameHealthySource) {
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
            presentationBridge.openProfileFid(Number(target.substring(5)))
            return
        }
        if (/^@[A-Za-z0-9_]+(?:[.-][A-Za-z0-9_]+)*$/.test(target)) {
            if (presentationBridge.activeView !== "cast"
                    && presentationBridge.activeView !== "profile")
                root.clearPendingShellReturn()
            root.shellRoute = ""
            presentationBridge.openProfileUsername(target.substring(1))
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

    function openComposer(parentCast, initialAction, suggestedText, quoteCast, structuredReport) {
        if (!presentationBridge.isAuthenticated || !presentationBridge.writeReady) {
            if (presentationBridge.isAuthenticated && presentationBridge.signerBusy
                    && presentationBridge.signerPhase === "checking") {
                root.pendingComposerIntent = {
                    "parentCast": parentCast || null,
                    "initialAction": initialAction || "",
                    "suggestedText": suggestedText || "",
                    "quoteCast": quoteCast || null,
                    "structuredReport": Boolean(structuredReport)
                }
                return
            }
            accountPopup.open()
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
        composerDrawer.transientDraft = false
        composerDrawer.suggestedText = String(suggestedText || "")
        composerDrawer.structuredReport = Boolean(structuredReport)
        // Clear the Home quick draft only after a confirmed Cast, never
        // when the full editor is opened or dismissed without publishing.
        root.homeDraftForCast = !composerParent && composerDrawer.suggestedText.length > 0
                                ? composerDrawer.suggestedText : ""
        composerDrawer.suggestedEmbeds = []
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
                displayName: presentationBridge.accountName.length > 0 ? presentationBridge.accountName : presentationBridge.identityName
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
                onAuthenticationRequested: root.openAccountPopup()
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

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                AppHeader {
                    // Breathing room between the device status-safe-area, brand
                    // chrome and the first content card. Safe area itself is
                    // already handled by the WASM viewport, so don't double it.
                    // Android already excludes the OS status bar from the Qt
                    // window. Do not stack an extra 24dp of header padding.
                    Layout.topMargin: root.mobile && Qt.platform.os !== "android" ? Theme.space3 : 0
                    Layout.bottomMargin: root.mobile && Qt.platform.os !== "android" ? Theme.space3 : 0
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

                                    Rectangle {
                                        width: Math.max(44, parent.width * 0.24)
                                        height: parent.height
                                        radius: 1.5
                                        color: Theme.accent
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
                                                    presentationBridge.openProfileFid(fid)
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
                                                onAuthenticationRequested: root.openAccountPopup()
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
                                                    presentationBridge.openProfileFid(fid)
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
                                        // The dashboard contains its own Flickable. Never
                                        // let its desktop intrinsic 790dp height force the
                                        // phone reading viewport beyond the visible pane.
                                        Layout.minimumHeight: 0
                                        Layout.preferredHeight: root.mobile ? 0 : -1
                                        Layout.maximumWidth: 16777215
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                                        Layout.leftMargin: root.compact ? Theme.mobileGutter
                                                           : root.homeStatsWorkspace ? 0 : Theme.space2
                                        Layout.rightMargin: root.compact ? Theme.mobileGutter
                                                            : root.homeStatsWorkspace ? 0 : Theme.space2
                                        displayName: presentationBridge.accountName.length > 0 ? presentationBridge.accountName : presentationBridge.identityName
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
                                        onStatsReportRequested: text => root.openComposer(null, "", text, null, true)
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
                                                    presentationBridge.openProfileFid(fid)
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
                                                onAuthenticationRequested: root.openAccountPopup()
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
                                                    presentationBridge.openProfileFid(fid)
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
                                                accountIssuer: presentationBridge.accountIdentityIssuer
                                                onBackRequested: root.returnFromProfileSurface()
                                                onProRequested: genyProDetailsDrawer.open()
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
                                                    presentationBridge.openProfileFid(fid)
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
                                                onAuthenticationRequested: root.openAccountPopup()
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
                                                    presentationBridge.openProfileFid(fid)
                                                }
                                                onReplyRequested: cast => root.openComposer(cast)
                                                onQuoteRequested: cast => root.openQuoteComposer(cast)
                                                onReactionRequested: (kind, cast, active) =>
                                                    presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                                onRemoveCastRequested: cast =>
                                                    presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                                onExternalRequested: url => root.openCastTextLink(url)
                                                onMediaRequested: (mediaItems, index) => mediaGallery.openAt(mediaItems, index)
                                                onAuthenticationRequested: root.openAccountPopup()
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
                 && !root.initialSetupOpen
                                     && !root.homeComposerFocused
                            z: 800
                            height: Qt.platform.os === "android" && root.mobile ? 74 : 88
                            anchors.left: readingPane.left
                            anchors.right: readingPane.right
                            anchors.bottom: readingPane.bottom
                            blurSourceItem: readingBackdrop
                            glassEnabled: true
                            activeView: presentationBridge.activeView
                            activeFeed: presentationBridge.activeFeed
                            shellRoute: root.shellRoute
                            primaryDestination: root.primaryDestination
                            widgetsOpen: contextDrawer.visible
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
                                    onProfileRequested: fid => presentationBridge.openProfileFid(fid)
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
            presentationBridge.openProfileFid(fid)
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
                blurSourceItem: shellContent
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
                anchors.leftMargin: accountPopup.bottomSheet ? Theme.mobileGutter : Theme.space5
                anchors.rightMargin: accountPopup.bottomSheet ? Theme.mobileGutter : Theme.space5
                anchors.topMargin: accountPopup.bottomSheet ? Theme.space3 : Theme.space5
                anchors.bottomMargin: accountPopup.bottomSheet ? Theme.space3 : Theme.space5
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
                                 : presentationBridge.authenticationRoute === "recovery"
                                 ? qsTrId("genycaster.setup.recovery_verified")
                                 : presentationBridge.authenticationRoute === "farcaster"
                                 ? qsTrId("genycaster.shell.farcaster_sign_in_openproof_description")
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
                        width: accountScroll.availableWidth
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
                                    text: presentationBridge.accountIdentityIssuer === "genycaster:device-local"
                                          ? qsTrId("genycaster.setup.remembered_device_label")
                                          : presentationBridge.authenticationRoute === "recovery"
                                          ? qsTrId("genycaster.setup.recovery_verified")
                                          : presentationBridge.authenticationRoute === "farcaster"
                                          ? qsTrId("genycaster.shell.farcaster_sign_in_openproof_verification")
                                          : qsTrId("genycaster.shell.verified_by_openproof")
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
                                                        : presentationBridge.provisionSigner()
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
        signerStatusText: presentationBridge.signerStatusText
        signerErrorText: presentationBridge.signerErrorText
        signerBusy: presentationBridge.signerBusy
        signerReady: presentationBridge.writeReady
        accountName: presentationBridge.accountName
        accountUsername: presentationBridge.accountUsername
        accountAvatarUrl: presentationBridge.accountAvatarUrl
        onContinueWithGenyleap: presentationBridge.beginSignIn()
        onContinueWithFarcaster: presentationBridge.beginFarcasterSignIn()
        onOpenRegistration: presentationBridge.openExternalUrl("https://farcaster.xyz/~/signup")
        onOpenApproval: presentationBridge.openAuthenticationApproval()
        onCancelAuthentication: presentationBridge.cancelSignIn()
        onAuthorizeSigner: presentationBridge.provisionSigner()
        onRecoverAccount: presentationBridge.beginRecoverySetup()
        onNativeRecoverAccount: (recoveryWords) =>
            presentationBridge.beginNativeRecoverySetup(recoveryWords)
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

        function onAuthenticationStateChanged() {
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
                // The server independently verifies this already-confirmed
                // protocol hash before awarding Streak progress.
                if (root.lastStreakCastId !== castId
                        && root.pendingTopicsOwner === Number(presentationBridge.accountFid)) {
                    root.lastStreakCastId = castId
                    openProofConnections.recordStreakCast(castId)
                }
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
