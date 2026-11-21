import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Rectangle {
    id: root

    property NeonReflectionSource reflectionSource: null
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
    signal feedbackRequested()
    signal settingsRequested()

    implicitHeight: root.compact ? Theme.scaleUi(54) : Theme.appBarHeight
    clip: false
    color: Theme.backgroundPrimary
    layer.enabled: false
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: false

    readonly property int gutter: Theme.gutter(root.width)

    // Keep the diffuse NEON tint but remove the hard chromatic
    // bottom-edge pass. That 1px divider was sampling the first row of
    // scrolling media and painting a cropped strip across the header.
    NeonReflection {
        anchors.fill: parent
        reflectionSource: root.reflectionSource
        // Restore an appreciable diffuse content-derived chromatic
        // reflection; only the thin cropped edge shader stays disabled.
        strength: Theme.reflectionStrength * (root.compact ? 1.65 : 1.30)
        cornerRadius: 0
        borderInset: 0
        edgeStrength: 0
        bottomEdgeOnly: false
    }

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
            height: Theme.scaleUi(root.compact ? 30 : 42)
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
                source: (Qt.platform.os === "android" ? "image://genycaster-brand/mark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg")
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
                    y: root.compact ? (parent.height - height) / 2 : 0
                    LayoutMirroring.enabled: false
                    horizontalAlignment: Image.AlignLeft
                    fillMode: Image.PreserveAspectFit
                    source: Theme.dark
                            ? (Qt.platform.os === "android" ? "image://genycaster-brand/wordmark-dark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark-dark.svg")
                            : (Qt.platform.os === "android" ? "image://genycaster-brand/wordmark" : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark.svg")
                    sourceSize.width: 450
                    sourceSize.height: 72
                    Accessible.name: qsTrId("genycaster.app.genycaster")
                }

                // No version subtitle in mobile/compact chrome; version details belong in Settings.
                Text {
                    visible: !root.compact
                    objectName: "appHeaderReleaseVersion"
                    width: parent.width
                    x: 0
                    y: brandWordmark.height + Theme.scaleUi(2)
                    text: Qt.application.version ? "v" + LocaleFormat.localizeDigits(Qt.application.version) : ""
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

            // Feedback stays beside notifications on the mobile header; it is
            // not buried in the desktop-only footer or a settings submenu.
            ActionButton {
                reflectionSource: root.reflectionSource
                visible: root.compact
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconFeedback
                text: qsTrId("genycaster.feedback.title")
                onClicked: root.feedbackRequested()
            }

            ActionButton {
                reflectionSource: root.reflectionSource
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
                reflectionSource: root.reflectionSource
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconSettings
                text: qsTrId("genycaster.settings.title")
                Accessible.name: qsTrId("genycaster.settings.open")
                onClicked: root.settingsRequested()
            }

            ActionButton {
                reflectionSource: root.reflectionSource
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

}
