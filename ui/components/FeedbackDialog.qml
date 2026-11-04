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
    property int activeTab: 0 // 0: new feedback, 1: submitted reports
    readonly property bool canSubmitFeedback: openProofConnections.cloudAuthorizationReady
    readonly property var sentimentModel: [
        { value: 1, icon: Theme.iconMoodFrustrated, tint: "#E66B66" },
        { value: 2, icon: Theme.iconMoodSad, tint: "#E8A65D" },
        { value: 3, icon: Theme.iconMoodNeutral, tint: "#D3AD55" },
        { value: 4, icon: Theme.iconMoodPositive, tint: "#59B89A" },
        { value: 5, icon: Theme.iconMoodExcited, tint: "#62B7C8" }
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

    function categoryLabel(value) {
        for (const item of root.categoryModel) {
            if (item.id === value) return item.label
        }
        return String(value || "")
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
        root.activeTab = 0
        if (root.canSubmitFeedback) openProofConnections.fetchFeedback()
    }
    Connections {
        target: openProofConnections
        function onFeedbackChanged() {
            if (root.pendingMessage.length > 0 && openProofConnections.feedbackSubmitted) {
                root.pendingMessage = ""
                root.successAcknowledged = true
                root.activeTab = 1
                // Submission handler already refreshes the private history.
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

        RowLayout {
            objectName: "feedbackTabBar"
            Layout.fillWidth: true
            spacing: Theme.space4
            Repeater {
                model: [qsTrId("genycaster.feedback.title"),
                        qsTrId("genycaster.feedback.recent_reports")]
                delegate: Button {
                    id: feedbackTab
                    required property int index
                    required property string modelData
                    Layout.fillWidth: false
                    implicitWidth: tabLabel.implicitWidth + Theme.space3 * 2
                    implicitHeight: 40
                    flat: true
                    Accessible.name: modelData
                    contentItem: DirectionAwareLabel {
                        id: tabLabel
                        text: feedbackTab.modelData
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: root.activeTab === feedbackTab.index ? Theme.ink : Theme.inkMuted
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: root.activeTab === feedbackTab.index
                                     ? Theme.weightDemiBold : Font.Normal
                    }
                    background: Item {
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            height: 2
                            radius: 1
                            color: Theme.accent
                            visible: root.activeTab === feedbackTab.index
                        }
                    }
                    onClicked: {
                        root.activeTab = index
                        if (index === 1 && root.canSubmitFeedback)
                            openProofConnections.fetchFeedback()
                    }
                }
            }
            Item { Layout.fillWidth: true }
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
                    visible: root.activeTab === 1 && root.canSubmitFeedback
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
                    visible: root.activeTab === 1 && root.canSubmitFeedback
                             && openProofConnections.feedbackReports.length > 0
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Repeater {
                        // The server bounds this owner-only history to 30 records.
                        model: openProofConnections.feedbackReports
                        delegate: Superellipse {
                            id: reportCard
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: reportBody.implicitHeight + Theme.space3 * 2
                            fillColor: Theme.surfaceMuted
                            borderWidth: Theme.strokeHairline
                            borderColor: Theme.borderSubtle
                            curveDepth: Theme.curveContainer

                            ColumnLayout {
                                id: reportBody
                                anchors.fill: parent
                                anchors.margins: Theme.space3
                                spacing: Theme.space2
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space2
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: root.categoryLabel(reportCard.modelData.category)
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeLabelSize
                                        font.weight: Theme.weightDemiBold
                                    }
                                    DirectionAwareLabel {
                                        text: root.statusLabel(reportCard.modelData.status)
                                        color: Theme.accent
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(reportCard.modelData.message || "")
                                    visible: text.length > 0
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeBodySize
                                    wrapMode: Text.Wrap
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(reportCard.modelData.review_note || "")
                                    visible: text.length > 0
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(reportCard.modelData.created_at || "").slice(0, 10)
                                    visible: text.length > 0
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }
                        }
                    }
                }

                // The dialog header already explains the form; avoid repeating
                // the same sentence between the tabs and sentiment choices.
                RowLayout {
                    visible: root.activeTab === 0
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Repeater {
                        model: root.sentimentModel
                        delegate: Button {
                            id: moodButton
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 50
                            hoverEnabled: true
                            padding: 0
                            flat: true
                            Accessible.name: qsTrId("genycaster.feedback.subtitle")
                            background: Superellipse {
                                fillColor: root.sentiment === moodButton.modelData.value
                                           ? Theme.primarySurface : Theme.surfaceMuted
                                borderWidth: root.sentiment === moodButton.modelData.value
                                             ? Theme.strokeEmphasis : Theme.strokeHairline
                                borderColor: root.sentiment === moodButton.modelData.value
                                             ? Theme.accent : Theme.borderSubtle
                                curveDepth: Theme.curveControl
                            }
                            contentItem: Icon {
                                text: moodButton.modelData.icon
                                color: moodButton.modelData.tint
                                preferredSize: Theme.iconSizeLarge
                            }
                            onClicked: root.sentiment = moodButton.modelData.value
                        }
                    }
                }
                GridLayout {
                    visible: root.activeTab === 0
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: Theme.space2
                    columnSpacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    Repeater {
                        model: root.categoryModel
                        delegate: Button {
                            id: categoryCard
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            hoverEnabled: true
                            padding: 0
                            leftPadding: Theme.space2
                            rightPadding: Theme.space2
                            flat: true
                            Accessible.name: categoryCard.modelData.label
                            background: Superellipse {
                                fillColor: root.category === categoryCard.modelData.id
                                           ? Theme.primarySurface : Theme.surfaceMuted
                                borderWidth: root.category === categoryCard.modelData.id
                                             ? Theme.strokeEmphasis : Theme.strokeHairline
                                borderColor: root.category === categoryCard.modelData.id
                                             ? Theme.accent : Theme.borderSubtle
                                curveDepth: Theme.curveControl
                            }
                            contentItem: RowLayout {
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection
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
                            onClicked: root.category = categoryCard.modelData.id
                        }
                    }
                }
                Superellipse {
                    visible: root.activeTab === 0
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
                visible: root.activeTab === 0
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
