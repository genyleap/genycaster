// Qt WebEngine Quick presentation is kept out of the common QML module:
// macOS/Windows/WASM/Android do not need a QtWebEngine dependency.
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
    popupType: Popup.Item
    modal: false
    dim: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose

    // 1 Bottom, 2 Left, 3 Right; 0 is reserved for native separate windows.
    readonly property int edge: adapter.presentationMode
    width: edge === 1 ? parent.width
           : Math.min(parent.width, Math.max(360, Math.min(820, parent.width * 0.72)))
    height: edge === 1 ? Math.min(parent.height * 0.85, 780) : parent.height
    x: edge === 3 ? parent.width - width : 0
    y: edge === 1 ? parent.height - height : 0

    property bool hostVisible: adapter.initiallyVisible
    property url requestedUrl: ""
    property string outboundScript: ""
    property bool sdkBack: false
    property string statusText: ""
    property alias browserView: web

    onHostVisibleChanged: {
        if (hostVisible) open()
        else close()
    }
    onRequestedUrlChanged: {
        if (requestedUrl.toString().length > 0)
            web.url = requestedUrl
    }
    onOutboundScriptChanged: {
        if (outboundScript.length > 0)
            web.runJavaScript(outboundScript, WebEngineScript.MainWorld)
    }
    onClosed: {
        // A hidden task is still alive; only a user-initiated close tears down
        // the Mini App runtime and its ephemeral WebEngine profile.
        if (hostVisible) adapter.requestClose()
    }
    Component.onCompleted: {
        if (hostVisible) open()
    }

    background: Rectangle {
        color: Theme.surfaceElevated
        border.color: Theme.borderMuted
        border.width: 1
        radius: edge === 1 ? Theme.curveContainer : 0
    }

    WebEngineProfile {
        id: isolatedProfile
        offTheRecord: true
    }

    WebChannel {
        id: channel
        Component.onCompleted: {
            if (root.adapter.miniApp)
                registerObject("genyMiniAppBridge", root.adapter.transport)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.scaleUi(51)
            color: Theme.surfaceRaised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space2

                Button {
                    text: qsTr("Back")
                    flat: true
                    enabled: web.canGoBack || root.sdkBack
                    onClicked: {
                        if (web.canGoBack) web.goBack()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: root.adapter.title
                    elide: Text.ElideRight
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Font.DemiBold
                }
                Label {
                    visible: !root.adapter.miniApp
                    text: root.adapter.hostDomain
                    color: Theme.inkMuted
                    elide: Text.ElideRight
                }
                Button {
                    text: qsTr("Close")
                    flat: true
                    onClicked: root.close()
                }
            }
        }

        WebEngineView {
            id: web
            Layout.fillWidth: true
            Layout.fillHeight: true
            profile: isolatedProfile
            webChannel: root.adapter.miniApp ? channel : null
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
                if (!root.adapter.miniApp) return
                // The public Farcaster shim runs only in the main world and
                // only in the top frame. Qt transport stays in ApplicationWorld.
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
                if (info.status === WebEngineView.LoadStartedStatus)
                    root.adapter.navigationStarted()
                else if (info.status === WebEngineView.LoadFailedStatus)
                    root.adapter.loadFailed()
            }
            onPermissionRequested: function(permission) { permission.deny() }
            onFullScreenRequested: function(request) { request.reject() }
            onFileDialogRequested: function(request) { request.accepted = true }
            onDesktopMediaRequested: function(request) { request.cancel() }
            // An untrusted Mini App cannot open unmediated top-level windows.
            onNewWindowRequested: function(request) { }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.statusText.length > 0 ? Theme.scaleUi(26) : 0
            visible: root.statusText.length > 0
            color: Theme.surfaceRaised
            Label {
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
