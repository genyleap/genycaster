pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Setup language laboratory. Native controls share the application translator,
// typography and RTL tokens; no separate locale or browser UI is introduced.
Item {
    id: root

    property var model: []
    property int currentIndex: 0
    property string selectedLabel: ""
    property string selectedDetail: ""
    property bool userInteracted: false
    property string suggestedTag: ""
    signal activated(int index, var entry)

    readonly property int itemCount: model && model.length !== undefined ? model.length : 0
    readonly property var currentEntry: currentIndex >= 0 && currentIndex < itemCount
                                        ? model[currentIndex] : null
    readonly property bool narrow: width < 460
    readonly property string filterText: search.text.trim().toLocaleLowerCase()
    readonly property string changedHint: userInteracted
        ? qsTrId("genycaster.setup.language_applies_now") : ""

    function matches(entry) {
        if (!filterText.length)
            return true
        return String(entry.language + " " + entry.hello + " " + entry.tag)
            .toLocaleLowerCase().indexOf(filterText) !== -1
    }

    function choose(index) {
        if (index < 0 || index >= itemCount)
            return
        currentIndex = index
        userInteracted = true
        activated(index, model[index])
    }

    Component.onCompleted: {
        // Keep the OS suggestion stable even after the app translator changes.
        suggestedTag = String(Qt.locale().name).toLowerCase().split(/[_-]/)[0]
    }

    implicitHeight: content.implicitHeight
    LayoutMirroring.enabled: false
    Accessible.name: qsTrId("genycaster.locale.language")
    Accessible.description: selectedLabel

    ColumnLayout {
        id: content
        width: parent.width
        spacing: Theme.space4

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space4
            layoutDirection: Theme.uiDirection

            // The Welcome headline should own this space. Avoid an
            // animated orbital emblem beside the native language selector.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.world_conversations")
                    color: Theme.ink
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: root.narrow ? 24 : 31
                    font.weight: Theme.weightBold
                    lineHeight: 1.15
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Theme.textStartAlignment
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.setup.language_orbit_hint")
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeLabelSize
                    color: Theme.inkMuted
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Theme.textStartAlignment
                }
            }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(48, Theme.touchTarget)
            placeholderText: qsTrId("genycaster.setup.language_search")
            placeholderTextColor: Theme.inkSubtle
            color: Theme.ink
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: Theme.typeLabelSize
            leftPadding: Theme.space4
            rightPadding: Theme.space4
            selectByMouse: true
            background: Superellipse {
                fillColor: Theme.surface
                borderColor: search.activeFocus ? Theme.focus : Theme.borderMuted
                borderWidth: search.activeFocus ? 2 : 1
                curveDepth: Theme.curveControl
            }
            Accessible.name: qsTrId("genycaster.setup.language_search")
            Keys.onEscapePressed: event => {
                search.clear()
                event.accepted = true
            }
        }

        // Break the device suggestion onto a separate line on narrow
        // phones; translated strings must never compete for the same width.
        GridLayout {
            Layout.fillWidth: true
            columns: root.width < 620 ? 1 : 2
            columnSpacing: Theme.space2
            rowSpacing: Theme.space1
            layoutDirection: Theme.uiDirection
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTrId("genycaster.locale.language")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
                wrapMode: Text.WordWrap
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTrId("genycaster.setup.system_suggestion")
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
                visible: root.suggestedTag.length > 0
                wrapMode: Text.WordWrap
                horizontalAlignment: Theme.textStartAlignment
            }
        }

        GridLayout {
            id: languageGrid
            Layout.fillWidth: true
            columns: root.narrow ? 2 : 3
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: root.model

                delegate: Item {
                    id: languageCard
                    required property int index
                    required property var modelData
                    readonly property bool selected: index === root.currentIndex
                    readonly property bool suggested: modelData.tag === root.suggestedTag
                    // Each language card follows its *own* script direction,
                    // regardless of the selected UI language.
                    readonly property bool nativeRtl: ["fa", "ar", "he", "ur", "ps"].indexOf(
                        String(modelData.tag || "").toLowerCase().split(/[-_]/)[0]) >= 0
                    visible: root.matches(modelData)
                    Layout.fillWidth: true
                    Layout.preferredHeight: 76
                    Layout.minimumWidth: 0
                    activeFocusOnTab: visible
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.language
                    Accessible.checked: selected
                    Accessible.description: suggested
                        ? qsTrId("genycaster.setup.system_suggestion") : ""

                    Superellipse {
                        anchors.fill: parent
                        fillColor: languageCard.selected ? Theme.surfaceSelected
                            : cardHover.hovered ? Theme.surfaceHover : Theme.surface
                        borderColor: languageCard.selected ? Theme.accent
                            : languageCard.activeFocus ? Theme.focus : Theme.borderMuted
                        borderWidth: languageCard.selected || languageCard.activeFocus ? 2 : 1
                        curveDepth: Theme.curveControl
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: languageCard.nativeRtl ? Qt.RightToLeft : Qt.LeftToRight
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: languageCard.modelData.language
                                font.family: Theme.fontFamilyForText(text)
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                color: Theme.ink
                                elide: Text.ElideRight
                                horizontalAlignment: languageCard.nativeRtl ? Text.AlignRight : Text.AlignLeft
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: languageCard.modelData.hello
                                Layout.minimumWidth: 0
                                font.family: Theme.fontFamilyForText(text)
                                font.pixelSize: Theme.typeCaptionSize
                                color: Theme.inkMuted
                                elide: Text.ElideRight
                                horizontalAlignment: languageCard.nativeRtl ? Text.AlignRight : Text.AlignLeft
                            }
                        }
                        Icon {
                            text: languageCard.selected ? Theme.iconCheck
                                  : languageCard.suggested ? Theme.iconLanguage : ""
                            color: languageCard.selected ? Theme.accent : Theme.inkSubtle
                            preferredSize: 16
                            visible: text.length > 0
                        }
                    }

                    HoverHandler { id: cardHover }
                    TapHandler {
                        onTapped: {
                            languageCard.forceActiveFocus()
                            root.choose(languageCard.index)
                        }
                    }
                    Keys.onSpacePressed: event => {
                        root.choose(languageCard.index)
                        event.accepted = true
                    }
                    Keys.onReturnPressed: event => {
                        root.choose(languageCard.index)
                        event.accepted = true
                    }
                    Keys.onEnterPressed: event => {
                        root.choose(languageCard.index)
                        event.accepted = true
                    }
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.filterText.length > 0
                     && !root.model.some(entry => root.matches(entry))
            text: qsTrId("genycaster.setup.language_no_results")
            color: Theme.inkMuted
            horizontalAlignment: Theme.textStartAlignment
            font.pixelSize: Theme.typeLabelSize
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space3
            spacing: Theme.space2

            // Fixed-width single-line rows caused preview titles and long
            // Korean/Persian status text to paint over each other on phones.
            GridLayout {
                Layout.fillWidth: true
                columns: root.width < 620 ? 1 : 2
                columnSpacing: Theme.space2
                rowSpacing: Theme.space1
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTrId("genycaster.setup.live_preview")
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Theme.textStartAlignment
                    wrapMode: Text.WordWrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.changedHint
                    visible: root.userInteracted && root.changedHint.length > 0
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Theme.textStartAlignment
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: 236
                fillColor: Theme.surface
                borderColor: Theme.borderMuted
                borderWidth: 1
                curveDepth: Theme.curveContainer
                clip: true

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    layoutDirection: Theme.uiDirection

                    Rectangle {
                        Layout.preferredWidth: root.narrow ? 91 : 142
                        Layout.fillHeight: true
                        color: Theme.surfaceMuted

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space3

                            DirectionAwareLabel {
                                text: "Genycaster"
                                color: Theme.ink
                                font.weight: Theme.weightBold
                                font.pixelSize: root.narrow ? 11 : 14
                            }
                            Repeater {
                                model: [
                                    qsTrId("genycaster.feed_header.home"),
                                    qsTrId("genycaster.feed_header.explore"),
                                    qsTrId("genycaster.notifications.activity")
                                ]
                                delegate: Rectangle {
                                    id: previewNavItem
                                    required property string modelData
                                    required property int index
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 8
                                    color: index === previewTab.selectedTab
                                        ? Theme.surfaceSelected : "transparent"
                                    DirectionAwareLabel {
                                        anchors.fill: parent
                                        anchors.margins: 7
                                        verticalAlignment: Text.AlignVCenter
                                        text: previewNavItem.modelData
                                        color: previewNavItem.index === previewTab.selectedTab
                                            ? Theme.accent : Theme.inkMuted
                                        font.pixelSize: root.narrow ? 10 : 12
                                        font.family: Theme.fontFamilyForText(text)
                                        elide: Text.ElideRight
                                    }
                                    TapHandler {
                                        onTapped: previewTab.selectedTab = parent.index
                                    }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        color: Theme.borderMuted
                    }

                    ColumnLayout {
                        id: previewTab
                        property int selectedTab: 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: Theme.space3
                        spacing: Theme.space2

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: previewTab.selectedTab === 0
                                ? qsTrId("genycaster.feed_header.home")
                                : previewTab.selectedTab === 1
                                ? qsTrId("genycaster.feed_header.explore")
                                : qsTrId("genycaster.notifications.activity")
                            color: Theme.ink
                            font.weight: Theme.weightDemiBold
                            font.pixelSize: 13
                            horizontalAlignment: Theme.textStartAlignment
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.borderMuted
                        }
                        Repeater {
                            model: 2
                            delegate: ColumnLayout {
                                required property int index
                                Layout.fillWidth: true
                                spacing: 4
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: index === 0 ? "@genyleap" : "@compez.eth"
                                    color: Theme.accent
                                    font.pixelSize: 11
                                    horizontalAlignment: Theme.textStartAlignment
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: index === 0
                                        ? qsTrId("genycaster.setup.preview_post_one")
                                        : qsTrId("genycaster.setup.preview_post_two")
                                    font.family: Theme.fontFamilyForText(text)
                                    font.pixelSize: root.narrow ? 10 : 12
                                    color: Theme.inkMuted
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Theme.textStartAlignment
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: Theme.borderMuted
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
