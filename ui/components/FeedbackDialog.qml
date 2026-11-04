import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GenyAdaptiveDialog {
    id: root
    objectName: "genycasterFeedbackDialog"
    property string category: "improvement"
    property int sentiment: 3
    property string reportRoute: ""
    property string pendingMessage: ""
    property bool successAcknowledged: false
    readonly property bool canSubmitFeedback: openProofConnections.cloudAuthorizationReady
    readonly property var sentimentModel: [
        { value: 1, icon: Theme.iconMoodFrustrated },
        { value: 2, icon: Theme.iconMoodSad },
        { value: 3, icon: Theme.iconMoodNeutral },
        { value: 4, icon: Theme.iconMoodPositive },
        { value: 5, icon: Theme.iconMoodExcited }
    ]
    readonly property var categoryModel: [
        { id: "bug", label: qsTrId("genycaster.feedback.bug"), icon: Theme.iconWarning },
        { id: "performance", label: qsTrId("genycaster.feedback.performance"), icon: Theme.iconChart },
        { id: "improvement", label: qsTrId("genycaster.feedback.improvement"), icon: Theme.iconFidelity },
        { id: "idea", label: qsTrId("genycaster.feedback.idea"), icon: Theme.iconInfo }
    ]

    contentPadding: root.compactPresentation ? Theme.space3 : Theme.space4
    mobileFullScreen: true
    showHandle: false
    width: root.compactPresentation ? (parent ? parent.width : 420)
           : Math.min(parent ? parent.width - Theme.space6 * 2 : 620, 620)
    height: root.compactPresentation ? (parent ? parent.height : 760)
            : Math.min(parent ? parent.height - Theme.space6 * 2 : 740, 740)

    function feedbackErrorLabel(errorCode) {
        switch (errorCode) {
        case "authentication_required": return qsTrId("genycaster.feedback.authentication_required")
        case "feedback_limit_reached": return qsTrId("genycaster.feedback.limit_reached")
        case "invalid_feedback": return qsTrId("genycaster.feedback.invalid")
        default: return qsTrId("genycaster.feedback.unavailable")
        }
    }

    function statusLabel(value) {
        switch (value) {
        case "submitted": return qsTrId("genycaster.feedback.status_submitted")
        case "triaged": return qsTrId("genycaster.feedback.status_triaged")
        case "accepted": return qsTrId("genycaster.feedback.status_accepted")
        case "resolved": return qsTrId("genycaster.feedback.status_resolved")
        case "rejected": return qsTrId("genycaster.feedback.status_rejected")
        default: return String(value || "")
        }
    }
    onDidOpen: {
        root.successAcknowledged = false
        if (root.canSubmitFeedback) openProofConnections.fetchFeedback()
    }
    Connections {
        target: openProofConnections
        function onFeedbackChanged() {
            if (root.pendingMessage.length > 0 && openProofConnections.feedbackSubmitted) {
                root.pendingMessage = ""
                root.successAcknowledged = true
                feedbackField.text = ""
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space3

        ModalHeader {
            Layout.fillWidth: true
            title: qsTrId("genycaster.feedback.title")
            subtitle: qsTrId("genycaster.feedback.subtitle")
            onCloseRequested: root.close()
        }

        ScrollView {
            id: feedbackViewport
            objectName: "feedbackFormScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: GenyScrollBar {}

            ColumnLayout {
                width: feedbackViewport.availableWidth
                spacing: Theme.space3

                Superellipse {
                    visible: root.canSubmitFeedback
                    Layout.fillWidth: true
                    Layout.preferredHeight: feedbackSummary.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceMuted
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.curveContainer
                    ColumnLayout {
                        id: feedbackSummary
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.feedback.stats")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            uniformCellWidths: true
                            rowSpacing: Theme.space1
                            Repeater {
                                model: [
                                    { value: Number(openProofConnections.feedbackStats.total || 0),
                                      label: qsTrId("genycaster.feedback.stats_total") },
                                    { value: Number(openProofConnections.feedbackStats.accepted || 0),
                                      label: qsTrId("genycaster.feedback.stats_accepted") },
                                    { value: Number(openProofConnections.feedbackStats.resolved || 0),
                                      label: qsTrId("genycaster.feedback.stats_resolved") }
                                ]
                                delegate: ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 2
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: LocaleFormat.formatInteger(parent.modelData.value)
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeTitle3Size
                                        font.weight: Theme.weightBold
                                    }
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: parent.modelData.label
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                            }
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.feedback.points")
                                  .arg(LocaleFormat.formatInteger(
                                    Number(openProofConnections.feedbackStats.points || 0)))
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                }

                ColumnLayout {
                    visible: root.canSubmitFeedback && openProofConnections.feedbackReports.length > 0
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.feedback.recent_reports")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    Repeater {
                        model: openProofConnections.feedbackReports.slice(0, 5)
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(parent.modelData.category || "")
                                color: Theme.ink
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            DirectionAwareLabel {
                                text: root.statusLabel(parent.modelData.status)
                                color: Theme.accent
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feedback.subtitle")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Repeater {
                        model: root.sentimentModel
                        delegate: Superellipse {
                            id: moodButton
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 50
                            fillColor: root.sentiment === modelData.value
                                       ? Theme.primarySurface : Theme.surfaceMuted
                            borderWidth: root.sentiment === modelData.value
                                         ? Theme.strokeEmphasis : Theme.strokeHairline
                            borderColor: root.sentiment === modelData.value
                                         ? Theme.accent : Theme.borderSubtle
                            curveDepth: Theme.curveControl
                            Icon {
                                anchors.centerIn: parent
                                text: moodButton.modelData.icon
                                color: root.sentiment === moodButton.modelData.value
                                       ? Theme.accent : Theme.inkMuted
                                preferredSize: Theme.iconSizeLarge
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: root.sentiment = moodButton.modelData.value }
                        }
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: Theme.space2
                    columnSpacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    Repeater {
                        model: root.categoryModel
                        delegate: Superellipse {
                            id: categoryCard
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            fillColor: root.category === modelData.id
                                       ? Theme.primarySurface : Theme.surfaceMuted
                            borderWidth: root.category === modelData.id
                                         ? Theme.strokeEmphasis : Theme.strokeHairline
                            borderColor: root.category === modelData.id
                                         ? Theme.accent : Theme.borderSubtle
                            curveDepth: Theme.curveControl
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.space2
                                spacing: Theme.space2
                                Icon {
                                    text: categoryCard.modelData.icon
                                    color: root.category === categoryCard.modelData.id
                                           ? Theme.accent : Theme.inkMuted
                                    preferredSize: Theme.iconSizeSmall
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: categoryCard.modelData.label
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: root.category = categoryCard.modelData.id }
                        }
                    }
                }
                Superellipse {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 164
                    fillColor: Theme.editorSurface
                    borderWidth: feedbackField.activeFocus
                                 ? Theme.strokeEmphasis : Theme.strokeHairline
                    borderColor: feedbackField.activeFocus
                                 ? Theme.accent : Theme.borderSubtle
                    curveDepth: Theme.curveLarge
                    DirectionAwareTextArea {
                        id: feedbackField
                        objectName: "feedbackMessageField"
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        placeholderText: qsTrId("genycaster.feedback.placeholder")
                        wrapMode: TextEdit.Wrap
                        background: null
                    }
                }
                DirectionAwareLabel {
                    visible: !root.canSubmitFeedback
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feedback.authentication_required")
                    color: Theme.warning
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    visible: openProofConnections.feedbackError.length > 0
                    Layout.fillWidth: true
                    text: root.feedbackErrorLabel(openProofConnections.feedbackError)
                    color: Theme.warning
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    visible: root.successAcknowledged
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feedback.sent")
                    color: Theme.positive
                    wrapMode: Text.Wrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
        }
        RowLayout {
            objectName: "feedbackFixedActions"
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ActionButton {
                Layout.fillWidth: true
                quiet: true
                text: qsTrId("genycaster.feedback.done")
                onClicked: root.close()
            }
            ActionButton {
                Layout.fillWidth: true
                positive: true
                busy: openProofConnections.feedbackBusy
                iconGlyph: Theme.iconArrowUpRight
                text: qsTrId("genycaster.feedback.submit")
                enabled: root.canSubmitFeedback && !openProofConnections.feedbackBusy
                         && feedbackField.text.trim().length >= 10
                onClicked: {
                    root.successAcknowledged = false
                    root.pendingMessage = feedbackField.text.trim()
                    openProofConnections.submitFeedback(root.category, root.sentiment,
                                                       root.pendingMessage, root.reportRoute)
                }
            }
        }
    }
}
