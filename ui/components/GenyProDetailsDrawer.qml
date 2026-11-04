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
    width: root.compact ? parent.width : Math.min(440,parent.width*0.94)
    height: root.compact ? Math.min(parent.height*0.88,760) : parent.height
    contentPadding: Theme.space4

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
            height: Math.max(56,implicitHeight)
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                curveDepth: Theme.curveCompact
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
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
                          ? qsTrId("genycaster.pro.status_verified")
                          : root.membership.status === "unavailable"
                            ? qsTrId("genycaster.pro.status_unavailable")
                            : qsTrId("genycaster.pro.requires_pro")
                    font.pixelSize: Theme.typeCaptionSize
                    color: root.eligible ? Theme.positive : Theme.textSecondary
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
            anchors.topMargin: Theme.space2
            anchors.bottomMargin: Theme.space2
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
                // Compact eligibility summary: one calm neutral surface,
                // strong numeric hierarchy, and no orange-tinted giant box.
                Superellipse {
                    objectName: "genyProNeutralEligibilitySummary"
                    Layout.fillWidth: true
                    implicitHeight: summaryColumn.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceRaised
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveContainer

                    RowLayout {
                        id: summaryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space3
                        spacing: Theme.space3

                        Superellipse {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            Layout.alignment: Qt.AlignTop
                            curveDepth: Theme.curveCompact
                            fillColor: Theme.surfaceMuted
                            borderWidth: 0
                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconWallet
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeLarge
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space1
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: LocaleFormat.formatInteger(root.minimumGeny) + " GENY"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightBold
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.pro.eligibility_summary").arg(
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
                }

                Repeater {
                    model: [
                        { icon: Theme.iconApps,
                          title: qsTrId("genycaster.pro.feature_miniapps_title"),
                          detail: qsTrId("genycaster.pro.feature_miniapps_desc") },
                        { icon: Theme.iconBookmark,
                          title: qsTrId("genycaster.pro.bookmark_collections"),
                          detail: qsTrId("genycaster.pro.feature_collections_desc") },
                        { icon: Theme.iconLink,
                          title: qsTrId("genycaster.pro.connections"),
                          detail: qsTrId("genycaster.pro.feature_connections_desc") },
                        { icon: Theme.iconChart,
                          title: qsTrId("genycaster.pro.market_pulse"),
                          detail: qsTrId("genycaster.pro.feature_market_desc") },
                        { icon: Theme.iconCompose,
                          title: qsTrId("genycaster.pro.feature_composer_title"),
                          detail: qsTrId("genycaster.pro.feature_composer_desc") }
                    ]
                    delegate: Button {
                        id: feature
                        required property var modelData
                        required property int index
                        readonly property bool expanded: root.expandedFeature === index
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitHeight: Math.max(48, featureLayout.implicitHeight + Theme.space2 * 2)
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
                            fillColor: feature.expanded ? Theme.surfaceMuted
                                : feature.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                            borderColor: Theme.borderSubtle
                            borderWidth: Theme.strokeHairline
                            curveDepth: Theme.curveCompact
                            Behavior on fillColor {
                                enabled: !Theme.reduceMotion
                                ColorAnimation { duration: Theme.motionFast }
                            }
                        }
                        contentItem: RowLayout {
                            id: featureLayout
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Superellipse {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                Layout.alignment: Qt.AlignTop
                                fillColor: Theme.surfaceMuted
                                borderWidth: 0
                                curveDepth: Theme.curveCompact
                                Icon {
                                    anchors.centerIn: parent
                                    text: String(feature.modelData.icon || "")
                                    color: Theme.inkMuted
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
                                    wrapMode: feature.expanded ? Text.WordWrap : Text.NoWrap
                                    maximumLineCount: feature.expanded ? 2 : 1
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
                spinIconWhileBusy: true
                busy: openProofConnections.proEntitlementsBusy
                enabled: !busy
                text: qsTrId("genycaster.pro.check_status")
                onClicked: openProofConnections.fetchProEntitlements(true)
            }
        }
    }
}
