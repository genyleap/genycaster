import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Human-first provider management. These are typed provider instances,
// not a generic URL toggle; selection is applied on browser restart.
ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.space4

    property bool addingReader: false
    property bool addingWriter: false
    property bool showOperatorDirectory: true
    property bool showServerGuide: false
    property bool selfHosted: false
    property string readContract: "snapchain-v1"
    property string pasteTarget: ""
    readonly property string resolvedSelectedReadId:
        providerProfiles.selectedReadProviderId.length === 0
        ? "hypersnap-v2" : providerProfiles.selectedReadProviderId

    function selectedReadLabel() {
        if (root.resolvedSelectedReadId === "hypersnap-v2") return "HyperSnap v2"
        const profiles = providerProfiles.readProfiles
        for (let i = 0; i < profiles.length; ++i) {
            if (String(profiles[i].id) === root.resolvedSelectedReadId)
                return String(profiles[i].label || profiles[i].id)
        }
        return root.resolvedSelectedReadId
    }

    readonly property var liveHealthRows: providerProfiles.connectionRows
    readonly property int checkedNodes: liveHealthRows.filter(
        item => String(item.status) !== "idle" && String(item.status) !== "checking").length
    readonly property int healthyNodes: liveHealthRows.filter(
        item => String(item.status) === "healthy").length

    function responseBars(ms) {
        if (ms < 0) return 0
        if (ms <= 250) return 4
        if (ms <= 650) return 3
        if (ms <= 1500) return 2
        return 1
    }

    function healthLabel(state) {
        switch (state) {
        case "checking": return qsTrId("genycaster.nodes.health_checking")
        case "healthy": return qsTrId("genycaster.nodes.health_ok")
        case "auth_required": return qsTrId("genycaster.nodes.health_auth")
        case "contract_mismatch": return qsTrId("genycaster.nodes.health_mismatch")
        case "incompatible": return qsTrId("genycaster.nodes.health_incompatible")
        case "invalid": return qsTrId("genycaster.nodes.health_invalid")
        case "failed": return qsTrId("genycaster.nodes.health_failed")
        default: return qsTrId("genycaster.nodes.health_untested")
        }
    }

    function probeLabel() {
        switch (providerProfiles.probeState) {
        case "checking": return qsTrId("genycaster.nodes.checking")
        case "healthy": return qsTrId("genycaster.nodes.connected")
        case "auth_required": return qsTrId("genycaster.nodes.auth_required")
        case "incompatible": return qsTrId("genycaster.nodes.incompatible")
        case "contract_mismatch": return qsTrId("genycaster.nodes.contract_mismatch").arg(
            providerProfiles.probeSuggestedContract === "snapchain-v1"
            ? qsTrId("genycaster.nodes.raw") : qsTrId("genycaster.nodes.indexed"))
        case "invalid": return qsTrId("genycaster.providers.error_invalid_endpoint")
        case "failed": return qsTrId("genycaster.nodes.unreachable")
                              + (providerProfiles.probeHttpStatus > 0
                                 ? " · HTTP " + providerProfiles.probeHttpStatus : "")
        default: return qsTrId("genycaster.nodes.not_tested")
        }
    }

    Connections {
        target: providerProfiles
        function onClipboardChanged() {
            const copied = providerProfiles.clipboardText
            if (root.pasteTarget === "reader") readEndpoint.text = copied.trim()
            else if (root.pasteTarget === "key") readKey.text = copied.trim()
            else if (root.pasteTarget === "writer") writeEndpoint.text = copied.trim()
            root.pasteTarget = ""
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.title")
        font.pixelSize: Theme.typeTitle2Size
        font.weight: Theme.weightBold
        color: Theme.ink
    }
    DirectionAwareLabel {
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.description")
        font.pixelSize: Theme.typeMetaSize
        color: Theme.inkMuted
        wrapMode: Text.Wrap
    }

    Superellipse {
        Layout.fillWidth: true
        implicitHeight: activeDetails.implicitHeight + Theme.space4 * 2
        fillColor: Theme.surfaceRaised
        curveDepth: Theme.curveContainer
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        ColumnLayout {
            id: activeDetails
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space4
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.active")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Icon {
                    text: Theme.iconCheck
                    color: Theme.positive
                    preferredSize: Theme.iconSizeSmall
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: providerProfiles.activeReadProviderLabel
                    alignToUi: true
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    text: qsTrId("genycaster.nodes.selected_now")
                    color: Theme.positive
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: root.resolvedSelectedReadId !== providerProfiles.activeReadProviderId
                text: qsTrId("genycaster.nodes.pending_read").arg(root.selectedReadLabel())
                font.pixelSize: Theme.typeMetaSize
                color: Theme.warning
                wrapMode: Text.Wrap
            }
            DirectionAwareLabel {
                visible: providerProfiles.activeReadContract === "snapchain-v1"
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.raw_small")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: providerProfiles.activeReadEndpoint
                color: Theme.inkMuted
                forceLeftToRight: true
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideMiddle
            }

        }
    }

    // An on-demand authenticated API compatibility probe, not ICMP ping.
    // Results are local to this app run and never claim continuous uptime.
    Superellipse {
        Layout.fillWidth: true
        implicitHeight: healthPanel.implicitHeight + Theme.space3 * 2
        fillColor: Theme.surfaceRaised
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer
        ColumnLayout {
            id: healthPanel
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.nodes.health_title")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                }
                ActionButton {
                    compact: true
                    iconGlyph: Theme.iconRefresh
                    busy: providerProfiles.probingAll
                    enabled: !providerProfiles.probingAll
                    text: qsTrId("genycaster.nodes.health_test_all")
                    onClicked: providerProfiles.probeAllReadProviders()
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.health_short")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                DirectionAwareLabel {
                    text: qsTrId("genycaster.nodes.health_progress")
                          .arg(root.checkedNodes).arg(root.liveHealthRows.length)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    visible: root.checkedNodes > 0
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTrId("genycaster.nodes.health_passed").arg(root.healthyNodes)
                    color: Theme.positive
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: Theme.borderMuted
                Rectangle {
                    width: parent.width * (root.liveHealthRows.length > 0
                            ? root.checkedNodes / root.liveHealthRows.length : 0)
                    height: parent.height
                    radius: parent.radius
                    color: Theme.positive
                    Behavior on width {
                        NumberAnimation {
                            duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
            Repeater {
                model: providerProfiles.connectionRows
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2
                        Icon {
                            text: String(modelData.id) === providerProfiles.activeReadProviderId
                                  ? Theme.iconCheck : Theme.iconCloud
                            color: String(modelData.id) === providerProfiles.activeReadProviderId
                                   ? Theme.positive : Theme.inkMuted
                            preferredSize: Theme.iconSizeSmall
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: String(modelData.label)
                            alignToUi: true
                            color: Theme.ink
                            font.weight: Theme.weightMedium
                            elide: Text.ElideMiddle
                        }
                        RowLayout {
                            spacing: 3
                            layoutDirection: Qt.LeftToRight
                            visible: String(modelData.status) === "healthy"
                            Repeater {
                                model: 4
                                delegate: Rectangle {
                                    required property int index
                                    width: 5
                                    height: 5 + (index + 1) * 3
                                    radius: 2
                                    Layout.alignment: Qt.AlignBottom
                                    color: index < root.responseBars(Number(modelData.latencyMs))
                                           ? Theme.positive : Theme.borderMuted
                                    Behavior on color {
                                        ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard }
                                    }
                                }
                            }
                        }
                        DirectionAwareLabel {
                            text: root.healthLabel(String(modelData.status))
                                  + (String(modelData.status) === "healthy"
                                     ? " · " + Number(modelData.latencyMs) + " ms"
                                     : (Number(modelData.httpStatus || 0) > 0
                                        ? " · HTTP " + Number(modelData.httpStatus) : ""))
                            color: String(modelData.status) === "healthy"
                                   ? Theme.positive
                                   : String(modelData.status) === "idle" ? Theme.inkMuted : Theme.warning
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        ActionButton {
                            compact: true
                            quiet: true
                            enabled: !providerProfiles.probingAll
                            text: qsTrId("genycaster.nodes.test")
                            onClicked: providerProfiles.probeReadProvider(String(modelData.id))
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.endpoint || "")
                        alignToUi: true
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.divider
                    }
                }
            }
        }
    }

    // Informational directory, separate from configured connections.
    // Live status and API compatibility are NOT inferred from this list.
    Superellipse {
        objectName: "publicProviderDirectory"
        Layout.fillWidth: true
        implicitHeight: directoryBody.implicitHeight + Theme.space3 * 2
        fillColor: Theme.surfaceRaised
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderSubtle
        curveDepth: Theme.curveContainer
        ColumnLayout {
            id: directoryBody
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.nodes.directory_title")
                    color: Theme.ink
                    font.weight: Theme.weightDemiBold
                    font.pixelSize: Theme.typeTitle3Size
                }
                ActionButton {
                    quiet: true
                    compact: true
                    iconGlyph: root.showOperatorDirectory ? Theme.iconChevronDown : Theme.iconArrowUpRight
                    text: root.showOperatorDirectory
                          ? qsTrId("genycaster.nodes.directory_collapse")
                          : qsTrId("genycaster.nodes.directory_expand")
                    onClicked: root.showOperatorDirectory = !root.showOperatorDirectory
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.directory_info")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
            ColumnLayout {
                visible: root.showOperatorDirectory
                Layout.fillWidth: true
                spacing: Theme.space1
                Repeater {
                    model: [
                        { name: "Hypersnap Public", provider: "Hypersnap", host: "haatz.quilibrium.com" },
                        { name: "Ardea / Arca", provider: "Arca", host: "ardea.arcabot.ai" },
                        { name: "Neynar Hub API", provider: "Neynar", host: "neynar.com" },
                        { name: "Pinata Hub", provider: "Pinata", host: "hub.pinata.cloud" },
                        { name: "Standard Crypto Hub", provider: "Standard Crypto", host: "hub.farcaster.standardcrypto.vc" }
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2
                        Icon {
                            text: Theme.iconCloud
                            color: Theme.inkMuted
                            preferredSize: Theme.iconSizeSmall
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.name)
                                alignToUi: true
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.host)
                                alignToUi: true
                                font.pixelSize: Theme.typeCaptionSize
                                color: Theme.inkSubtle
                                elide: Text.ElideMiddle
                            }
                        }
                        DirectionAwareLabel {
                            text: qsTrId("genycaster.nodes.directory_unverified")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.inkMuted
                        }
                    }
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    alignStart: true
                    iconGlyph: Theme.iconArrowUpRight
                    text: qsTrId("genycaster.nodes.directory_source")
                    onClicked: Qt.openUrlExternally("https://hypersnap.org/network")
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.read_title")
            color: Theme.ink
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightDemiBold
        }
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Icon {
                visible: providerProfiles.activeReadProviderId === "hypersnap-v2"
                text: Theme.iconCheck
                color: Theme.positive
                preferredSize: Theme.iconSizeSmall
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.default_choice") + " · HyperSnap v2"
                alignStart: true
                selected: providerProfiles.selectedReadProviderId.length === 0
                onClicked: providerProfiles.selectReadProvider("")
            }
        }
        Repeater {
            model: providerProfiles.readProfiles
            delegate: Superellipse {
                id: readProfileCard
                required property var modelData
                property bool showCredentials: false
                Layout.fillWidth: true
                implicitHeight: profileBody.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: providerProfiles.selectedReadProviderId === String(modelData.id)
                             ? Theme.accent : Theme.borderMuted
                curveDepth: Theme.curveContainer
                ColumnLayout {
                    id: profileBody
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.label || modelData.id)
                        color: Theme.ink
                        font.weight: Theme.weightDemiBold
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2
                        Icon {
                            visible: String(modelData.id) === providerProfiles.activeReadProviderId
                            text: Theme.iconCheck
                            color: Theme.positive
                            preferredSize: Theme.iconSizeSmall
                        }
                        DirectionAwareLabel {
                            visible: String(modelData.id) === providerProfiles.activeReadProviderId
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.nodes.selected_now")
                            color: Theme.positive
                            font.weight: Theme.weightDemiBold
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        DirectionAwareLabel {
                            visible: providerProfiles.selectedReadProviderId === String(modelData.id)
                                     && String(modelData.id) !== providerProfiles.activeReadProviderId
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.nodes.pending_tag")
                            color: Theme.warning
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.endpoint || "")
                        color: Theme.inkMuted
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.contract) === "snapchain-v1"
                              ? qsTrId("genycaster.nodes.raw_brief")
                              : qsTrId("genycaster.nodes.indexed_brief")
                        color: String(modelData.contract) === "snapchain-v1"
                               ? Theme.warning : Theme.inkSubtle
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Layout.preferredHeight: childrenRect.height
                        ActionButton {
                            compact: true
                            text: providerProfiles.selectedReadProviderId === String(modelData.id)
                                  ? (providerProfiles.activeReadProviderId === String(modelData.id)
                                     ? qsTrId("genycaster.nodes.active_tag")
                                     : qsTrId("genycaster.nodes.pending_tag"))
                                  : qsTrId("genycaster.providers.use")
                            selected: providerProfiles.selectedReadProviderId === String(modelData.id)
                            onClicked: providerProfiles.selectReadProvider(String(modelData.id))
                        }
                        ActionButton {
                            compact: true
                            quiet: true
                            visible: Qt.platform.os === "wasm"
                            selected: readProfileCard.showCredentials
                            text: qsTrId("genycaster.nodes.api_key")
                            onClicked: readProfileCard.showCredentials = !readProfileCard.showCredentials
                        }
                        ActionButton {
                            compact: true
                            quiet: true
                            destructive: true
                            text: qsTrId("genycaster.providers.remove")
                            onClicked: providerProfiles.removeReadProfile(String(modelData.id))
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        visible: Qt.platform.os === "wasm" && readProfileCard.showCredentials
                        property string nodeId: String(modelData.id)
                        DirectionAwareTextField {
                            id: savedNodeKey
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: qsTrId("genycaster.nodes.api_optional")
                            Accessible.name: qsTrId("genycaster.nodes.api_key")
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            Layout.preferredHeight: childrenRect.height
                            ActionButton {
                                compact: true
                                text: qsTrId("genycaster.nodes.paste")
                                onClicked: {
                                    root.pasteTarget = "existing-key:" + String(modelData.id)
                                    providerProfiles.requestBrowserPaste()
                                }
                            }
                            ActionButton {
                                compact: true
                                text: qsTrId("genycaster.nodes.save_key")
                                enabled: savedNodeKey.text.length > 0
                                onClicked: {
                                    if (providerProfiles.setSessionReadApiKey(
                                            String(modelData.id), savedNodeKey.text))
                                        savedNodeKey.clear()
                                }
                            }
                        }
                        Connections {
                            target: providerProfiles
                            function onClipboardChanged() {
                                if (root.pasteTarget === "existing-key:" + String(modelData.id)) {
                                    savedNodeKey.text = providerProfiles.clipboardText.trim()
                                    root.pasteTarget = ""
                                }
                            }
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: providerProfiles.probeProviderId === String(modelData.id)
                        text: root.probeLabel()
                              + (providerProfiles.probeState === "healthy"
                                 ? " · " + providerProfiles.probeLatencyMs + " ms" : "")
                        font.pixelSize: Theme.typeCaptionSize
                        color: providerProfiles.probeState === "healthy"
                               ? Theme.positive : Theme.warning
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
        // Visible read-mode choice: no critical privacy/route control hidden
        // under a generic Advanced accordion.
        Superellipse {
            Layout.fillWidth: true
            implicitHeight: fallbackDetails.implicitHeight + Theme.space3 * 2
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderSubtle
            curveDepth: Theme.curveContainer
            ColumnLayout {
                id: fallbackDetails
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space2
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.nodes.extra_data")
                            color: Theme.ink
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.nodes.extra_data_desc")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.inkMuted
                            wrapMode: Text.Wrap
                        }
                    }
                    GenySwitch {
                        checked: providerProfiles.compatibleReadFallback
                        Accessible.name: qsTrId("genycaster.nodes.extra_data")
                        onToggled: providerProfiles.setCompatibleReadFallback(checked)
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: providerProfiles.activeReadContract === "snapchain-v1"
                    text: providerProfiles.appliedCompatibleReadFallback
                          ? qsTrId("genycaster.nodes.counts_short")
                          : qsTrId("genycaster.nodes.counts_raw")
                    color: providerProfiles.appliedCompatibleReadFallback ? Theme.accent : Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }
        ActionButton {
            Layout.fillWidth: true
            iconGlyph: Theme.iconPlus
            alignStart: true
            text: qsTrId("genycaster.nodes.add")
            selected: root.addingReader
            onClicked: root.addingReader = !root.addingReader
        }
    }

    ColumnLayout {
        visible: root.addingReader
        Layout.fillWidth: true
        spacing: Theme.space3
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.nodes.choose_type")
            color: Theme.ink
            font.weight: Theme.weightDemiBold
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            ActionButton {
                Layout.fillWidth: true
                selected: root.readContract === "snapchain-v1"
                text: qsTrId("genycaster.nodes.raw")
                enabled: providerProfiles.rawReadSupported
                onClicked: {
                    root.readContract = "snapchain-v1"
                    providerProfiles.clearDraftProbe()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                selected: root.readContract === "hypersnap-v2"
                text: qsTrId("genycaster.nodes.indexed")
                enabled: providerProfiles.indexedReadSupported
                onClicked: {
                    root.readContract = "hypersnap-v2"
                    providerProfiles.clearDraftProbe()
                }
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.readContract === "snapchain-v1"
                  ? qsTrId("genycaster.nodes.raw_description")
                  : qsTrId("genycaster.nodes.indexed_description")
            color: Theme.inkMuted
            wrapMode: Text.Wrap
            font.pixelSize: Theme.typeCaptionSize
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.readContract === "snapchain-v1"
                  ? "/v1/castsByFid · /v1/userDataByFid"
                  : "/v2/farcaster/user"
            forceLeftToRight: true
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.inkSubtle
            elide: Text.ElideRight
        }
        DirectionAwareLabel {
            text: qsTrId("genycaster.nodes.name")
            color: Theme.ink
        }
        DirectionAwareTextField {
            id: readName
            Layout.fillWidth: true
            placeholderText: qsTrId("genycaster.nodes.name_example")
            Accessible.name: qsTrId("genycaster.nodes.name")
        }
        DirectionAwareLabel {
            text: qsTrId("genycaster.nodes.endpoint")
            color: Theme.ink
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareTextField {
                id: readEndpoint
                Layout.fillWidth: true
                forceLeftToRight: true
                placeholderText: "https://node.example.org"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                Accessible.name: qsTrId("genycaster.nodes.endpoint")
                onTextEdited: providerProfiles.clearDraftProbe()
            }
            ActionButton {
                compact: true
                text: qsTrId("genycaster.nodes.paste")
                onClicked: {
                    root.pasteTarget = "reader"
                    providerProfiles.requestBrowserPaste()
                }
            }
        }
        ActionButton {
            compact: true
            quiet: true
            alignStart: true
            text: qsTrId("genycaster.nodes.local_example")
            onClicked: {
                readEndpoint.text = "http://127.0.0.1:3381"
                root.readContract = "snapchain-v1"
                root.selfHosted = true
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.nodes.local_warning")
            color: Theme.inkMuted
            wrapMode: Text.Wrap
            font.pixelSize: Theme.typeCaptionSize
        }
        RowLayout {
            Layout.fillWidth: true
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.providers.self_hosted")
                wrapMode: Text.Wrap
                color: Theme.ink
            }
            GenySwitch {
                checked: root.selfHosted
                onToggled: root.selfHosted = checked
            }
        }
        ColumnLayout {
            visible: Qt.platform.os === "wasm"
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareLabel {
                text: qsTrId("genycaster.nodes.api_key")
                color: Theme.ink
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareTextField {
                    id: readKey
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: qsTrId("genycaster.nodes.api_optional")
                    Accessible.name: qsTrId("genycaster.nodes.api_key")
                    onTextEdited: providerProfiles.clearDraftProbe()
                }
                ActionButton {
                    text: qsTrId("genycaster.nodes.paste")
                    compact: true
                    onClicked: {
                        root.pasteTarget = "key"
                        providerProfiles.requestBrowserPaste()
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.api_warning")
                color: Theme.warning
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeCaptionSize
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.space2
            Layout.preferredHeight: childrenRect.height
            ActionButton {
                compact: true
                text: qsTrId("genycaster.nodes.test")
                busy: providerProfiles.probeProviderId === "draft"
                      && providerProfiles.probeState === "checking"
                onClicked: providerProfiles.probeDraftNode(root.readContract, readEndpoint.text,
                                                           readKey.text)
            }
            ActionButton {
                prominent: true
                text: qsTrId("genycaster.providers.add")
                onClicked: {
                    const nodeName = readName.text.trim().length ? readName.text.trim()
                                    : qsTrId("genycaster.nodes.name_example")
                    const nodeId = "custom-" + Date.now().toString(36)
                    if (providerProfiles.addReadProfile(nodeId, nodeName, root.readContract,
                                                        readEndpoint.text.trim(), root.selfHosted)) {
                        const savedKey = readKey.text.length === 0
                            || providerProfiles.setSessionReadApiKey(nodeId, readKey.text)
                        if (!savedKey) {
                            providerProfiles.removeReadProfile(nodeId)
                            return
                        }
                        // Saving an unverified endpoint must not silently
                        // replace the current working read source.
                        providerProfiles.probeReadProvider(nodeId)
                        readName.clear()
                        readEndpoint.clear()
                        readKey.clear()
                        root.addingReader = false
                    }
                }
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: providerProfiles.probeProviderId === "draft"
            text: root.probeLabel()
                  + (providerProfiles.probeState === "healthy"
                     ? " · " + providerProfiles.probeLatencyMs + " ms" : "")
            color: providerProfiles.probeState === "healthy" ? Theme.positive : Theme.warning
            wrapMode: Text.Wrap
        }
        ActionButton {
            visible: providerProfiles.probeProviderId === "draft"
                     && providerProfiles.probeState === "contract_mismatch"
            compact: true
            alignStart: true
            text: qsTrId("genycaster.nodes.use_detected").arg(
                providerProfiles.probeSuggestedContract === "snapchain-v1"
                ? qsTrId("genycaster.nodes.raw") : qsTrId("genycaster.nodes.indexed"))
            onClicked: {
                root.readContract = providerProfiles.probeSuggestedContract
                providerProfiles.clearDraftProbe()
            }
        }
    }

    // Future node-provisioning surface. No server credentials are entered or
    // handled until audited provisioning and SSH permission flows are implemented.
    Superellipse {
        objectName: "serverProvisioningPreview"
        Layout.fillWidth: true
        implicitHeight: serverSetupBody.implicitHeight + Theme.space4 * 2
        fillColor: Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer
        ColumnLayout {
            id: serverSetupBody
            anchors.fill: parent
            anchors.margins: Theme.space4
            spacing: Theme.space3
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Icon {
                    text: Theme.iconCloud
                    preferredSize: Theme.iconSize
                    color: Theme.accent
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.nodes.provision_title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                }
                Superellipse {
                    Layout.preferredWidth: previewPill.implicitWidth + Theme.space3
                    Layout.preferredHeight: 28
                    fillColor: Theme.primarySurface
                    curveDepth: Theme.curveCompact
                    DirectionAwareLabel {
                        id: previewPill
                        anchors.centerIn: parent
                        text: qsTrId("genycaster.nodes.provision_soon")
                        color: Theme.accent
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.provision_desc")
                font.pixelSize: Theme.typeMetaSize
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            Repeater {
                model: [
                    qsTrId("genycaster.nodes.provision_personal"),
                    qsTrId("genycaster.nodes.provision_private"),
                    qsTrId("genycaster.nodes.provision_business")
                ]
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Icon {
                        text: Theme.iconCheck
                        color: Theme.positive
                        preferredSize: Theme.iconSizeSmall
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.ink
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.nodes.provision_pipeline")
                color: Theme.accent
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                wrapMode: Text.Wrap
            }
            ActionButton {
                Layout.fillWidth: true
                quiet: true
                alignStart: true
                selected: root.showServerGuide
                iconGlyph: Theme.iconInfo
                text: qsTrId("genycaster.nodes.provision_guide")
                onClicked: root.showServerGuide = !root.showServerGuide
            }
            ColumnLayout {
                visible: root.showServerGuide
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.nodes.provision_hardware")
                    color: Theme.ink
                    font.weight: Theme.weightDemiBold
                    font.pixelSize: Theme.typeLabelSize
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.nodes.provision_requirements")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.nodes.provision_security")
                    color: Theme.warning
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconArrowUpRight
                    alignStart: true
                    text: qsTrId("genycaster.nodes.provision_docs")
                    onClicked: Qt.openUrlExternally("https://github.com/farcasterxyz/snapchain")
                }
            }
        }
    }

    Superellipse {
        visible: providerProfiles.readRestartRequired
        Layout.fillWidth: true
        implicitHeight: reloadNote.implicitHeight + Theme.space3 * 2
        fillColor: Theme.surfaceMuted
        curveDepth: Theme.curveCompact
        DirectionAwareLabel {
            id: reloadNote
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space3
            text: qsTrId("genycaster.nodes.reload")
            color: Theme.warning
            wrapMode: Text.Wrap
        }
    }

    ActionButton {
        visible: Qt.platform.os === "wasm"
                 && (providerProfiles.readRestartRequired
                     || providerProfiles.writeRestartRequired)
        Layout.fillWidth: true
        prominent: true
        iconGlyph: Theme.iconRefresh
        text: qsTrId("genycaster.settings.wasm_refresh_now")
        enabled: !updateController.webRefreshing
        onClicked: updateController.refreshWebApp()
    }

    DirectionAwareLabel {
        visible: updateController.error.length > 0
        Layout.fillWidth: true
        text: updateController.error
        color: Theme.danger
        wrapMode: Text.Wrap
    }

    DirectionAwareLabel {
        visible: providerProfiles.lastError.length > 0
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.save_error") + " · " + providerProfiles.lastError
        color: Theme.danger
        wrapMode: Text.Wrap
    }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }

    DirectionAwareLabel {
        visible: providerProfiles.writeSupported || providerProfiles.writeProfiles.length > 0
        Layout.fillWidth: true
        text: qsTrId("genycaster.providers.write_title")
        color: Theme.ink
        font.pixelSize: Theme.typeTitle3Size
        font.weight: Theme.weightDemiBold
    }
    DirectionAwareLabel {
        visible: providerProfiles.writeSupported || providerProfiles.writeProfiles.length > 0
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.write_separate")
        color: Theme.inkMuted
        wrapMode: Text.Wrap
    }
    DirectionAwareLabel {
        visible: !providerProfiles.writeSupported
                 && providerProfiles.writeProfiles.length > 0
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.write_unavailable")
        color: Theme.inkSubtle
        wrapMode: Text.Wrap
    }
    Repeater {
        model: providerProfiles.writeProfiles
        delegate: ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: String(modelData.label) + " · " + String(modelData.endpoint)
                color: Theme.ink
                elide: Text.ElideMiddle
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.space2
                Layout.preferredHeight: childrenRect.height
                ActionButton {
                    compact: true
                    enabled: providerProfiles.writeSupported
                    selected: providerProfiles.selectedWriteProviderId === String(modelData.id)
                    text: qsTrId("genycaster.providers.use")
                    onClicked: providerProfiles.selectWriteProvider(String(modelData.id))
                }
                ActionButton {
                    compact: true
                    quiet: true
                    destructive: true
                    text: qsTrId("genycaster.providers.remove")
                    onClicked: providerProfiles.removeWriteProfile(String(modelData.id))
                }
            }
        }
    }
    ActionButton {
        visible: providerProfiles.writeSupported
        Layout.fillWidth: true
        alignStart: true
        text: qsTrId("genycaster.nodes.add_writer")
        selected: root.addingWriter
        onClicked: root.addingWriter = !root.addingWriter
    }
    ColumnLayout {
        visible: root.addingWriter && providerProfiles.writeSupported
        Layout.fillWidth: true
        spacing: Theme.space2
        DirectionAwareTextField {
            id: writeName
            Layout.fillWidth: true
            placeholderText: qsTrId("genycaster.nodes.name")
        }
        RowLayout {
            Layout.fillWidth: true
            DirectionAwareTextField {
                id: writeEndpoint
                Layout.fillWidth: true
                forceLeftToRight: true
                placeholderText: "https://validator.example.org"
            }
            ActionButton {
                compact: true
                text: qsTrId("genycaster.nodes.paste")
                onClicked: {
                    root.pasteTarget = "writer"
                    providerProfiles.requestBrowserPaste()
                }
            }
        }
        ActionButton {
            text: qsTrId("genycaster.providers.add")
            onClicked: {
                const id = "writer-" + Date.now().toString(36)
                if (providerProfiles.addWriteProfile(id, writeName.text.trim(),
                    "snapchain-v1", writeEndpoint.text.trim(), "", root.selfHosted)) {
                    writeName.clear()
                    writeEndpoint.clear()
                    root.addingWriter = false
                }
            }
        }
    }
    DirectionAwareLabel {
        visible: providerProfiles.writeRestartRequired
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.reload")
        color: Theme.warning
        wrapMode: Text.Wrap
    }
    DirectionAwareLabel {
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.test_note")
        color: Theme.inkSubtle
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
    }
}
