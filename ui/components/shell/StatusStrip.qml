import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    property string statusText: ""
    property string errorText: ""
    property string diagnosticText: ""
    property bool diagnosticsEnabled: false
    property bool runtimeReady: false
    property bool offline: false
    property bool sourceUnavailable: false
    property bool networkKnown: false
    property bool stale: false
    property bool busy: false
    property bool readStarted: false
    property bool hasUsableContent: false
    property bool detailsExpanded: false

    signal retryRequested()

    readonly property bool hasReadError: errorText.length > 0
    readonly property bool setupFailure: !runtimeReady && hasReadError
    readonly property bool degradedWithContent: hasUsableContent && (offline || stale || hasReadError || sourceUnavailable)
    readonly property bool warningState: setupFailure || ((offline || hasReadError || sourceUnavailable) && !hasUsableContent)
    readonly property bool shouldShow: warningState || degradedWithContent
    readonly property string stateLabel: setupFailure ? qsTrId("genycaster.status.setup_issue")
                                         : offline ? qsTrId("genycaster.status.offline")
                                         : sourceUnavailable ? qsTrId("genycaster.status.source_issue")
                                         : hasReadError ? qsTrId("genycaster.status.data_issue")
                                         : stale ? qsTrId("genycaster.status.saved")
                                         : busy ? qsTrId("genycaster.status.syncing")
                                         : qsTrId("genycaster.status.live")
    readonly property color stateColor: warningState ? Theme.warning : Theme.accent
    readonly property string stateIcon: warningState ? Theme.iconWarning
                                        : offline ? Theme.iconWifi
                                        : sourceUnavailable ? Theme.iconCloud
                                        : busy ? Theme.iconRefresh
                                        : Theme.iconInfo
    readonly property string primaryMessage: setupFailure ? errorText
                                              : offline ? qsTrId("genycaster.status.no_network_connection_showing_saved_data_when_availabl")
                                              : sourceUnavailable && hasUsableContent ? qsTrId("genycaster.status.showing_saved_data_live_refresh_will_retry_automatical")
                                              : hasReadError ? errorText
                                              : stale ? qsTrId("genycaster.status.showing_saved_data_live_refresh_will_retry_automatical")
                                              : statusText

    implicitHeight: contentColumn.implicitHeight + Theme.space2 * 2
    visible: root.shouldShow || opacity > 0.001
    opacity: root.shouldShow ? 1 : 0
    scale: root.shouldShow ? 1 : 0.975
    transformOrigin: Item.Center
    transform: Translate {
        y: root.shouldShow ? 0 : -8
        Behavior on y {
            NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic }
        }
    }
    Accessible.description: qsTrId("genycaster.status.network_value").arg(
                                !root.networkKnown ? qsTrId("genycaster.status.unknown")
                                : root.offline ? qsTrId("genycaster.status.disconnected")
                                               : qsTrId("genycaster.status.reachable"))
                            + " · " + (!root.runtimeReady ? qsTrId("genycaster.status.read_runtime_unavailable")
                                         : root.sourceUnavailable ? qsTrId("genycaster.status.live_source_unavailable")
                                                                  : qsTrId("genycaster.status.live_source_responded"))
                            + " · " + (!root.readStarted && root.runtimeReady ? qsTrId("genycaster.status.ready") : root.stateLabel)

    Behavior on opacity {
        NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic }
    }

    Superellipse {
        anchors.fill: parent
        fillColor: root.warningState ? Theme.warningSurface : Theme.noticeSurface
        borderWidth: 1
        borderColor: root.warningState
                     ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.28)
                     : Theme.noticeBorder
        curveDepth: Theme.curveControl
        elevation: Theme.elevationNone
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        spacing: Theme.space1

        RowLayout {
            Layout.fillWidth: true
            implicitHeight: Theme.compactControlHeight
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                fillColor: root.warningState
                           ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.13)
                           : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                curveDepth: 14
                exponent: 2.01

                Icon {
                    anchors.centerIn: parent
                    text: root.stateIcon
                    color: root.stateColor
                    preferredSize: Theme.typeCaptionSize
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: root.stateLabel
                    color: root.warningState ? Theme.warning : Theme.textPrimary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Theme.textStartAlignment
                    elide: Text.ElideRight
                }

                Label {
                    visible: root.primaryMessage.length > 0
                    Layout.fillWidth: true
                    text: root.primaryMessage
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.textStartAlignment
                    elide: root.detailsExpanded ? Text.ElideNone : Text.ElideRight
                    wrapMode: root.detailsExpanded ? Text.Wrap : Text.NoWrap
                }
            }

            GenySpinner {
                visible: root.busy
                running: visible
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }

            ActionButton {
                visible: root.diagnosticsEnabled && root.diagnosticText.length > 0
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconInfo
                text: root.detailsExpanded ? qsTrId("genycaster.status.hide_details") : qsTrId("genycaster.status.details")
                onClicked: root.detailsExpanded = !root.detailsExpanded
            }

            ActionButton {
                visible: root.runtimeReady && (root.hasReadError || root.sourceUnavailable)
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.status.retry")
                enabled: !root.busy
                onClicked: root.retryRequested()
            }
        }

        DirectionAwareLabel {
            visible: root.diagnosticsEnabled && root.detailsExpanded && root.diagnosticText.length > 0
            Layout.fillWidth: true
            text: root.diagnosticText
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            lineHeight: Theme.typeCaptionLineHeight
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
        }
    }
}
