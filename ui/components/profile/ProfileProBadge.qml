import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// One compact own-account membership chip. A server-verified Pro/Basic status
// is distinguished from an unknown session. Hover/tap opens the details drawer.
Button {
    id: root
    objectName: "verifiedGenyProBadge"
    property var entitlement: ({})
    property bool compact: false
    // Own-account status is shown even for Basic and unavailable sessions.
    // Only a verified, matching server entitlement may render the Pro mark.
    property bool showUpgrade: false
    readonly property bool eligible: entitlement.status === "verified"
        && entitlement.tier === "pro" && entitlement.eligible === true
    readonly property bool basicVerified: entitlement.status === "verified"
        && entitlement.tier === "basic" && entitlement.eligible === false
    readonly property string statusText: eligible ? qsTrId("genycaster.pro.short")
        : basicVerified ? qsTrId("genycaster.pro.get_pro")
        : qsTrId("genycaster.pro.check_pro_status")
    signal detailsRequested()
    visible: showUpgrade || eligible
    implicitHeight: Theme.scaleUi(compact ? 29 : 32)
    implicitWidth: Math.max(Theme.scaleUi(compact ? 64 : 68),
                            badgeContents.implicitWidth + Theme.space3 * 2)
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: !compact
    focusPolicy: Qt.StrongFocus
    LayoutMirroring.enabled: false
    Accessible.name: root.statusText
    Accessible.description: eligible ? qsTrId("genycaster.pro.verified_wallet")
        : basicVerified ? qsTrId("genycaster.pro.basic_status_hint")
        : qsTrId("genycaster.pro.status_unknown_hint")
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
        if (!overlay) return
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

    background: NeonSurface {
        interactive: root.enabled && !root.compact
        ambientNeon: root.eligible
        neonStrength: Theme.neonControlStrength
        neonColor: root.eligible ? Qt.rgba(Theme.proAccent.r, Theme.proAccent.g,
                                           Theme.proAccent.b, 0.35) : "transparent"
        fillColor: root.down || root.hovered
            ? (root.eligible || root.basicVerified ? Theme.proSurfaceHover : Theme.surfaceHover)
            : root.eligible ? Theme.proSurface : Theme.surfaceRaised
        borderWidth: Theme.keyboardFocusVisible(root) ? 2 : 1
        borderColor: Theme.keyboardFocusVisible(root)
            ? Theme.focus : (root.eligible || root.basicVerified ? Theme.proBorder : Theme.borderSubtle)
        curveDepth: height / 2
        elevation: Theme.elevationNone
    }

    contentItem: RowLayout {
        id: badgeContents
        spacing: Theme.space1
        layoutDirection: Qt.LeftToRight
        Icon {
            Layout.alignment: Qt.AlignVCenter
            text: root.eligible ? Theme.iconSpark : root.basicVerified
                ? Theme.iconSpark : Theme.iconRefresh
            preferredSize: Theme.scaleUi(compact ? 13 : 15)
            color: root.eligible || root.basicVerified ? Theme.proAccent : Theme.textSecondary
        }
        Label {
            Layout.alignment: Qt.AlignVCenter
            text: root.statusText
            color: root.eligible || root.basicVerified ? Theme.proAccent : Theme.textSecondary
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
                text: root.eligible ? qsTrId("genycaster.pro.verified_wallet")
                    : root.basicVerified ? qsTrId("genycaster.pro.basic_status_hint")
                    : qsTrId("genycaster.pro.status_unknown_hint")
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }
}
