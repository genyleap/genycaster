import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebView

// Android's system WebView is a public web-content host, not the macOS
// Farcaster Mini App SDK runtime. Do not inject wallet, signer, account secrets
// or privileged JavaScript objects into untrusted Mini App origins.
// The native WebView occupies its own non-overlapping content region.
Popup {
    id: root
    objectName: "androidMiniAppWebView"
    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 360
    height: parent ? parent.height : 640
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    property string launchUrl: ""
    property string launchTitle: ""
    property string loadError: ""

    background: Rectangle { color: Theme.background }

    function dismiss() {
        root.close()
        miniAppHost.close()
    }

    onClosed: {
        launchUrl = ""
        loadError = ""
        miniAppHost.setState(false)
    }

    Connections {
        target: miniAppHost
        function onLaunchRequested(url, title, iconSource) {
            root.loadError = ""
            root.launchTitle = title || url
            root.launchUrl = url
            root.open()
            miniAppHost.setState(true)
        }
        function onCloseRequested() { root.close() }
        function onCloseAllRequested() { root.close() }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.opened
        onActivated: root.dismiss()
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(52, 44 + (root.parent ? root.parent.SafeArea.margins.top : 0))
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.topMargin: root.parent ? root.parent.SafeArea.margins.top : 0
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space2
                ActionButton {
                    iconOnly: true
                    compact: true
                    quiet: true
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
                    iconOnly: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconRefresh
                    text: qsTrId("genycaster.feed_header.refresh")
                    onClicked: if (browser.item) browser.item.reload()
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.divider }

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

        Loader {
            id: browser
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: root.opened && root.launchUrl.length > 0
            sourceComponent: Component {
                WebView {
                    url: root.launchUrl
                    onLoadingChanged: function(request) {
                        if (request.status === WebView.LoadFailedStatus) {
                            root.loadError = request.errorString ||
                                qsTrId("genycaster.miniapp.unavailable")
                            miniAppHost.setState(false, root.loadError)
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
