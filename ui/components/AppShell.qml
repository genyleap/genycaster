import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "appShell"

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

    // The iOS keyboard's Done action can dismiss the DOM keyboard without
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
    property string replyTargetErrorText: ""
    property string shellRoute: ""
    // Shell route (e.g. Notifications) that was visible when a cast or profile
    // opened above it. Back from that content restores this route instead of
    // falling through to the Home feed.
    property string pendingReturnShellRoute: ""
    property string committedRichCastId: ""
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
    property string pendingMiniAppComposeRequestId: ""
    property string pendingMiniAppComposeText: ""
    property var pendingMiniAppComposeEmbeds: []
    property string pendingMiniAppShareTaskId: ""
    property string homePresentationMode: "for-you"
    property string homeVisualFilter: "nfts"
    property string primaryDestination: "home"
    property var selectedChannelInfo: ({})
    property string explorerPresentationMode: "for-you"
    property string explorerVisualFilter: "nfts"
    // Keep the notifications busy flag as an imperative snapshot. Feeding the
    // bridge property directly into the lazily-instantiated route produced a QML
    // binding cycle during notificationsChanged delivery on Qt 6.11.
    property bool notificationBusySnapshot: false
    readonly property bool localeSettingsOpen: localePopupLoader.item !== null && localePopupLoader.item.opened
    readonly property bool dataStorageSettingsOpen: dataStoragePopupLoader.item !== null && dataStoragePopupLoader.item.opened
    readonly property bool initialSetupOpen: initialSetup.visible
    readonly property bool overlayOpen: root.initialSetupOpen || root.localeSettingsOpen || root.dataStorageSettingsOpen
                                        || composerDrawer.opened || accountPopup.opened
                                        || renderingSettingsPopup.opened || taskManagerPopup.opened || feedbackDialog.opened
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

    function openAccountPopup() {
        accountPopup.open()
    }

    function openLocaleSettings(section) {
        localePopupLoader.active = true
        const requestedSection = section === undefined ? -1 : Number(section)
        Qt.callLater(function() {
            const popup = localePopupLoader.item
            if (popup === null)
                return
            if (requestedSection >= 0)
                popup.currentSection = requestedSection
            // A normal Settings action starts on the grouped phone index;
            // deep links such as Market Settings open the matching detail.
            if (popup.mobilePresentation)
                popup.mobileIndexOpen = requestedSection < 0
            popup.open()
            popup.scrollToBeginning()
        })
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
        if (root.pendingReturnShellRoute.length === 0)
            return
        root.shellRoute = root.pendingReturnShellRoute
        root.pendingReturnShellRoute = ""
    }

    function returnFromCastDetail() {
        presentationBridge.returnFromDetail()
        root.returnToPendingShellRoute()
    }

    function returnFromProfileSurface() {
        presentationBridge.showFeed()
        root.returnToPendingShellRoute()
    }

    function openMyProfileFromNavigation() {
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
        const source = nextMode === "visuals" ? "nfts"
                     : nextMode === "devs" ? "developers"
                     : nextMode === "conversations" ? "conversations" : "recent"
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
        root.primaryDestination = "explorer"
        root.shellRoute = ""
        // A re-tap navigates within cached rows; it must not trigger an
        // avoidable network refresh or leave the viewport far down the feed.
        root.selectExplorerPresentationMode(root.explorerPresentationMode, false)
        Qt.callLater(() => {
            if (root.primaryDestination === "explorer" && root.shellRoute === ""
                    && feedSurfaceLoader.item)
                feedSurfaceLoader.item.scrollToBeginning()
        })
    }

    function openTrends() {
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

    function openSuggestedProfile(username) {
        root.clearPendingShellReturn()
        root.primaryDestination = "explorer"
        root.shellRoute = ""
        presentationBridge.openSearchQuery(String(username || ""))
    }

    function openQuoteComposer(cast) {
        if (!cast || !/^0x[0-9a-f]{40}$/i.test(String(cast.castId || ""))
                || Number(cast.authorFid || 0) <= 0)
            return
        root.openComposer(null, "quote", "", cast)
    }

    function openComposer(parentCast, initialAction, suggestedText, quoteCast) {
        if (!presentationBridge.isAuthenticated || !presentationBridge.writeReady) {
            accountPopup.open()
            return
        }
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
                displayName: presentationBridge.accountName.length > 0 ? presentationBridge.accountName : presentationBridge.identityName
                avatarUrl: presentationBridge.accountAvatarUrl
                // StoryStrip is an account-only Follow graph, never public Recent authors.
                recentProfiles: presentationBridge.followingStoryProfiles
                ownAvatarUrl: String(presentationBridge.accountAvatarUrl
                                     || presentationBridge.accountProfile.avatarUrl || "")
                authenticated: presentationBridge.isAuthenticated
                interactionsAvailable: presentationBridge.isAuthenticated && presentationBridge.writeReady && !presentationBridge.writeBusy
                compact: root.compact
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
        }
    }


    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    RowLayout {
        id: shellContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: root.mobile ? parent.bottom : globalStatus.top
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
                    onSettingsRequested: root.openLocaleSettings()
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.fill: parent
                        layoutDirection: Theme.uiDirection
                        anchors.leftMargin: root.wide ? Theme.space2 : 0
                        anchors.rightMargin: root.wide ? Theme.space2 : 0
                        anchors.topMargin: root.wide ? Theme.space3 : 0
                        spacing: root.wide ? Theme.space3 : 0

                        Item {
                            id: readingPane
                            // The pane owns the remaining workspace. Individual social surfaces
                            // keep their own bounded reading widths and center themselves inside it.
                            // This mirrors the pre-V20 shell and avoids dead strips between content
                            // and the context rail when the window is wider than the reading column.
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ColumnLayout {
                                id: readingContent
                                anchors.fill: parent
                                // Reserve only the actual footer. The previous
                                // extra bottom gutter left a blank strip above
                                // the dark dock and made clipped casts appear
                                // disconnected from the scrollable viewport.
                                anchors.bottomMargin: root.mobile && !root.homeComposerFocused
                                                      && !root.initialSetupOpen
                                    ? mobileBottomNavigation.height : 0
                                // A mobile ListView must reach the curved dock edge;
                                // extra ColumnLayout spacing otherwise clips its
                                // final visible cast behind a blank gutter.
                                spacing: root.mobile ? 0 : Theme.space3

                                ComingSoonSurface {
                                    visible: root.shellRoute.length > 0 && root.shellRoute !== "notifications" && root.shellRoute !== "apps"
                                             && root.shellRoute !== "bookmarks"
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

                                Loader {
                                    active: root.shellRoute === "bookmarks"
                                    visible: active
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.maximumWidth: root.adaptiveDetailWidth
                                    Layout.alignment: Qt.AlignHCenter
                                    sourceComponent: Component {
                                        BookmarksSurface {
                                            rows: presentationBridge.bookmarkedCasts
                                            compact: root.compact
                                            authenticated: presentationBridge.isAuthenticated
                                            onCastRequested: castId => {
                                                root.rememberShellRouteForReturn()
                                                root.shellRoute = ""
                                                presentationBridge.openCast(castId)
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
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.maximumWidth: root.adaptiveDetailWidth
                                    Layout.alignment: Qt.AlignHCenter

                                    sourceComponent: Component {
                                        AppsSurface {
                                            compact: root.compact
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
                                    id: notificationsRouteLoader
                                    active: root.shellRoute === "notifications"
                                    visible: active
                                    opacity: active ? 1.0 : 0.0
                                    scale: active ? 1.0 : 0.99
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
                                            loadingNotifications: false
                                            hasMore: presentationBridge.notificationsHasMore
                                            errorText: presentationBridge.notificationsErrorText
                                            authenticated: presentationBridge.isAuthenticated
                                            compact: root.compact
                                            accountFid: presentationBridge.accountFid
                                            accountProfile: presentationBridge.accountProfile
                                            onRefreshRequested: presentationBridge.openNotifications()
                                            onLoadMoreRequested: presentationBridge.loadMoreNotifications()
                                            onSignInRequested: root.openAccountPopup()
                                            onProfileRequested: fid => {
                                                root.rememberShellRouteForReturn()
                                                root.shellRoute = ""
                                                presentationBridge.openProfileFid(fid)
                                            }
                                            onCastRequested: castId => {
                                                root.rememberShellRouteForReturn()
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

                                FeedHeader {
                                    visible: root.shellRoute.length === 0 && presentationBridge.activeView === "feed"
                                             && presentationBridge.activeFeed === "channel"
                                    opacity: visible ? 1.0 : 0.0
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: root.adaptiveReadingWidth
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.leftMargin: root.compact ? Theme.mobileGutter : Theme.space2
                                    Layout.rightMargin: root.compact ? Theme.mobileGutter : Theme.space2
                                    activeFeed: presentationBridge.activeFeed
                                    channelInfo: root.selectedChannelInfo
                                    identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
                                    compact: root.compact
                                    busy: presentationBridge.isBusy
                                    feedStarted: presentationBridge.feedStarted
                                    hasContent: presentationBridge.rows.length > 0
                                    sourceUnavailable: presentationBridge.sourceUnavailable
                                    errorText: presentationBridge.errorText
                                    onTrendingRequested: { root.shellRoute = ""; presentationBridge.selectFeed("trending") }
                                    onFollowingRequested: { root.shellRoute = ""; presentationBridge.selectFeed("following") }
                                    onSearchRequested: query => presentationBridge.openSearchQuery(query)
                                    onRefreshRequested: presentationBridge.retry()

                                    Behavior on opacity {
                                        NumberAnimation { duration: Theme.motionEmphasis; easing.type: Easing.OutCubic }
                                    }
                                }

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
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.maximumWidth: 16777215
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                                    Layout.leftMargin: root.compact ? Theme.mobileGutter : Theme.space2
                                    Layout.rightMargin: root.compact ? Theme.mobileGutter : Theme.space2
                                    displayName: presentationBridge.accountName.length > 0 ? presentationBridge.accountName : presentationBridge.identityName
                                    username: presentationBridge.accountUsername
                                    avatarUrl: presentationBridge.accountAvatarUrl
                                    authenticated: presentationBridge.isAuthenticated
                                    busy: presentationBridge.isBusy
                                    notificationUnreadCount: presentationBridge.notificationUnreadCount
                                    rows: presentationBridge.rows
                                    compact: root.compact
                                    onExploreRequested: root.openExplorer()
                                    onComposeRequested: text => root.openComposer(null, "", text)
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
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.maximumWidth: presentationBridge.activeView === "feed" ? root.adaptiveFeedWidth : root.adaptiveDetailWidth
                                    Layout.alignment: Qt.AlignHCenter

                                    sourceComponent: Component {
                                        FeedSurface {
                                            presentationActive: feedSurfaceLoader.surfaceRequested
                                            rows: presentationBridge.rows
                                            // Explorer semantics classify only the live primary feed.
                                            // The legacy art supplement can remain cached for a whole
                                            // session and is therefore intentionally excluded here.
                                            supplementalRows: []
                                            supplementalBusy: false
                                            runtimeReady: presentationBridge.runtimeReady
                                            socialMode: presentationBridge.activeView !== "feed"
                                            busy: presentationBridge.isBusy
                                            hasMore: presentationBridge.hasMore
                                            cachedMore: presentationBridge.cachedFeedHasMore
                                            offline: presentationBridge.isOffline
                                            sourceUnavailable: presentationBridge.sourceUnavailable
                                            errorText: presentationBridge.errorText
                                            activeFeed: presentationBridge.activeFeed
                                            communityFallback: presentationBridge.communityFallback
                                            presentationMode: root.primaryDestination === "explorer"
                                                              ? root.explorerPresentationMode : root.homePresentationMode
                                            visualFilter: root.primaryDestination === "explorer"
                                                          ? root.explorerVisualFilter : root.homeVisualFilter
                                            headerContent: presentationBridge.activeView !== "feed" ? null
                                                           : root.primaryDestination === "explorer" ? explorerFeedExperienceHeaderComponent : null
                                            identityAvailable: presentationBridge.isAuthenticated && root.identityAvailable
                                            feedStarted: presentationBridge.feedStarted
                                            compact: root.compact
                                            interactionsAvailable: presentationBridge.isAuthenticated
                                                                   && presentationBridge.writeReady
                                                                   && !presentationBridge.writeBusy
                                            accountFid: presentationBridge.accountFid
                                            pendingNewPostCount: presentationBridge.activeView === "feed"
                                                                 ? presentationBridge.pendingNewPostCount : 0
                                            onCastRequested: castId => presentationBridge.openCast(castId)
                                            onProfileRequested: fid => presentationBridge.openProfileFid(fid)
                                            onReplyRequested: cast => root.openComposer(cast)
                                            onQuoteRequested: cast => root.openQuoteComposer(cast)
                                            onReactionRequested: (kind, cast, active) =>
                                                presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                            onRemoveCastRequested: cast =>
                                                presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                            onLoadMoreRequested: presentationBridge.loadMore()
                                            onRefreshRequested: root.primaryDestination === "explorer"
                                                ? presentationBridge.refreshLatestFeed() : presentationBridge.retry()
                                            onRevealPendingPostsRequested: presentationBridge.revealPendingPosts()
                                            onExternalRequested: url => presentationBridge.openExternalUrl(url)
                                            onMediaRequested: (mediaItems, index) => mediaGallery.openAt(mediaItems, index)
                                            onRetryRequested: root.primaryDestination === "explorer"
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
                                    id: profileSurfaceLoader
                                    readonly property bool surfaceRequested: root.shellRoute.length === 0
                                                                             && presentationBridge.activeView === "profile"
                                    active: surfaceRequested || item !== null
                                    visible: surfaceRequested
                                    opacity: visible ? 1.0 : 0.0
                                    scale: visible ? 1.0 : 0.99
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.maximumWidth: root.adaptiveProfileWidth
                                    Layout.alignment: Qt.AlignHCenter

                                    sourceComponent: Component {
                                        ProfileSurface {
                                            detail: presentationBridge.detail
                                            rows: presentationBridge.rows
                                            profileFeed: presentationBridge.profileFeed
                                            runtimeReady: presentationBridge.runtimeReady
                                            compact: root.compact
                                            busy: presentationBridge.isBusy
                                            hasMore: presentationBridge.hasMore
                                            offline: presentationBridge.isOffline
                                            sourceUnavailable: presentationBridge.sourceUnavailable
                                            errorText: presentationBridge.errorText
                                            interactionsAvailable: presentationBridge.isAuthenticated
                                                                   && presentationBridge.writeReady
                                                                   && !presentationBridge.writeBusy
                                            accountFid: presentationBridge.accountFid
                                            accountIssuer: presentationBridge.accountIdentityIssuer
                                            onBackRequested: root.returnFromProfileSurface()
                                            onFollowersRequested: presentationBridge.openFollowers()
                                            onFollowingRequested: presentationBridge.openFollowing()
                                            onRefreshRequested: presentationBridge.selectProfileFeed(presentationBridge.profileFeed)
                                            onProfileFeedRequested: feed => presentationBridge.selectProfileFeed(feed)
                                            onCastRequested: castId => presentationBridge.openCast(castId)
                                            onProfileRequested: fid => presentationBridge.openProfileFid(fid)
                                            onReplyRequested: cast => root.openComposer(cast)
                                            onQuoteRequested: cast => root.openQuoteComposer(cast)
                                            onReactionRequested: (kind, cast, active) =>
                                                presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                            onFollowingMutationRequested: (targetFid, following) =>
                                                presentationBridge.setFollowing(targetFid, following)
                                            onRemoveCastRequested: cast =>
                                                presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                            onExternalRequested: url => presentationBridge.openExternalUrl(url)
                                            onMediaRequested: (mediaItems, index) => mediaGallery.openAt(mediaItems, index)
                                            onAuthenticationRequested: root.openAccountPopup()
                                            onLoadMoreRequested: presentationBridge.loadMore()
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
                                    readonly property bool surfaceRequested: root.shellRoute.length === 0
                                                                             && presentationBridge.activeView === "cast"
                                    active: surfaceRequested || item !== null
                                    visible: surfaceRequested
                                    opacity: visible ? 1.0 : 0.0
                                    scale: visible ? 1.0 : 0.99
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.maximumWidth: root.adaptiveCastWidth
                                    Layout.alignment: Qt.AlignHCenter

                                    sourceComponent: Component {
                                        DetailSurface {
                                            activeView: presentationBridge.activeView
                                            detail: presentationBridge.detail
                                            compact: root.compact
                                            busy: presentationBridge.isBusy
                                            interactionsAvailable: presentationBridge.isAuthenticated
                                                                   && presentationBridge.writeReady
                                                                   && !presentationBridge.writeBusy
                                            accountFid: presentationBridge.accountFid
                                            confirmedReply: root.confirmedReply
                                            onBackRequested: root.returnFromCastDetail()
                                            onRefreshRequested: presentationBridge.retry()
                                            onCastRequested: castId => presentationBridge.openCast(castId)
                                            onProfileRequested: fid => presentationBridge.openProfileFid(fid)
                                            onReplyRequested: cast => root.openComposer(cast)
                                            onQuoteRequested: cast => root.openQuoteComposer(cast)
                                            onReactionRequested: (kind, cast, active) =>
                                                presentationBridge.setReaction(kind, Number(cast.authorFid || 0), String(cast.castId || ""), active)
                                            onRemoveCastRequested: cast =>
                                                presentationBridge.removeCast(Number(cast.authorFid || 0), String(cast.castId || ""))
                                            onExternalRequested: url => presentationBridge.openExternalUrl(url)
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


                            ComposerDrawer {
                                id: composerDrawer
                                // On phones the composer must be positioned against the
                                // live Qt viewport, not the readingPane whose geometry
                                // may extend beyond the iOS keyboard.
                                hostItem: root.mobile ? root : readingPane
                                blurSourceItem: shellContent
                                parentCast: root.composerParent
                                onPublishRequested: (text, mentionFids, mentionPositions, embeds, parentUrl) => {
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
                                    root.submittedReplyParentId = root.composerParent
                                        ? String(root.composerParent.castId || "") : ""
                                    root.submittedReplyText = String(text || "")
                                    root.submittedReplyEmbeds = []
                                    for (const item of (embeds || [])) {
                                        const url = String(item && (item.value || item.url) || "")
                                        if (url.length === 0) continue
                                        const isHostedImage = url.startsWith(
                                            "https://media-farcaster.genyleap.com/m/v1/")
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

                        // Edge-to-edge mobile chrome: a dark footer fills the
                        // bottom of the viewport, with a softly curved content
                        // boundary above it. No floating capsule or exterior
                        // light-colored gutter at the device's curved corners.
                        BottomNavigation {
                            id: mobileBottomNavigation
                            // This stays under readingPane; never reparent a
                            // Layout-owned item after layout has run.
                            visible: root.mobile && !root.initialSetupOpen
                                     && !root.homeComposerFocused
                            z: 800
                            height: 88
                            anchors.left: readingPane.left
                            anchors.right: readingPane.right
                            anchors.bottom: readingPane.bottom
                            blurSourceItem: readingContent
                            activeView: presentationBridge.activeView
                            activeFeed: presentationBridge.activeFeed
                            shellRoute: root.shellRoute
                            primaryDestination: root.primaryDestination
                            onTrendingRequested: root.openExplorer()
                            onFollowingRequested: root.openHome()
                            onRouteRequested: route => root.openShellRoute(route)
                            // Primary compose bypasses the secondary action sheet.
                            onCreateRequested: root.openComposer(null)
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

                        Loader {
                            active: root.showHomeContextRail
                            visible: active
                            Layout.fillHeight: true
                            Layout.preferredWidth: active ? root.desktopContextRailWidth : 0
                            Layout.maximumWidth: active ? root.desktopContextRailWidth : 0

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
                            active: root.showSocialContextRail
                            visible: active
                            Layout.fillHeight: true
                            Layout.preferredWidth: active ? root.desktopContextRailWidth : 0
                            Layout.maximumWidth: active ? root.desktopContextRailWidth : 0

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
        visible: !root.mobile
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
        bottomSheet: false
        blurSourceItem: shellContent
        dimBackground: false
        interactive: root.compactDesktop
        width: parent ? Math.min(root.desktopContextRailWidth, parent.width * 0.52) : root.desktopContextRailWidth
        height: parent ? parent.height : 720

        contentItem: ContextRail {
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
        x: Theme.rtl
           ? root.railReservation + Theme.space5
           : Math.max(Theme.space5, root.width - root.railReservation - Theme.space5 - width)
        onComposeRequested: root.openComposer(null)
        onReplyRequested: root.openComposer(presentationBridge.detail)
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
        sourceComponent: Component {
            LocaleSettingsPopup {
                parent: root
                blurSourceItem: shellContent
                onCustomAccentStudioRequested: color => accentColorStudio.openWith(color)
                onDataStorageRequested: root.openDataStorageSettings()
                onSignOutConfirmationRequested: root.requestSignOut()
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

                ModalHeader {
                    Layout.fillWidth: true
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
                    onCloseRequested: accountPopup.close()
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

                        Item { Layout.preferredHeight: Theme.space3 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.divider
                }

                ColumnLayout {
                    id: accountActions
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    ActionButton {
                        visible: presentationBridge.authenticationPhase === "awaiting-approval"
                                 || presentationBridge.authenticationPhase === "linking-farcaster"
                        Layout.fillWidth: true
                        text: presentationBridge.authenticationPhase === "linking-farcaster"
                              || presentationBridge.authenticationRoute === "farcaster"
                              ? qsTrId("genycaster.shell.open_farcaster")
                              : qsTrId("genycaster.shell.open_genyleap")
                        prominent: true
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

                    ActionButton {
                        visible: !presentationBridge.isAuthenticated
                                 && presentationBridge.authenticationPhase !== "starting"
                                 && presentationBridge.authenticationPhase !== "awaiting-approval"
                                 && presentationBridge.authenticationPhase !== "linking-farcaster"
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.continue_with_genyleap")
                        prominent: true
                        enabled: presentationBridge.runtimeReady && presentationBridge.authenticationAvailable
                        onClicked: presentationBridge.beginSignIn()
                    }

                    ActionButton {
                        visible: !presentationBridge.isAuthenticated
                                 && presentationBridge.authenticationPhase !== "starting"
                                 && presentationBridge.authenticationPhase !== "awaiting-approval"
                                 && presentationBridge.authenticationPhase !== "linking-farcaster"
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.sign_in_with_farcaster")
                        quiet: true
                        enabled: presentationBridge.runtimeReady && presentationBridge.authenticationAvailable
                        onClicked: presentationBridge.beginFarcasterSignIn()
                    }

                    ActionButton {
                        visible: presentationBridge.isAuthenticated && !presentationBridge.writeReady
                        Layout.fillWidth: true
                        text: presentationBridge.signerBusy
                              ? qsTrId("genycaster.shell.enabling")
                              : wasmReadOnly
                                ? qsTrId("genycaster.shell.enable_publishing_with_recovery_phrase")
                                : qsTrId("genycaster.shell.enable_interactions")
                        prominent: true
                        // WASM uses its own asynchronous wallet/WebCrypto/Snapchain
                        // signer path. An unavailable *native* keychain must not
                        // disable the browser custody approval workflow.
                        enabled: !presentationBridge.signerBusy
                                 && (wasmReadOnly || presentationBridge.signerPhase !== "unavailable")
                        onClicked: presentationBridge.provisionSigner()
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
        visible: presentationBridge.initialSetupRequired
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
        onAcceptLegalConsent: (termsAccepted, privacyAcknowledged, reviewCompleted) =>
            presentationBridge.acceptLegalConsent(termsAccepted, privacyAcknowledged, reviewCompleted)
        onFinishSetup: presentationBridge.completeInitialSetup()
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
            if (root.awaitingRichCommit
                    && presentationBridge.writePhase === "confirmed"
                    && castId.length > 0
                    && castId !== root.submittedCastBaseline
                    && castId !== root.committedRichCastId) {
                richComposer.commitPublishedCast(castId)
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
