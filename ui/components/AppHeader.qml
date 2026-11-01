import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool isAuthenticated: false
    property bool writeReady: false
    property bool writeBusy: false
    property bool compact: false
    property bool showBrand: true
    property int notificationUnreadCount: 0
    property bool notificationsSelected: false

    signal accountRequested()
    signal ownProfileRequested()
    signal notificationsRequested()
    signal settingsRequested()

    implicitHeight: root.compact && Qt.platform.os === "android"
                    ? Theme.scaleUi(54) : Theme.appBarHeight
    color: Theme.backgroundPrimary
    layer.enabled: false
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: false

    readonly property int gutter: Theme.gutter(root.width)

    // All brand geometry is explicit, rather than nested RTL RowLayouts:
    // layout mirroring must never distribute the wordmark away from its icon.
    Item {
        id: headerContent
        x: 0
        y: 0
        width: root.width
        height: root.height
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: false

        Item {
            id: brandCluster
            objectName: "appHeaderBrandCluster"
            visible: root.showBrand
            readonly property real symbolSize: Theme.scaleUi(root.compact && Qt.platform.os === "android" ? 28 : 30)
            readonly property real wordmarkWidth: Theme.scaleUi(root.compact ? (Qt.platform.os === "android" ? 122 : 132) : 150)
            readonly property real brandGap: Theme.space2
            width: symbolSize + brandGap + wordmarkWidth
            height: Theme.scaleUi(root.compact && Qt.platform.os === "android" ? 38 : 42)
            x: Theme.rtl ? parent.width - root.gutter - width : root.gutter
            y: (parent.height - height) / 2
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: false

            Image {
                id: brandSymbol
                width: brandCluster.symbolSize
                height: brandCluster.symbolSize
                x: Theme.rtl ? brandCluster.width - width : 0
                y: (brandCluster.height - height) / 2
                LayoutMirroring.enabled: false
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                sourceSize.width: 90
                sourceSize.height: 90
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }

            Item {
                id: brandText
                objectName: "appHeaderWordmarkAndVersion"
                width: brandCluster.wordmarkWidth
                height: brandCluster.height
                x: Theme.rtl ? 0 : brandSymbol.width + brandCluster.brandGap
                y: (brandCluster.height - height) / 2
                LayoutMirroring.enabled: false
                LayoutMirroring.childrenInherit: false

                Image {
                    id: brandWordmark
                    width: parent.width
                    height: Theme.scaleUi(root.compact ? (Qt.platform.os === "android" ? 20 : 22) : 24)
                    x: 0
                    y: 0
                    LayoutMirroring.enabled: false
                    horizontalAlignment: Image.AlignLeft
                    fillMode: Image.PreserveAspectFit
                    source: Theme.dark
                            ? "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark-dark.svg"
                            : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark.svg"
                    sourceSize.width: 450
                    sourceSize.height: 72
                    Accessible.name: qsTrId("genycaster.app.genycaster")
                }

                // The release identifier remains LTR and stable in every locale.
                // CMake owns the numeric version; this candidate is rc1.
                Text {
                    objectName: "appHeaderReleaseVersion"
                    width: parent.width
                    x: 0
                    y: brandWordmark.height + Theme.scaleUi(2)
                    text: "Genycaster v" + (Qt.application.version || "1.0.0-rc1")
                    color: Theme.inkMuted
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightMedium
                    horizontalAlignment: Theme.rtl ? Text.AlignRight : Text.AlignLeft
                    elide: Text.ElideRight
                    renderType: Text.NativeRendering
                }
            }
        }

        RowLayout {
            id: headerActions
            objectName: "appHeaderActions"
            // A free-standing RowLayout may otherwise grow to the width of
            // its containing workspace, scattering individual icon buttons.
            width: implicitWidth
            height: implicitHeight
            x: Theme.rtl ? root.gutter : parent.width - root.gutter - width
            y: (parent.height - height) / 2
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: false
            layoutDirection: Qt.LeftToRight
            spacing: Theme.space1

            ActionButton {
                compact: true
                quiet: true
                selected: root.notificationsSelected
                iconOnly: true
                iconGlyph: Theme.iconNotification
                badgeText: root.notificationUnreadCount > 0 && !root.notificationsSelected
                           ? (root.notificationUnreadCount > 99 ? (LocaleFormat.formatInteger(99) + "+")
                              : LocaleFormat.formatInteger(root.notificationUnreadCount))
                           : ""
                text: qsTrId("genycaster.side_nav.notifications")
                onClicked: root.notificationsRequested()
            }

            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconSettings
                text: qsTrId("genycaster.settings.title")
                Accessible.name: qsTrId("genycaster.settings.open")
                onClicked: root.settingsRequested()
            }

            ActionButton {
                // Signed-in mobile users reach Profile from the bottom tab.
                visible: !root.compact || !root.isAuthenticated
                compact: true
                quiet: root.isAuthenticated
                prominent: !root.isAuthenticated
                iconOnly: root.compact
                iconGlyph: root.isAuthenticated ? Theme.iconProfile : Theme.iconSignIn
                text: root.isAuthenticated
                      ? qsTrId("genycaster.header.account")
                      : qsTrId("genycaster.header.sign_in")
                Accessible.description: root.isAuthenticated
                                        ? qsTrId("genycaster.header.open_your_signed_in_account_session")
                                        : qsTrId("genycaster.header.sign_in_to_interact_on_farcaster")
                onClicked: root.isAuthenticated ? root.ownProfileRequested() : root.accountRequested()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.borderMuted
        opacity: 0.72
    }
}
