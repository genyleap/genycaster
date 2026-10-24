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
    property bool showAdvanced: false
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
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: providerProfiles.activeReadProviderId === "hypersnap-v2"
                      ? "HyperSnap v2" : providerProfiles.activeReadProviderLabel
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightDemiBold
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
                text: qsTrId("genycaster.nodes.raw_limitations")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.warning
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
            Flow {
                Layout.fillWidth: true
                width: parent.width
                spacing: Theme.space2
                Layout.preferredHeight: childrenRect.height
                ActionButton {
                    compact: true
                    iconGlyph: Theme.iconRefresh
                    text: qsTrId("genycaster.nodes.test")
                    busy: providerProfiles.probeState === "checking"
                          && providerProfiles.probeProviderId === providerProfiles.activeReadProviderId
                    onClicked: providerProfiles.probeReadProvider(providerProfiles.activeReadProviderId)
                }
                DirectionAwareLabel {
                    visible: providerProfiles.probeProviderId === providerProfiles.activeReadProviderId
                    text: root.probeLabel()
                          + (providerProfiles.probeState === "healthy"
                             ? " · " + providerProfiles.probeLatencyMs + " ms" : "")
                    font.pixelSize: Theme.typeCaptionSize
                    color: providerProfiles.probeState === "healthy"
                           ? Theme.positive : Theme.warning
                    verticalAlignment: Text.AlignVCenter
                    height: Theme.compactControlHeight
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
        ActionButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.default_read") + " · HyperSnap"
            alignStart: true
            selected: providerProfiles.selectedReadProviderId.length === 0
            onClicked: providerProfiles.selectReadProvider("")
        }
        Repeater {
            model: providerProfiles.readProfiles
            delegate: Superellipse {
                required property var modelData
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
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: String(modelData.id) === providerProfiles.activeReadProviderId
                        text: qsTrId("genycaster.nodes.active_tag")
                        color: Theme.positive
                        font.pixelSize: Theme.typeCaptionSize
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
                              ? qsTrId("genycaster.nodes.raw_limitations")
                              : qsTrId("genycaster.nodes.indexed_description")
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
                            text: qsTrId("genycaster.nodes.test")
                            busy: providerProfiles.probeProviderId === String(modelData.id)
                                  && providerProfiles.probeState === "checking"
                            onClicked: providerProfiles.probeReadProvider(String(modelData.id))
                        }
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
                            destructive: true
                            text: qsTrId("genycaster.providers.remove")
                            onClicked: providerProfiles.removeReadProfile(String(modelData.id))
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        visible: Qt.platform.os === "wasm"
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
        ActionButton {
            Layout.fillWidth: true
            alignStart: true
            quiet: true
            text: qsTrId("genycaster.nodes.advanced")
            selected: root.showAdvanced
            onClicked: root.showAdvanced = !root.showAdvanced
        }
        ColumnLayout {
            visible: root.showAdvanced
            Layout.fillWidth: true
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.providers.compatible_read_fallback")
                    color: Theme.ink
                    wrapMode: Text.Wrap
                }
                GenySwitch {
                    checked: providerProfiles.compatibleReadFallback
                    onToggled: providerProfiles.setCompatibleReadFallback(checked)
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.providers.compatible_read_fallback_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
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
        Layout.fillWidth: true
        text: qsTrId("genycaster.providers.write_title")
        color: Theme.ink
        font.pixelSize: Theme.typeTitle3Size
        font.weight: Theme.weightDemiBold
    }
    DirectionAwareLabel {
        Layout.fillWidth: true
        text: qsTrId("genycaster.nodes.write_separate")
        color: Theme.inkMuted
        wrapMode: Text.Wrap
    }
    DirectionAwareLabel {
        visible: !providerProfiles.writeSupported
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
