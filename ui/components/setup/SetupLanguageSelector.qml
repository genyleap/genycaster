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
    property bool storyboard: false
    property string suggestedTag: ""
    signal activated(int index, var entry)

    readonly property int itemCount: model && model.length !== undefined ? model.length : 0
    readonly property var currentEntry: currentIndex >= 0 && currentIndex < itemCount
                                        ? model[currentIndex] : null
    readonly property bool narrow: width < 460
    readonly property int rowHeight: Math.max(Theme.touchTarget,
        Theme.scaleUi(narrow ? 56 : 60), Theme.typeTitle3Size * 2 + Theme.space2)
    readonly property real wheelHeight: rowHeight * 5 + Theme.space2 * 4

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
            Layout.topMargin: root.narrow ? Theme.space4 : Theme.space5
            text: root.storyboard ? qsTrId("genycaster.setup.design_language_title") : qsTrId("genycaster.locale.language")
            color: Theme.ink
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: root.narrow ? Theme.typeTitle1Size : Theme.typeTitle1Size + Theme.scaleUi(4)
            font.weight: Theme.weightBold
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.maximumWidth: 430
            Layout.alignment: Qt.AlignHCenter
            text: qsTrId("genycaster.setup.language_orbit_hint")
            visible: !root.storyboard
            color: Theme.inkMuted
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: Theme.typeLabelSize
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Item {
            Layout.fillWidth: true
            Layout.topMargin: root.storyboard ? Theme.space4 : root.narrow ? Theme.space5 : Theme.space7
            Layout.preferredHeight: root.wheelHeight
            Layout.bottomMargin: root.narrow ? Theme.space3 : Theme.space4

            // Every language owns its own selectable NeonSurface. A separate
            // fixed backdrop fell behind the ListView and never received its
            // hover/focus state.
            ListView {
                id: picker
                objectName: "setupLanguageWheel"
                anchors.centerIn: parent
                width: Math.min(parent.width, Theme.scaleUi(440))
                height: parent.height
                model: root.model
                clip: true
                // Real inter-row space: distinct language tiles must not
                // touch even while the wheel snaps or translates the list.
                spacing: Theme.space2
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
                    readonly property bool selected: index === picker.currentIndex
                    readonly property bool nativeRtl: ["fa", "ar", "he", "ur", "ps"].indexOf(
                        String(modelData.tag || "").toLowerCase().split(/[-_]/)[0]) >= 0

                    width: picker.width
                    height: root.rowHeight
                    activeFocusOnTab: false
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.language
                    Accessible.checked: selected

                    HoverHandler {
                        id: languagePointer
                        enabled: Theme.neonEnabled && !picker.moving
                        blocking: false
                    }
                    NeonSurface {
                        id: languageSurface
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space1
                        anchors.rightMargin: Theme.space1
                        anchors.topMargin: Theme.space1
                        anchors.bottomMargin: Theme.space1
                        interactive: !picker.moving
                        deferredNeon: !languageRow.selected
                        ambientNeon: languageRow.selected
                        focused: languageRow.selected && Theme.keyboardFocusVisible(picker)
                        externalHovered: languagePointer.hovered && !picker.moving
                        externalPointerX: languageSurface.mapFromItem(
                            languageRow, languagePointer.point.position.x,
                            languagePointer.point.position.y).x
                        externalPointerY: languageSurface.mapFromItem(
                            languageRow, languagePointer.point.position.x,
                            languagePointer.point.position.y).y
                        neonStrength: Theme.neonCardStrength
                        // Match the semantic selection of SetupActionCard,
                        // not a neutral input-field border.
                        neonColor: languageRow.selected ? Theme.accent : "transparent"
                        fillColor: languageRow.selected
                                   ? (languagePointer.hovered ? Theme.primarySurfaceHover : Theme.primarySurface)
                                   : languagePointer.hovered ? Theme.surfaceHover : "transparent"
                        borderWidth: languageRow.selected && Theme.keyboardFocusVisible(picker) ? 2
                                     : languageRow.selected || languagePointer.hovered ? Theme.strokeHairline : 0
                        borderColor: languageRow.selected && Theme.keyboardFocusVisible(picker) ? Theme.focus
                                     : languageRow.selected
                                       ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                                 languagePointer.hovered ? 0.83 : 0.52)
                                       : Theme.borderMuted
                        curveDepth: Theme.curveContainer
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space3
                        anchors.rightMargin: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: languageRow.nativeRtl ? Qt.RightToLeft : Qt.LeftToRight

                        // Balanced slots keep the language centered as the
                        // selected check moves from row to row.
                        Item {
                            visible: root.storyboard
                            Layout.preferredWidth: Theme.iconSizeSmall
                            Layout.preferredHeight: Theme.iconSizeSmall
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: languageRow.modelData.language
                            color: languageRow.selected ? Theme.ink : Theme.inkMuted
                            font.family: Theme.fontFamilyForText(text)
                            font.pixelSize: Theme.typeTitle3Size
                            font.weight: languageRow.selected ? Theme.weightDemiBold : Theme.weightRegular
                            horizontalAlignment: root.storyboard ? Text.AlignHCenter : languageRow.nativeRtl ? Text.AlignRight : Text.AlignLeft
                            elide: Text.ElideRight
                        }

                        Item {
                            visible: root.storyboard
                            Layout.preferredWidth: Theme.iconSizeSmall
                            Layout.preferredHeight: Theme.iconSizeSmall

                            Icon {
                                anchors.centerIn: parent
                                visible: languageRow.selected
                                text: Theme.iconCheck
                                color: Theme.accent
                                preferredSize: Theme.iconSizeSmall
                            }
                        }

                        DirectionAwareLabel {
                            visible: !root.storyboard
                            text: languageRow.modelData.hello
                            color: languageRow.selected ? Theme.inkMuted : Theme.inkSubtle
                            font.family: Theme.fontFamilyForText(text)
                            font.pixelSize: Theme.typeCaptionSize
                            horizontalAlignment: root.storyboard ? Text.AlignHCenter : languageRow.nativeRtl ? Text.AlignRight : Text.AlignLeft
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
            visible: !root.storyboard && text.length > 0
            color: Theme.inkSubtle
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
