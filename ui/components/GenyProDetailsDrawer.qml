import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Product-owned Pro explainer. It never upgrades an account locally:
// membership and wallet balances are verified by the account service.
GenyDrawer {
    id: root
    objectName: "genyProDetailsDrawer"
    property bool compact: false
    readonly property var membership: openProofConnections.proEntitlements || ({})
    readonly property bool eligible: membership.status === "verified"
                                     && membership.tier === "pro"
                                     && membership.eligible === true
    readonly property int minimumGeny: Math.max(1,Number(membership.minimum_geny || 16000))

    bottomSheet: root.compact
    showHandle: root.compact
    dimBackground: true
    width: root.compact ? parent.width : Math.min(460,parent.width*0.94)
    height: root.compact ? Math.min(parent.height*0.9,780) : parent.height
    contentPadding: Theme.space3

    onOpened: openProofConnections.fetchProEntitlements(true)

    contentItem: Item {
        id: viewport
        clip: true

        RowLayout {
            id: fixedHeader
            objectName: "genyProDrawerFixedHeader"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.max(66,implicitHeight)
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                curveDepth: Theme.curveCompact
                fillColor: Theme.proSurface
                borderWidth: 1
                borderColor: Theme.proBorder
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconSpark
                    color: Theme.proAccent
                    preferredSize: Theme.iconSizeLarge
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.title")
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.eligible
                          ? qsTrId("genycaster.pro.verified_wallet").arg(
                              LocaleFormat.formatInteger(root.minimumGeny))
                          : qsTrId("genycaster.pro.requires_pro")
                    font.pixelSize: Theme.typeCaptionSize
                    color: root.eligible ? Theme.proAccent : Theme.textSecondary
                    elide: Text.ElideRight
                }
            }
            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.pro.not_now")
                onClicked: root.close()
            }
        }

        Flickable {
            id: detailsViewport
            objectName: "genyProDrawerScrollableDetails"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: fixedHeader.bottom
            anchors.bottom: fixedActions.top
            anchors.topMargin: Theme.space3
            anchors.bottomMargin: Theme.space3
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: detailsColumn.implicitHeight + Theme.space3
            interactive: contentHeight > height
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: detailsColumn
                width: detailsViewport.width
                spacing: Theme.space3
                Superellipse {
                    Layout.fillWidth: true
                    implicitHeight: summaryColumn.implicitHeight + Theme.space4*2
                    fillColor: Theme.proSurface
                    borderColor: Theme.proBorder
                    borderWidth: 1
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        id: summaryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space4
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.title")
                            color: Theme.proAccent
                            font.pixelSize: Theme.typeTitle3Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.unlock_hint").arg(
                                LocaleFormat.formatInteger(root.minimumGeny))
                            color: Theme.textPrimary
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.typeBodySize
                        }
                        DirectionAwareLabel {
                            visible: root.membership.status === "unavailable"
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.verification_unavailable")
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Repeater {
                    model: [
                        { icon: Theme.iconApps,
                          title: qsTrId("genycaster.miniapp.session_limit"),
                          detail: qsTrId("genycaster.apps.my_miniapps") },
                        { icon: Theme.iconBookmark,
                          title: qsTrId("genycaster.pro.bookmark_collections"),
                          detail: qsTrId("genycaster.pro.requires_pro") },
                        { icon: Theme.iconLink,
                          title: qsTrId("genycaster.pro.connections"),
                          detail: qsTrId("genycaster.pro.connections_hint") },
                        { icon: Theme.iconChart,
                          title: qsTrId("genycaster.pro.market_pulse"),
                          detail: qsTrId("genycaster.pro.market_pulse_hint") },
                        { icon: Theme.iconCompose,
                          title: qsTrId("genycaster.pro.advanced_editor"),
                          detail: qsTrId("genycaster.pro.thread_explanation") }
                    ]
                    delegate: Superellipse {
                        id: feature
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: featureLayout.implicitHeight + Theme.space3*2
                        fillColor: Theme.surfaceRaised
                        borderColor: Theme.borderSubtle
                        borderWidth: Theme.strokeHairline
                        curveDepth: Theme.curveCompact
                        RowLayout {
                            id: featureLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.space3
                            spacing: Theme.space3
                            Icon {
                                text: String(feature.modelData.icon || "")
                                color: Theme.proAccent
                                preferredSize: Theme.iconSize
                                Layout.alignment: Qt.AlignTop
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(feature.modelData.title || "")
                                    color: Theme.textPrimary
                                    wrapMode: Text.Wrap
                                    font.weight: Theme.weightDemiBold
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(feature.modelData.detail || "")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: fixedActions.top
            height: 1
            color: Theme.divider
        }
        RowLayout {
            id: fixedActions
            objectName: "genyProDrawerFixedActions"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.max(56,implicitHeight)
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            ActionButton {
                Layout.fillWidth: true
                compact: true
                quiet: true
                text: qsTrId("genycaster.pro.not_now")
                onClicked: root.close()
            }
            ActionButton {
                Layout.fillWidth: true
                compact: true
                prominent: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.pro.check_status")
                onClicked: openProofConnections.fetchProEntitlements(true)
            }
        }
    }
}
