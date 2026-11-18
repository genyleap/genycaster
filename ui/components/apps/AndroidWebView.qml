import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebView
import Farcaster.Client

// Android's public system WebViews are isolated from Farcaster signer/wallet
// authority. They are kept alive in separate native surfaces, with exactly
// one visible tab; server-verified GENY Pro lifts the local tab cap to four.
Popup {
    id: root
    objectName: "androidMiniAppWebView"
    parent: Overlay.overlay
    readonly property string activePresentation: miniAppHost.presentationMode
    property bool miniAppFullscreen: false
    readonly property bool wideSurface: activePresentation === "window"
    x: miniAppFullscreen || !parent ? 0
        : activePresentation === "right" ? parent.width - width
        : activePresentation === "window" || activePresentation === "bottom"
          ? (parent.width - width) / 2 : 0
    y: miniAppFullscreen || !parent ? 0
        : activePresentation === "bottom" ? parent.height - height
        : activePresentation === "window" ? (parent.height - height) / 2 : 0
    width: parent ? (miniAppFullscreen ? parent.width
        : activePresentation === "window" ? Math.min(parent.width - Theme.space4 * 2, 600)
        : activePresentation === "bottom" ? parent.width
        : Math.min(parent.width * 0.9, 530)) : 360
    height: parent ? (miniAppFullscreen ? parent.height
        : activePresentation === "window" ? Math.min(parent.height - Theme.space4 * 2, 780)
        : activePresentation === "bottom" ? parent.height * 0.86
        : parent.height) : 640
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape

    property string launchUrl: ""
    property string launchTitle: ""
    property string loadError: ""
    property int activeSessionIndex: -1
    ListModel { id: miniAppSessions }

    // QtWebView exposes URL changes but no pre-navigation veto on Android.
    // This public-only fallback must never gain signer/wallet authority:
    // immediately clear a document if it leaves its launch origin.
    function publicOrigin(candidate) {
        const value = String(candidate || "").trim()
        const match = /^https:\/\/([a-z0-9](?:[a-z0-9.-]*[a-z0-9])?)(?::443)?(?=[\/?#]|$)/i.exec(value)
        if (!match) return ""
        const host = match[1].toLowerCase()
        if (host.indexOf(".") < 1 || host.includes("..")
                || /^(?:[0-9]+\.){3}[0-9]+$/.test(host)
                || host.endsWith(".") || host.endsWith(".local")
                || host.endsWith(".internal") || host.endsWith(".localhost")
                || host.endsWith(".test") || host.endsWith(".invalid"))
            return ""
        return "https://" + host
    }

    function blockForeignNavigation(index, destination) {
        if (index < 0 || index >= miniAppSessions.count) return true
        const session = miniAppSessions.get(index)
        if (Boolean(session.blocked)) return true
        const original = root.publicOrigin(session.url)
        const target = root.publicOrigin(destination)
        if (original.length > 0 && target === original) return false
        // Hide and clear the navigated WebView, rather than resetting the
        // URL to the launch page and accidentally entering a redirect loop.
        miniAppSessions.setProperty(index, "blocked", true)
        if (index === root.activeSessionIndex) {
            root.loadError = qsTrId("genycaster.miniapp.unavailable")
            miniAppHost.setState(true, root.loadError)
        }
        return true
    }

    background: Superellipse {
        fillColor: Theme.surface
        curveDepth: root.miniAppFullscreen ? 0 : Theme.scaleUi(18)
        exponent: 2
        borderWidth: root.miniAppFullscreen ? 0 : 1
        borderColor: Theme.borderMuted
    }

    function activate(index) {
        if (index < 0 || index >= miniAppSessions.count) return
        activeSessionIndex = index
        const item = miniAppSessions.get(index)
        root.launchUrl = item.url
        root.launchTitle = item.title
        root.loadError = Boolean(item.blocked)
            ? qsTrId("genycaster.miniapp.unavailable") : ""
        miniAppHost.setState(true, root.loadError)
    }

    function removeSession(index) {
        if (index < 0 || index >= miniAppSessions.count) return
        const previous = activeSessionIndex
        miniAppSessions.remove(index)
        if (!miniAppSessions.count) {
            root.dismiss()
            return
        }
        const next = index < previous ? previous - 1
            : index === previous ? Math.min(index, miniAppSessions.count-1)
            : previous
        root.activate(next)
    }

    function dismiss() {
        root.close()
        miniAppHost.close()
    }

    onClosed: {
        miniAppSessions.clear()
        activeSessionIndex = -1
        launchUrl = ""
        loadError = ""
        miniAppHost.setState(false)
        miniAppFullscreen = false
    }

    Connections {
        target: miniAppHost
        function onLaunchRequested(url, title, iconSource) {
            const name = String(title || url)
            for (let i=0;i<miniAppSessions.count;i++) {
                if (miniAppSessions.get(i).url === url) {
                    // Only an explicit new launch can replace a blocked
                    // document; never silently reattach its old WebView.
                    if (Boolean(miniAppSessions.get(i).blocked)) {
                        miniAppSessions.remove(i)
                        break
                    }
                    root.open()
                    root.activate(i)
                    return
                }
            }
            if (miniAppSessions.count >= miniAppHost.sessionLimit) {
                root.loadError = qsTrId("genycaster.miniapp.session_limit")
                miniAppHost.setState(root.opened,root.loadError)
                return
            }
            miniAppSessions.append({url:String(url), title:name, blocked:false})
            root.open()
            root.activate(miniAppSessions.count-1)
        }
        function onCloseRequested() { root.close() }
        function onCloseAllRequested() { root.close() }
        function onSessionLimitChanged() {
            while (miniAppSessions.count > miniAppHost.sessionLimit) {
                // Keep the foreground app after a downgrade. Never hold extra
                // WebViews open without the latest verified entitlement.
                root.removeSession(activeSessionIndex === 0
                    ? miniAppSessions.count-1 : 0)
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.opened
        onActivated: {
            if (root.miniAppFullscreen) root.miniAppFullscreen = false
            else root.dismiss()
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        Rectangle {
            visible: !root.miniAppFullscreen
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Math.max(52, 44 + (root.parent ? root.parent.SafeArea.margins.top : 0)) : 0
            color: Theme.surface
            RowLayout {
                layoutDirection: Theme.uiDirection
                anchors.fill: parent
                anchors.topMargin: root.parent ? root.parent.SafeArea.margins.top : 0
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space2
                ActionButton {
                    iconOnly: true; compact: true; quiet: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.close")
                    onClicked: root.dismiss()
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.launchTitle
                    elide: Text.ElideMiddle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                ActionButton {
                    iconOnly: true; compact: true; quiet: true
                    iconGlyph: Theme.iconExpand
                    text: qsTrId("genycaster.player.fullscreen")
                    onClicked: root.miniAppFullscreen = true
                }
                ActionButton {
                    iconOnly: true; compact: true; quiet: true
                    iconGlyph: Theme.iconRefresh
                    text: qsTrId("genycaster.feed_header.refresh")
                    onClicked: {
                        if (root.activeSessionIndex < 0
                                || Boolean(miniAppSessions.get(root.activeSessionIndex).blocked))
                            return
                        const current=liveWebViews.itemAt(root.activeSessionIndex)
                        if (current) current.reload()
                    }
                }
            }
        }
        Rectangle { visible: !root.miniAppFullscreen; Layout.fillWidth: true; Layout.preferredHeight: visible ? 1 : 0; color: Theme.divider }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: miniAppSessions.count > 1 ? Theme.scaleUi(43) : 0
            visible: miniAppSessions.count > 1 && !root.miniAppFullscreen
            contentWidth: tabRow.implicitWidth
            contentHeight: height
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            Row {
                id: tabRow
                spacing: Theme.space1
                height: parent.height
                Repeater {
                    model: miniAppSessions
                    Row {
                        spacing: 0
                        height: parent.height
                        ActionButton {
                            compact: true
                            quiet: index !== root.activeSessionIndex
                            text: String(model.title)
                            onClicked: root.activate(index)
                        }
                        ActionButton {
                            iconOnly: true; compact: true; quiet: true
                            iconGlyph: Theme.iconClose
                            text: qsTrId("genycaster.shell.close")
                            onClicked: {
                                const tabIndex = index
                                Qt.callLater(() => root.removeSession(tabIndex))
                            }
                        }
                    }
                }
            }
        }
        DirectionAwareLabel {
            visible: root.loadError.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space3
            Layout.rightMargin: Theme.space3
            text: root.loadError
            color: Theme.danger
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ActionButton {
                z: 5
                visible: root.miniAppFullscreen
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.space3
                iconOnly: true; compact: true
                iconGlyph: Theme.iconCompress
                text: qsTrId("genycaster.player.exit_fullscreen")
                onClicked: root.miniAppFullscreen = false
            }
            Repeater {
                id: liveWebViews
                model: miniAppSessions
                WebView {
                    anchors.fill: parent
                    visible: root.opened && index === root.activeSessionIndex
                             && !Boolean(model.blocked)
                    url: model.blocked ? "" : String(model.url)
                    onUrlChanged: function(nextUrl) {
                        if (Boolean(model.blocked) || String(nextUrl).length === 0)
                            return
                        if (root.blockForeignNavigation(index, nextUrl)) stop()
                    }
                    onLoadingChanged: function(request) {
                        if (index !== root.activeSessionIndex || Boolean(model.blocked))
                            return
                        if (request.status === WebView.LoadFailedStatus) {
                            root.loadError = request.errorString ||
                                qsTrId("genycaster.miniapp.unavailable")
                            miniAppHost.setState(true, root.loadError)
                        } else if (request.status === WebView.LoadSucceededStatus) {
                            root.loadError = ""
                            miniAppHost.setState(true)
                        }
                    }
                }
            }
        }
    }
}
