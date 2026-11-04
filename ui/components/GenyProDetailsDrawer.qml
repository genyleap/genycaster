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
    property int expandedFeature: -1

    bottomSheet: root.compact
    showHandle: root.compact
    dimBackground: true
    width: root.compact ? parent.width : Math.min(460,parent.width*0.94)
    height: root.compact ? Math.min(parent.height*0.92,820) : parent.height
    contentPadding: Theme.space3

    onOpened: {
        expandedFeature = -1
        detailsViewport.contentY = 0
        openProofConnections.fetchProEntitlements(true)
    }

    contentItem: Item {
        id: viewport
        clip: true

        RowLayout {
            id: fixedHeader
            objectName: "genyProDrawerFixedHeader"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.max(58,implicitHeight)
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
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
                    font.pixelSize: Theme.typeTitle3Size
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
                spacing: Theme.space2
                Superellipse {
                    Layout.fillWidth: true
                    implicitHeight: summaryColumn.implicitHeight + Theme.space3*2
                    fillColor: Theme.proSurface
                    borderColor: Theme.proBorder
                    borderWidth: 1
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        id: summaryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: LocaleFormat.formatInteger(root.minimumGeny) + " GENY"
                            color: Theme.proAccent
                            font.pixelSize: Theme.typeTitle2Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.unlock_hint").arg(
                                LocaleFormat.formatInteger(root.minimumGeny))
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        DirectionAwareLabel {
                            visible: root.membership.status === "unavailable"
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.verification_unavailable")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
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
                    delegate: Button {
                        id: feature
                        required property var modelData
                        required property int index
                        readonly property bool expanded: root.expandedFeature === index
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitHeight: Math.max(46, featureLayout.implicitHeight + Theme.space2 * 2)
                        leftPadding: Theme.space3
                        rightPadding: Theme.space3
                        topPadding: Theme.space2
                        bottomPadding: Theme.space2
                        flat: true
                        hoverEnabled: !root.compact
                        Accessible.name: String(modelData.title || "")
                        Accessible.description: String(modelData.detail || "")
                        onClicked: root.expandedFeature = expanded ? -1 : index

                        background: Superellipse {
                            fillColor: feature.expanded ? Theme.proSurface
                                : feature.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                            borderColor: feature.expanded ? Theme.proBorder : Theme.borderSubtle
                            borderWidth: Theme.strokeHairline
                            curveDepth: Theme.curveCompact
                        }
                        contentItem: RowLayout {
                            id: featureLayout
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Superellipse {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                Layout.alignment: Qt.AlignTop
                                fillColor: Theme.proSurface
                                borderWidth: 0
                                curveDepth: Theme.curveCompact
                                Icon {
                                    anchors.centerIn: parent
                                    text: String(feature.modelData.icon || "")
                                    color: Theme.proAccent
                                    preferredSize: Theme.iconSizeSmall
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(feature.modelData.title || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                                DirectionAwareLabel {
                                    visible: feature.expanded
                                    Layout.fillWidth: true
                                    text: String(feature.modelData.detail || "")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                            Icon {
                                Layout.alignment: Qt.AlignTop
                                text: feature.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
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
