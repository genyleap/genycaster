import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconGlyph: ""
    property url brandImageSource: ""
    property string trailingText: ""
    // Optional measured response-time hint used by read-node selection.
    property int latencyMs: -1
    property bool primary: false
    property bool complete: false
    // Selection is NOT task completion. This is a single-choice radio indicator.
    property bool selected: false
    property bool busy: false
    property bool showChevron: true
    property bool showSelectionMark: true
    property bool compactCard: false
    property bool onboardingStyle: false
    property bool neutralPrimary: false
    property bool accountShowcase: false
    property bool networkShowcase: false
    property bool setupEmphasis: false
    // Disabled feature choice: muted card with a localized availability
    // badge. Keeps the status visible without implying the card is clickable.
    property bool unavailable: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    HoverHandler {
        id: cardPointer
        enabled: Theme.neonEnabled
        blocking: false
    }
    implicitHeight: Math.max(accountShowcase ? 182 : networkShowcase ? 120
                             : compactCard ? 62 : onboardingStyle ? 78 : 96,
                             contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: compactCard || (onboardingStyle && !accountShowcase && !networkShowcase)
                ? Theme.space3 : Theme.space4
    bottomPadding: compactCard || (onboardingStyle && !accountShowcase && !networkShowcase)
                   ? Theme.space3 : Theme.space4

    Accessible.name: title
    Accessible.description: subtitle + (root.unavailable
                            ? " · " + qsTrId("genycaster.setup.temporarily_unavailable") : "")
    Accessible.checked: root.selected

    // Disabled primary actions use a neutral card background; do not paint
    // white-on-white labels just because the enabled state was prominent.
    // An in-flight action remains visually active while clicks are disabled.
    // Unrelated sign-in options stay neutral, never inheriting its spinner.
    readonly property bool visuallyDisabled: !enabled && !busy
    readonly property color foreground: root.unavailable ? Theme.inkMuted
                                      : setupEmphasis ? "#FCFCFE"
                                      : visuallyDisabled ? Theme.inkMuted
                                      : primary ? (neutralPrimary ? Theme.surface : Theme.accentInk) : Theme.ink
    readonly property color secondaryForeground: root.unavailable ? Theme.inkSubtle
                                               : setupEmphasis ? "#BABBC6"
                                               : visuallyDisabled ? Theme.inkMuted
                                               : primary ? Qt.rgba(foreground.r, foreground.g, foreground.b, 0.84)
                                               : Theme.inkMuted

    background: NeonCard {
        // Every SetupActionCard is a real Button. Earlier only the account
        // and protocol showcase variants got hover reflection, leaving sign-in,
        // legal, provider and recovery choices completely inert.
        interactive: root.enabled && !root.unavailable
        ambientNeon: root.enabled
                     && (root.accountShowcase || root.networkShowcase || root.primary
                         || root.setupEmphasis || root.selected)
        deferredNeon: !root.accountShowcase && !root.networkShowcase
                      && !root.primary && !root.setupEmphasis && !root.selected
        neonBusy: root.busy
        inverted: root.primary && !root.neutralPrimary
        focused: Theme.keyboardFocusVisible(root)
        externalHovered: root.hovered
        externalPointerX: cardPointer.hovered ? cardPointer.point.position.x : width / 2
        externalPointerY: cardPointer.hovered ? cardPointer.point.position.y : height / 2
        neonStrength: Theme.neonCardStrength
        neonColor: root.selected
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.40)
            : "transparent"
        fillColor: root.unavailable ? (Theme.dark ? "#23272E" : "#F5F6F8")
                   : root.setupEmphasis ? (Theme.dark ? "#24252C" : "#22232A")
                   : root.visuallyDisabled
                   ? Theme.surfaceMuted
                   : root.primary
                     ? (root.neutralPrimary ? Theme.ink
                        : root.down ? Theme.primaryPressed
                                    : root.hovered ? Theme.primaryFillHover : Theme.primaryFill)
                     : root.down ? Theme.surfacePressed
                                 : root.hovered ? Theme.surfaceHover : Theme.surface
        borderWidth: Theme.keyboardFocusVisible(root) ? 2
                     : root.setupEmphasis ? 0
                     : root.selected ? 2
                     : root.primary && !root.visuallyDisabled ? 0 : Theme.strokeStandard
        borderColor: Theme.keyboardFocusVisible(root) ? Theme.focus
                     : root.unavailable ? (Theme.dark ? "#3C444E" : "#E4E7EB")
                     : root.selected ? Theme.accent : Theme.borderMuted
        curveDepth: Theme.curveContainer
        elevation: root.primary && !root.visuallyDisabled ? Theme.elevationControl : Theme.elevationNone


    }

    contentItem: Item {
        implicitHeight: root.accountShowcase ? 150
                        : root.networkShowcase ? 88 : standardCardRow.implicitHeight
        RowLayout {
            id: standardCardRow
            anchors.fill: parent
            visible: !root.accountShowcase && !root.networkShowcase
            spacing: Theme.space3
            layoutDirection: Theme.uiDirection

        GenySpinner {
            visible: root.busy
            running: visible
            color: root.foreground
            Layout.preferredWidth: Theme.iconSize
            Layout.preferredHeight: Theme.iconSize
        }

        Icon {
            visible: !root.busy && root.iconGlyph.length > 0 && root.brandImageSource.toString().length === 0
            text: root.iconGlyph
            color: !root.visuallyDisabled && !root.primary ? Theme.inkMuted : root.foreground
            preferredSize: Theme.iconSize
        }

        Image {
            visible: !root.busy && root.brandImageSource.toString().length > 0
            source: root.brandImageSource
            Layout.preferredWidth: Theme.iconSize + 4
            Layout.preferredHeight: Theme.iconSize + 4
            sourceSize: Qt.size(96, 96)
            fillMode: Image.PreserveAspectFit
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.onboardingStyle ? Theme.space1 : 2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.title
                color: root.foreground
                font.pixelSize: root.compactCard || root.onboardingStyle ? Theme.typeLabelSize : Math.max(Theme.typeLabelSize, 17)
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                visible: root.subtitle.length > 0
                Layout.fillWidth: true
                text: root.subtitle
                color: root.secondaryForeground
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightMedium
                horizontalAlignment: Theme.textStartAlignment
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: root.compactCard ? 1 : root.onboardingStyle ? 3 : 2
            }
        }

        Label {
            visible: root.trailingText.length > 0
            text: root.trailingText
            color: root.primary ? root.secondaryForeground
                   : root.latencyMs < 0 ? Theme.inkSubtle
                   : root.latencyMs <= 300 ? Theme.success
                   : root.latencyMs <= 800 ? Theme.warning : Theme.danger
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightDemiBold
        }

        Icon {
            visible: root.selected && root.showSelectionMark
            text: Theme.iconCheck
            color: root.primary ? root.foreground : Theme.accent
            preferredSize: Theme.iconSize
        }

        Icon {
            visible: root.complete && !root.selected
            text: Theme.iconCheck
            color: root.foreground
            preferredSize: Theme.iconSizeSmall
        }

        Icon {
            visible: root.showChevron && !root.complete
                     && (!root.selected || !root.showSelectionMark) && !root.busy
            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
            color: root.visuallyDisabled ? Theme.inkMuted
                   : root.primary ? root.secondaryForeground : Theme.inkSubtle
            preferredSize: Theme.iconSizeSmall
        }
        }
        // Large account choices: an unmistakable leading icon and a concise
        // title/description group; no disconnected miniature glyphs.
        ColumnLayout {
            id: editorialAccountCard
            anchors.fill: parent
            visible: root.accountShowcase
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Superellipse {
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 58
                    fillColor: root.hovered ? Theme.surfaceHover : Theme.surfaceMuted
                    borderWidth: 0
                    curveDepth: Theme.curveContainer
                    Icon {
                        anchors.centerIn: parent
                        text: root.iconGlyph
                        color: root.foreground
                        preferredSize: 36
                    }
                }
                Item { Layout.fillWidth: true }
                DirectionAwareLabel {
                    visible: root.unavailable
                    text: qsTrId("genycaster.setup.temporarily_unavailable")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                Icon {
                    visible: root.showChevron && !root.unavailable
                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    preferredSize: Theme.iconSizeSmall
                    color: root.hovered ? Theme.ink : Theme.inkMuted
                }
            }
            Item { Layout.preferredHeight: Theme.space1 }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.title
                color: root.foreground
                font.pixelSize: Math.max(Theme.typeLabelSize, 18)
                font.weight: Theme.weightBold
                horizontalAlignment: Theme.textStartAlignment
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.subtitle
                color: root.secondaryForeground
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Theme.textStartAlignment
                maximumLineCount: 3
            }
            Item { Layout.fillHeight: true }
        }

        // Protocol identity must be recognizable before the descriptive text.
        // Official Genyleap icon assets are resolved through Theme.iconSource.
        RowLayout {
            id: networkCardRow
            anchors.fill: parent
            visible: root.networkShowcase
            spacing: Theme.space3
            layoutDirection: Theme.uiDirection
            Superellipse {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                fillColor: root.selected ? Theme.surfaceHover : Theme.surfaceMuted
                borderWidth: root.selected ? Theme.strokeStandard : 0
                borderColor: root.selected ? Theme.accent : Theme.borderMuted
                curveDepth: Theme.curveContainer
                Icon {
                    anchors.centerIn: parent
                    text: root.iconGlyph
                    preferredSize: 40
                    color: root.foreground
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.title
                    color: root.foreground
                    font.pixelSize: Math.max(Theme.typeLabelSize, 17)
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Theme.textStartAlignment
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.subtitle
                    color: root.secondaryForeground
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.textStartAlignment
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }
            }
            Icon {
                visible: root.selected && root.showSelectionMark
                text: Theme.iconCheck
                color: Theme.accent
                preferredSize: Theme.iconSize
            }
            Icon {
                visible: !root.selected && root.showChevron
                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                color: Theme.inkSubtle
                preferredSize: Theme.iconSizeSmall
            }
        }
    }
}
