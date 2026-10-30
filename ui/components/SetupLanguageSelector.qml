pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// A bounded, native-feeling wheel. Selection is committed at the end of a
// gesture instead of changing translators for every intermediate wheel row.
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
    readonly property int rowHeight: narrow ? 56 : 60

    function choose(index) {
        if (index < 0 || index >= itemCount)
            return
        if (picker.currentIndex !== index)
            picker.currentIndex = index
        if (currentIndex === index && userInteracted)
            return
        currentIndex = index
        userInteracted = true
        activated(index, model[index])
    }

    onCurrentIndexChanged: {
        if (picker.currentIndex !== currentIndex && !picker.moving)
            picker.currentIndex = currentIndex
    }

    Component.onCompleted: {
        suggestedTag = String(Qt.locale().name).toLowerCase().split(/[_-]/)[0]
        picker.currentIndex = currentIndex
    }

    implicitHeight: content.implicitHeight
    LayoutMirroring.enabled: false
    Accessible.name: qsTrId("genycaster.locale.language")
    Accessible.description: selectedLabel

    ColumnLayout {
        id: content
        width: parent.width
        spacing: root.narrow ? Theme.space3 : Theme.space4

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.topMargin: root.narrow ? Theme.space6 : Theme.space5
            text: qsTrId("genycaster.locale.language")
            color: Theme.ink
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: root.narrow ? 32 : 38
            font.weight: Theme.weightBold
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.maximumWidth: 430
            Layout.alignment: Qt.AlignHCenter
            text: qsTrId("genycaster.setup.language_orbit_hint")
            color: Theme.inkMuted
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: Theme.typeLabelSize
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Item {
            Layout.fillWidth: true
            Layout.topMargin: root.narrow ? Theme.space6 : Theme.space7
            Layout.preferredHeight: root.narrow ? 328 : 356
            Layout.bottomMargin: root.narrow ? Theme.space3 : Theme.space4

            Superellipse {
                anchors.centerIn: parent
                width: Math.min(parent.width, root.narrow ? 340 : 400)
                height: root.rowHeight - 4
                fillColor: Theme.surfaceMuted
                borderColor: Theme.borderMuted
                borderWidth: Theme.strokeStandard
                curveDepth: Theme.curveControl
            }

            ListView {
                id: picker
                objectName: "setupLanguageWheel"
                anchors.centerIn: parent
                width: Math.min(parent.width, root.narrow ? 340 : 400)
                height: parent.height
                model: root.model
                clip: true
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                snapMode: ListView.SnapToItem
                highlightRangeMode: ListView.StrictlyEnforceRange
                preferredHighlightBegin: (height - root.rowHeight) / 2
                preferredHighlightEnd: (height + root.rowHeight) / 2
                keyNavigationWraps: false
                activeFocusOnTab: true
                Accessible.name: qsTrId("genycaster.locale.language")
                onMovementEnded: root.choose(currentIndex)

                header: Item {
                    width: picker.width
                    height: Math.max(0, (picker.height - root.rowHeight) / 2)
                }
                footer: Item {
                    width: picker.width
                    height: Math.max(0, (picker.height - root.rowHeight) / 2)
                }

                delegate: Item {
                    id: languageRow
                    required property int index
                    required property var modelData
                    readonly property int distance: Math.abs(index - picker.currentIndex)
                    readonly property bool selected: index === picker.currentIndex
                    readonly property bool nativeRtl: ["fa", "ar", "he", "ur", "ps"].indexOf(
                        String(modelData.tag || "").toLowerCase().split(/[-_]/)[0]) >= 0

                    width: picker.width
                    height: root.rowHeight
                    opacity: Math.max(0.14, 1.0 - distance * 0.29)
                    activeFocusOnTab: false
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.language
                    Accessible.checked: selected

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space3
                        anchors.rightMargin: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: languageRow.nativeRtl ? Qt.RightToLeft : Qt.LeftToRight

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: languageRow.modelData.language
                            color: Theme.ink
                            font.family: Theme.fontFamilyForText(text)
                            font.pixelSize: languageRow.selected ? 22 : 19
                            font.weight: languageRow.selected ? Theme.weightDemiBold : Theme.weightRegular
                            horizontalAlignment: languageRow.nativeRtl ? Text.AlignRight : Text.AlignLeft
                            elide: Text.ElideRight
                        }

                        DirectionAwareLabel {
                            text: languageRow.modelData.hello
                            color: languageRow.selected ? Theme.inkMuted : Theme.inkSubtle
                            font.family: Theme.fontFamilyForText(text)
                            font.pixelSize: Theme.typeCaptionSize
                            horizontalAlignment: languageRow.nativeRtl ? Text.AlignRight : Text.AlignLeft
                        }
                    }

                    TapHandler {
                        onTapped: {
                            picker.forceActiveFocus()
                            root.choose(languageRow.index)
                        }
                    }
                }

                Keys.onUpPressed: event => {
                    root.choose(Math.max(0, picker.currentIndex - 1))
                    event.accepted = true
                }
                Keys.onDownPressed: event => {
                    root.choose(Math.min(root.itemCount - 1, picker.currentIndex + 1))
                    event.accepted = true
                }
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Home) {
                        root.choose(0)
                        event.accepted = true
                    } else if (event.key === Qt.Key_End) {
                        root.choose(root.itemCount - 1)
                        event.accepted = true
                    }
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.currentEntry && root.currentEntry.tag === root.suggestedTag
                  ? qsTrId("genycaster.setup.system_suggestion") : ""
            visible: text.length > 0
            color: Theme.inkSubtle
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
