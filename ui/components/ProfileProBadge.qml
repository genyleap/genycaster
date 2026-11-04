import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// One compact, server-verified status chip. A mouse hover, keyboard activation
// or touch tap opens the same themed explanation (never Qt's default tooltip).
Button {
    id: root
    objectName: "verifiedGenyProBadge"
    property var entitlement: ({})
    property bool compact: false
    readonly property bool eligible: entitlement.status === "verified"
                                     && entitlement.eligible === true
    readonly property int minimumGeny: Math.max(1,
        Number(entitlement.minimum_geny || 16000))
    signal detailsRequested()
    visible: eligible
    implicitHeight: Theme.scaleUi(32)
    implicitWidth: Math.max(Theme.scaleUi(68),
                            badgeContents.implicitWidth + Theme.space3 * 2)
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: !compact
    focusPolicy: Qt.StrongFocus
    LayoutMirroring.enabled: false
    Accessible.name: qsTrId("genycaster.pro.title")
    Accessible.description: qsTrId("genycaster.pro.verified_wallet")
        .arg(LocaleFormat.formatInteger(minimumGeny))
    Accessible.role: Accessible.Button
    onHoveredChanged: {
        hintDelay.stop()
        if (!hovered || compact) {
            proHint.close()
            return
        }
        hintDelay.restart()
    }
    onVisibleChanged: {
        if (!visible) {
            hintDelay.stop()
            proHint.close()
        }
    }
    // A click or mobile tap opens the full responsive Pro explanation.
    // The themed tooltip remains a desktop hover-only affordance.
    onClicked: {
        hintDelay.stop()
        proHint.close()
        root.detailsRequested()
    }
    Keys.onEscapePressed: proHint.close()

    function showHint() {
        const overlay = root.Overlay.overlay
        if (!overlay || !eligible) return
        const point = root.mapToItem(overlay, 0, 0)
        proHint.x = Math.max(Theme.space2,
            Math.min(overlay.width - proHint.width - Theme.space2,
                point.x + (root.width - proHint.width) / 2))
        const above = point.y - proHint.implicitHeight - Theme.space2
        proHint.y = above >= Theme.space2 ? above
            : Math.min(overlay.height - proHint.implicitHeight - Theme.space2,
                       point.y + root.height + Theme.space2)
        proHint.open()
    }

    Timer {
        id: hintDelay
        interval: 380
        repeat: false
        onTriggered: if (root.hovered && !root.compact) root.showHint()
    }

    background: Superellipse {
        fillColor: root.down || root.hovered
            ? Theme.proSurfaceHover : Theme.proSurface
        borderWidth: Theme.keyboardFocusVisible(root) ? 2 : 1
        borderColor: Theme.keyboardFocusVisible(root)
            ? Theme.focus : Theme.proBorder
        curveDepth: height / 2
        elevation: Theme.elevationNone
        Behavior on fillColor {
            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
    }

    contentItem: RowLayout {
        id: badgeContents
        spacing: Theme.space1
        layoutDirection: Qt.LeftToRight
        Icon {
            Layout.alignment: Qt.AlignVCenter
            text: Theme.iconSpark
            preferredSize: Theme.scaleUi(15)
            color: Theme.proAccent
        }
        Label {
            Layout.alignment: Qt.AlignVCenter
            text: qsTrId("genycaster.pro.short")
            color: Theme.proAccent
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightBold
            font.letterSpacing: 0.6
        }
    }

    Popup {
        id: proHint
        objectName: "genyProThemedTooltip"
        parent: root.Overlay.overlay
        modal: false
        focus: false
        width: Math.max(Theme.scaleUi(220),
            Math.min(Theme.scaleUi(314),
                (parent ? parent.width : Theme.scaleUi(314)) - Theme.space4))
        padding: Theme.space3
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface {
            curveDepth: Theme.curveCompact
            elevation: Theme.elevationPopover
        }
        contentItem: ColumnLayout {
            spacing: Theme.space1
            RowLayout {
                spacing: Theme.space2
                Icon {
                    text: Theme.iconSpark
                    preferredSize: Theme.iconSizeSmall
                    color: Theme.proAccent
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.title")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightBold
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.preferredWidth: proHint.width - proHint.leftPadding - proHint.rightPadding
                text: qsTrId("genycaster.pro.verified_wallet")
                    .arg(LocaleFormat.formatInteger(root.minimumGeny))
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }
}
