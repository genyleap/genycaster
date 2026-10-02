import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "initialSetup"

    property bool runtimeReady: false
    property bool authenticationAvailable: false
    property string authenticationPhase: "guest"
    property string authenticationRoute: "openproof"
    property bool authenticationApprovalAvailable: false
    property string authenticationErrorText: ""

    signal continueWithGenyleap()
    signal continueWithFarcaster()
    signal continueAsGuest()
    signal openApproval()
    signal cancelAuthentication()

    readonly property bool compact: width < 760
    readonly property bool authenticationStarting: authenticationPhase === "starting"
    readonly property bool awaitingApproval: authenticationPhase === "awaiting-approval"
    readonly property bool linkingFarcaster: authenticationPhase === "linking-farcaster"
    readonly property bool authenticationInProgress: authenticationStarting || awaitingApproval || linkingFarcaster

    Rectangle {
        anchors.fill: parent
        color: Theme.background

        Superellipse {
            width: Math.max(root.width * 0.58, 420)
            height: width
            curveDepth: width / 2
            x: root.width - width * 0.62
            y: -height * 0.48
            fillColor: Theme.primarySurface
            borderWidth: 0
            opacity: Theme.dark ? 0.72 : 0.9
        }

        Superellipse {
            width: Math.max(root.width * 0.36, 320)
            height: width
            curveDepth: width / 2
            x: -width * 0.48
            y: root.height - height * 0.52
            fillColor: Theme.primarySurface
            borderWidth: 0
            opacity: Theme.dark ? 0.48 : 0.62
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    ScrollView {
        id: setupScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
        ScrollBar.vertical: GenyScrollBar {}

        ColumnLayout {
            width: Math.min(1120, Math.max(320, setupScroll.availableWidth - Theme.space6 * 2))
            x: Math.max(Theme.space4, (setupScroll.availableWidth - width) / 2)
            spacing: root.compact ? Theme.space5 : Theme.space7

            Item { Layout.preferredHeight: root.compact ? Theme.space5 : Theme.space7 }

            RowLayout {
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                spacing: Theme.space3

                Superellipse {
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 46
                    curveDepth: Theme.curveControl
                    fillColor: Theme.primaryFill
                    borderWidth: 0

                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconLeaf
                        color: Theme.accentInk
                        preferredSize: 23
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.app.genycaster")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightBold
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.verified_by_openproof")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                columns: root.compact ? 1 : 2
                columnSpacing: root.compact ? 0 : Theme.space7
                rowSpacing: Theme.space5

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space5

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.title")
                            color: Theme.ink
                            font.pixelSize: root.compact ? Theme.typeTitle1Size : Theme.typeDisplaySize
                            font.weight: Theme.weightBold
                            lineHeight: 1.02
                            wrapMode: Text.Wrap
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.subtitle")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeBodySize
                            lineHeight: 1.45
                            wrapMode: Text.Wrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Superellipse {
                            Layout.fillWidth: true
                            implicitHeight: verifiedRow.implicitHeight + Theme.space4 * 2
                            curveDepth: Theme.curveContainer
                            fillColor: Theme.surface
                            borderWidth: Theme.strokeHairline
                            borderColor: Theme.borderMuted

                            RowLayout {
                layoutDirection: Theme.uiDirection
                                id: verifiedRow
                                anchors.fill: parent
                                anchors.margins: Theme.space4
                                spacing: Theme.space3

                                Superellipse {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    curveDepth: Theme.curveCompact
                                    fillColor: Theme.primarySurface
                                    borderWidth: 0

                                    Icon {
                                        anchors.centerIn: parent
                                        text: Theme.iconCheck
                                        color: Theme.accent
                                        preferredSize: 18
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space1

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.shell.verified_by_openproof")
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeBodySize
                                        font.weight: Theme.weightDemiBold
                                        wrapMode: Text.Wrap
                                    }

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.shell.openproof_account_farcaster_protocol_identity")
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                        lineHeight: 1.35
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }

                        Superellipse {
                            Layout.fillWidth: true
                            implicitHeight: guestRow.implicitHeight + Theme.space4 * 2
                            curveDepth: Theme.curveContainer
                            fillColor: Theme.surface
                            borderWidth: Theme.strokeHairline
                            borderColor: Theme.borderMuted

                            RowLayout {
                layoutDirection: Theme.uiDirection
                                id: guestRow
                                anchors.fill: parent
                                anchors.margins: Theme.space4
                                spacing: Theme.space3

                                Superellipse {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    curveDepth: Theme.curveCompact
                                    fillColor: Theme.surfaceMuted
                                    borderWidth: 0

                                    Icon {
                                        anchors.centerIn: parent
                                        text: Theme.iconGlobe
                                        color: Theme.inkMuted
                                        preferredSize: 18
                                    }
                                }

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.shell.you_can_keep_reading_genycaster_as_a_guest_sign_in_is_")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    lineHeight: 1.35
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                Superellipse {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: setupCardContent.implicitHeight + Theme.space5 * 2
                    curveDepth: Theme.curveModal
                    fillColor: Theme.surface
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.border
                    shadowEnabled: true

                    ColumnLayout {
                        id: setupCardContent
                        anchors.fill: parent
                        anchors.margins: root.compact ? Theme.space4 : Theme.space5
                        spacing: Theme.space3

                        RowLayout {
                layoutDirection: Theme.uiDirection
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            Superellipse {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                curveDepth: Theme.curveCompact
                                fillColor: Theme.primarySurface
                                borderWidth: 0

                                Icon {
                                    anchors.centerIn: parent
                                    text: Theme.iconSignIn
                                    color: Theme.accent
                                    preferredSize: 15
                                }
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.sign_in_to_interact")
                                color: Theme.ink
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightBold
                                wrapMode: Text.Wrap
                            }
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.shell.you_can_keep_reading_genycaster_as_a_guest_sign_in_is_")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            lineHeight: 1.35
                            wrapMode: Text.Wrap
                        }

                        Superellipse {
                            visible: root.runtimeReady && !root.authenticationAvailable
                            Layout.fillWidth: true
                            implicitHeight: setupUnavailable.implicitHeight + Theme.space3 * 2
                            curveDepth: Theme.curveCompact
                            fillColor: Theme.warningSurface
                            borderWidth: 0

                            DirectionAwareLabel {
                                id: setupUnavailable
                                anchors.fill: parent
                                anchors.margins: Theme.space3
                                text: qsTrId("genycaster.shell.sign_in_is_not_configured_in_this_build_guest_reading_")
                                color: Theme.warning
                                font.pixelSize: Theme.typeCaptionSize
                                lineHeight: 1.3
                                wrapMode: Text.Wrap
                            }
                        }

                        Superellipse {
                            visible: root.authenticationErrorText.length > 0
                            Layout.fillWidth: true
                            implicitHeight: setupError.implicitHeight + Theme.space3 * 2
                            curveDepth: Theme.curveCompact
                            fillColor: Theme.warningSurface
                            borderWidth: 0

                            DirectionAwareLabel {
                                id: setupError
                                anchors.fill: parent
                                anchors.margins: Theme.space3
                                text: root.authenticationErrorText
                                color: Theme.warning
                                font.pixelSize: Theme.typeCaptionSize
                                lineHeight: 1.3
                                wrapMode: Text.Wrap
                            }
                        }

                        RowLayout {
                layoutDirection: Theme.uiDirection
                            visible: root.authenticationStarting
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            GenySpinner {
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.authenticationRoute === "farcaster"
                                      ? qsTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request")
                                      : qsTrId("genycaster.shell.connecting_securely_to_genyleap")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                wrapMode: Text.Wrap
                            }
                        }

                        ColumnLayout {
                            visible: root.awaitingApproval || root.linkingFarcaster
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.linkingFarcaster || root.authenticationRoute === "farcaster"
                                      ? qsTrId("genycaster.shell.approve_the_request_in_a_farcaster_compatible_client_t")
                                      : qsTrId("genycaster.shell.approve_genyleap_sign_in_in_your_browser")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                lineHeight: 1.35
                                wrapMode: Text.Wrap
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: root.linkingFarcaster || root.authenticationRoute === "farcaster"
                                      ? qsTrId("genycaster.shell.open_farcaster")
                                      : qsTrId("genycaster.shell.open_genyleap")
                                prominent: true
                                enabled: root.authenticationApprovalAvailable
                                onClicked: root.openApproval()
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.cancel")
                                quiet: true
                                onClicked: root.cancelAuthentication()
                            }
                        }

                        ColumnLayout {
                            visible: !root.authenticationInProgress
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            ActionButton {
                                objectName: "initialSetupGenyleapButton"
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.continue_with_genyleap")
                                prominent: true
                                iconGlyph: Theme.iconSignIn
                                enabled: root.runtimeReady && root.authenticationAvailable
                                onClicked: root.continueWithGenyleap()
                            }

                            ActionButton {
                                objectName: "initialSetupFarcasterButton"
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.shell.sign_in_with_farcaster")
                                quiet: true
                                iconGlyph: Theme.iconLink
                                enabled: root.runtimeReady && root.authenticationAvailable
                                onClicked: root.continueWithFarcaster()
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.topMargin: Theme.space1
                                implicitHeight: 1
                                color: Theme.divider
                            }

                            ActionButton {
                                objectName: "initialSetupGuestButton"
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.setup.explore_as_guest")
                                quiet: true
                                iconGlyph: Theme.iconExplore
                                onClicked: root.continueAsGuest()
                            }
                        }

                        DirectionAwareLabel {
                            visible: !root.runtimeReady
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.bridge.starting_local_read_runtime")
                            color: Theme.inkSubtle
                            font.pixelSize: Theme.typeCaptionSize
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.space5
                text: qsTrId("genycaster.app.genycaster")
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
