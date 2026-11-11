// Linux-only Qt WebEngine Quick host. Presentation geometry mirrors the
// macOS attached drawer: 424px edge panels and a centered 82%-height bottom
// sheet. The runtime/SDK authority remains in the cross-platform C++ Browser.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import QtWebChannel 1.12
import Farcaster.Client

Popup {
    id: root
    required property var adapter
    parent: adapter.hostItem
    objectName: "linuxMiniAppDrawer"
    popupType: Popup.Item
    modal: false
    dim: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose

    // The public Browser interface uses the same physical presentation modes
    // as macOS and Windows: 0 Window, 1 Bottom, 2 Left, 3 Right.
    readonly property int edge: adapter.presentationMode
    readonly property real preferredWidth: Math.min(parent ? parent.width : 424, 424)
    width: preferredWidth
    height: edge === 1
            ? Math.min(parent ? parent.height : 755, 755,
                       Math.max(420, (parent ? parent.height : 755) * 0.82))
            : (parent ? parent.height : 0)
    x: edge === 3 ? (parent ? parent.width : width) - width
       : edge === 1 ? ((parent ? parent.width : width) - width) / 2 : 0
    y: edge === 1 ? (parent ? parent.height : height) - height : 0

    property bool hostVisible: adapter.initiallyVisible
    property url requestedUrl: ""
    property string outboundScript: ""
    property bool sdkBack: false
    property bool pageLoading: false
    property string statusText: ""
    property alias browserView: web
    property double rendererPid: web.renderProcessPid

    onHostVisibleChanged: {
        if (hostVisible) open()
        else close()
    }
    onRequestedUrlChanged: {
        if (requestedUrl.toString().length)
            web.url = requestedUrl
    }
    onOutboundScriptChanged: {
        if (outboundScript.length)
            web.runJavaScript(outboundScript, WebEngineScript.MainWorld)
    }
    onClosed: if (hostVisible) adapter.requestClose()
    onOpened: adapter.requestActivated()
    Component.onCompleted: if (hostVisible) open()

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0; to: 1
                duration: root.adapter.reduceMotion ? 0 : Theme.motionStandard
            }
            NumberAnimation {
                property: root.edge === 1 ? "y" : "x"
                from: root.edge === 1 ? (root.parent ? root.parent.height : root.height)
                    : root.edge === 2 ? -root.width
                    : (root.parent ? root.parent.width : root.width)
                to: root.edge === 1 ? root.y : root.x
                duration: root.adapter.reduceMotion ? 0 : Theme.motionStandard
                easing.type: Easing.OutCubic
            }
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            to: 0
            duration: root.adapter.reduceMotion ? 0 : Theme.motionFast
        }
    }

    background: Rectangle {
        color: Theme.surface
        radius: root.edge === 1 ? Theme.curveContainer : 0
        border.color: Theme.borderMuted
        border.width: 1
    }

    WebEngineProfile {
        id: ephemeralProfile
        offTheRecord: true
    }
    WebChannel {
        id: bridgeChannel
        Component.onCompleted:
            registerObject("genyMiniAppBridge", root.adapter.transport)
    }

    contentItem: ColumnLayout {
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.scaleUi(52)
            color: Theme.surfaceRaised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space2

                ActionButton {
                    iconOnly: true
                    quiet: true
                    compact: true
                    iconGlyph: Theme.iconArrowLeft
                    text: qsTrId("genycaster.profile.back")
                    enabled: web.canGoBack || root.sdkBack
                    onClicked: if (web.canGoBack) web.goBack()
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.adapter.title
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }
                ActionButton {
                    iconOnly: true
                    quiet: true
                    compact: true
                    iconGlyph: Theme.iconRefresh
                    text: qsTrId("genycaster.feed_header.refresh")
                    onClicked: web.reload()
                }
                ActionButton {
                    iconOnly: true
                    quiet: true
                    compact: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.close")
                    onClicked: root.close()
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        WebEngineView {
            id: web
            Layout.fillWidth: true
            Layout.fillHeight: true
            profile: ephemeralProfile
            webChannel: bridgeChannel
            webChannelWorld: WebEngineScript.ApplicationWorld
            backgroundColor: "#101018"
            settings.javascriptCanOpenWindows: false
            settings.localContentCanAccessFileUrls: false
            settings.localContentCanAccessRemoteUrls: false
            settings.playbackRequiresUserGesture: true
            settings.screenCaptureEnabled: false
            settings.navigateOnDropEnabled: false
            activeFocusOnPress: true

            Component.onCompleted: {
                // Scripts are injected at DocumentCreation before navigating.
                // Only the private ApplicationWorld gets the Qt transport.
                userScripts.collection = [
                    {
                        name: "GenyCasterPrivateMiniAppTransport",
                        sourceCode: root.adapter.transportScript,
                        injectionPoint: WebEngineScript.DocumentCreation,
                        worldId: WebEngineScript.ApplicationWorld,
                        runsOnSubFrames: false
                    },
                    {
                        name: "GenyCasterMiniAppSdk",
                        sourceCode: root.adapter.publicScript,
                        injectionPoint: WebEngineScript.DocumentCreation,
                        worldId: WebEngineScript.MainWorld,
                        runsOnSubFrames: false
                    }
                ]
            }
            onNavigationRequested: function(request) {
                if (!root.adapter.allowNavigation(request.url, request.isMainFrame))
                    request.reject()
            }
            onUrlChanged: root.adapter.pageUrlChanged(url)
            onLoadingChanged: function(info) {
                if (info.status === WebEngineView.LoadStartedStatus) {
                    root.pageLoading = true
                    root.adapter.navigationStarted()
                } else if (info.status === WebEngineView.LoadFailedStatus) {
                    root.pageLoading = false
                    root.adapter.loadFailed()
                } else if (info.status === WebEngineView.LoadSucceededStatus) {
                    root.pageLoading = false
                }
            }
            onPermissionRequested: function(permission) { permission.deny() }
            onFullScreenRequested: function(request) { request.reject() }
            onFileDialogRequested: function(request) {
                request.accepted = true
                request.dialogReject()
            }
            // Untrusted documents cannot create independent native windows.
            onNewWindowRequested: function(request) {}
        }

        Rectangle {
            // A bounded progress strip avoids displaying an abrupt blank frame
            // while Chromium constructs a new renderer.
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 2 : 0
            visible: root.pageLoading
            color: Theme.surfaceRaised
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * 0.6
                color: Theme.accent
                SequentialAnimation on x {
                    running: root.pageLoading && !root.adapter.reduceMotion
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: -parent.width * 0.6
                        to: parent.width
                        duration: 950
                    }
                }
            }
        }

        Rectangle {
            visible: root.statusText.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Theme.scaleUi(28) : 0
            color: Theme.surfaceRaised
            DirectionAwareLabel {
                anchors.fill: parent
                leftPadding: Theme.space3
                verticalAlignment: Text.AlignVCenter
                text: root.statusText
                color: Theme.inkMuted
                elide: Text.ElideRight
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }
}
