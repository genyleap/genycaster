import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Superellipse {
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
    property bool expanded: false

    signal retryRequested()

    readonly property bool hasReadError: root.errorText.length > 0
    readonly property bool setupFailure: !root.runtimeReady && root.hasReadError
    readonly property bool retryable: root.runtimeReady && (root.hasReadError || root.sourceUnavailable || root.offline)
    readonly property bool warningState: root.setupFailure || root.offline || root.hasReadError || root.sourceUnavailable
    // Diagnostics alone must never expand a healthy Live/Ready card.
    readonly property bool canExpand: root.warningState && (root.retryable || root.diagnosticsEnabled)
    readonly property string stateLabel: root.setupFailure ? qsTrId("genycaster.status.setup_issue")
                                         : root.offline ? qsTrId("genycaster.status.offline")
                                         : root.sourceUnavailable ? qsTrId("genycaster.status.source_issue")
                                         : root.hasReadError ? qsTrId("genycaster.status.data_issue")
                                         : qsTrId("genycaster.status.live")
    readonly property color stateColor: root.warningState ? Theme.warning : Theme.success
    readonly property string stateIcon: root.warningState ? Theme.iconWarning
                                        : root.offline ? Theme.iconWifi
                                        : root.sourceUnavailable ? Theme.iconCloud
                                        : Theme.iconCheck
    readonly property string primaryMessage: root.setupFailure ? root.errorText
                                              : root.offline ? qsTrId("genycaster.status.no_network_connection_showing_saved_data_when_availabl")
                                              : root.sourceUnavailable && root.hasUsableContent
                                                ? qsTrId("genycaster.status.showing_saved_data_live_refresh_will_retry_automatical")
                                              : root.hasReadError ? root.errorText
                                              : ""

    // Network updates must not resize the navigation viewport below this card.
    // Only the user's explicit expansion changes the reserved extent.
    onWarningStateChanged: {
        if (!warningState)
            expanded = false
    }

    readonly property real summaryHeight: Math.max(34, titleMetrics.height + messageMetrics.height)
    readonly property real detailHeight: 152
    implicitHeight: summaryHeight + Theme.space2 * 2 + (root.expanded ? detailHeight + Theme.space2 : 0)
    clip: true
    fillColor: statusHover.hovered ? Theme.surfaceInteractive : Theme.surfaceSecondary
    borderWidth: 0
    curveDepth: Theme.curveControl

    Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }

    FontMetrics {
        id: titleMetrics
        font.family: Theme.fontFamilyForText(qsTrId("genycaster.status.live"))
        font.pixelSize: Theme.typeMetaSize
        font.weight: Theme.weightDemiBold
    }
    FontMetrics {
        id: messageMetrics
        font.family: titleMetrics.font.family
        font.pixelSize: Theme.typeCaptionSize
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: root.summaryHeight
            Layout.maximumHeight: root.summaryHeight
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Item {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34

                Superellipse {
                    anchors.fill: parent
                    fillColor: Qt.rgba(root.stateColor.r, root.stateColor.g, root.stateColor.b, 0.10)
                    borderWidth: 0
                    curveDepth: width / 2
                    exponent: 2.01

                    Icon {
                        anchors.centerIn: parent
                        text: root.stateIcon
                        color: root.stateColor
                        preferredSize: Theme.typeCaptionSize
                    }
                }

                Superellipse {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 9
                    height: 9
                    exponent: 2.01
                    curveDepth: 5
                    fillColor: root.stateColor
                    borderWidth: 2
                    borderColor: root.fillColor
                    opacity: 1

                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space1

                    DirectionAwareLabel {
                        id: statusTitle
                        Layout.fillWidth: true
                        Layout.preferredHeight: titleMetrics.height
                        Layout.maximumHeight: titleMetrics.height
                        text: root.stateLabel
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }

                    DirectionAwareLabel {
                        // Health is already named on the leading label. A
                        // second Live badge adds noise and flashes on recovery.
                        visible: false
                        text: ""
                        color: root.stateColor
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }

                DirectionAwareLabel {
                    id: statusMessage
                    Layout.fillWidth: true
                    Layout.preferredHeight: messageMetrics.height
                    Layout.maximumHeight: messageMetrics.height
                    // A permanent second line prevents transient errors from
                    // inserting/removing a row. Full messages live in details.
                    text: root.primaryMessage.length > 0 ? root.primaryMessage.replace(/\s+/g, " ")
                          : qsTrId("genycaster.status.ready")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
            }

            Icon {
                opacity: root.canExpand || root.expanded ? 1 : 0
                text: root.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                color: Theme.textTertiary
                preferredSize: Theme.typeCaptionSize
            }

            TapHandler {
                enabled: root.canExpand || root.expanded
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.expanded = !root.expanded
            }
        }

        RowLayout {
            visible: root.expanded
            Layout.fillWidth: true
            Layout.minimumHeight: root.detailHeight
            Layout.maximumHeight: root.detailHeight
            Layout.leftMargin: Theme.space1
            Layout.rightMargin: Theme.space1
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            ScrollView {
                id: statusDetails
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AsNeeded }

                DirectionAwareLabel {
                    width: statusDetails.availableWidth
                    text: (root.primaryMessage || root.stateLabel)
                          + (root.warningState && root.diagnosticsEnabled && root.diagnosticText.length > 0
                             ? "\n" + root.diagnosticText : "")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
            }

            ActionButton {
                opacity: root.retryable ? 1 : 0
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.status.retry")
                enabled: root.retryable && !root.busy
                Accessible.ignored: !root.retryable
                onClicked: root.retryRequested()
            }
        }
    }

    HoverHandler { id: statusHover }
    Accessible.role: Accessible.Button
    Accessible.name: root.stateLabel
    Accessible.description: root.primaryMessage
    Accessible.onPressAction: {
        if (root.canExpand || root.expanded)
            root.expanded = !root.expanded
    }
}
