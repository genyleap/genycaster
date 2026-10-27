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

    implicitHeight: Theme.appBarHeight
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
            readonly property real symbolSize: Theme.scaleUi(30)
            readonly property real wordmarkWidth: Theme.scaleUi(root.compact ? 132 : 150)
            readonly property real brandGap: Theme.space2
            width: symbolSize + brandGap + wordmarkWidth
            height: Theme.scaleUi(42)
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
                    height: Theme.scaleUi(root.compact ? 22 : 24)
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
                    Accessible.name: "GenyCaster"
                }

                // Separate visual runs: Persian, punctuation and localized
                // numerals must not be reordered by the surrounding RTL UI.
                Row {
                    id: localizedVersion
                    visible: Theme.rtl
                    x: 0
                    y: brandWordmark.height + Theme.scaleUi(2)
                    spacing: Theme.scaleUi(3)
                    layoutDirection: Qt.RightToLeft
                    LayoutMirroring.enabled: false

                    Text {
                        text: qsTrId("genycaster.release.beta_label")
                        color: Theme.inkMuted
                        font.family: Theme.fontFamilyForText(text)
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightMedium
                        renderType: Text.NativeRendering
                    }
                    Text {
                        text: "·"
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        renderType: Text.NativeRendering
                    }
                    Text {
                        text: qsTrId("genycaster.release.beta_version_number")
                        color: Theme.inkMuted
                        font.family: Theme.fontFamilyForText(text)
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightMedium
                        renderType: Text.NativeRendering
                    }
                }

                Text {
                    visible: !Theme.rtl
                    x: 0
                    y: brandWordmark.height + Theme.scaleUi(2)
                    text: qsTrId("genycaster.release.beta_1_0")
                    color: Theme.inkMuted
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightMedium
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
