import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Provider configuration lives in UI composition. Only explicit, installed
// adapter contracts are selectable; URL probing alone never grants capabilities.
ColumnLayout {
    id: panel
    Layout.fillWidth: true
    spacing: Theme.space3

    property string readContract: providerProfiles.rawReadSupported ? "snapchain-v1" : "hypersnap-v2"
    property bool readSelfHosted: true
    property bool writeSelfHosted: true
    property bool editingRead: false
    property bool editingWrite: false

    // The legacy network toggle shares the persisted Read preference.
    // Reevaluate our selected-state bindings when it clears that preference.
    Connections {
        target: presentationBridge
        function onPreferredNetworkChanged() { providerProfiles.refresh() }
    }

    function readableError(code) {
        switch (String(code || "")) {
        case "invalid_id": return qsTrId("genycaster.providers.error_invalid_id")
        case "invalid_label": return qsTrId("genycaster.providers.error_invalid_label")
        case "invalid_endpoint": return qsTrId("genycaster.providers.error_invalid_endpoint")
        case "unsupported_contract": return qsTrId("genycaster.providers.error_unsupported")
        case "duplicate_id": return qsTrId("genycaster.providers.error_duplicate")
        case "limit_reached": return qsTrId("genycaster.providers.error_limit")
        case "unknown_provider": return qsTrId("genycaster.providers.error_unknown")
        case "storage": return qsTrId("genycaster.providers.error_storage")
        default: return ""
        }
    }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.title")
            color: Theme.ink
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightBold
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.description")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }

    DirectionAwareLabel {
        visible: providerProfiles.lastError.length > 0
        Layout.fillWidth: true
        text: panel.readableError(providerProfiles.lastError)
        color: Theme.danger
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.read_title")
            color: Theme.ink
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
        }
        ActionButton {
            Layout.fillWidth: true
            alignStart: true
            text: qsTrId("genycaster.providers.default_read")
            selected: providerProfiles.selectedReadProviderId.length === 0
            onClicked: providerProfiles.selectReadProvider("")
        }
        ActionButton {
            visible: providerProfiles.indexedReadSupported
            Layout.fillWidth: true
            alignStart: true
            text: qsTrId("genycaster.settings.hypersnap_label")
            selected: providerProfiles.selectedReadProviderId === "hypersnap-v2"
            onClicked: providerProfiles.selectReadProvider("hypersnap-v2")
        }
        Repeater {
            model: providerProfiles.readProfiles
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.label || modelData.id || "")
                        color: Theme.ink
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.contract || "") + " · " + String(modelData.endpoint || "")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                    }
                }
                ActionButton {
                    compact: true
                    selected: providerProfiles.selectedReadProviderId === String(modelData.id)
                    text: qsTrId("genycaster.providers.use")
                    onClicked: providerProfiles.selectReadProvider(String(modelData.id))
                }
                ActionButton {
                    compact: true
                    quiet: true
                    destructive: true
                    iconOnly: true
                    iconGlyph: Theme.iconTrash
                    text: qsTrId("genycaster.providers.remove")
                    onClicked: providerProfiles.removeReadProfile(String(modelData.id))
                }
            }
        }
        ActionButton {
            Layout.fillWidth: true
            alignStart: true
            iconGlyph: Theme.iconPlus
            text: qsTrId("genycaster.providers.add_read")
            selected: panel.editingRead
            onClicked: panel.editingRead = !panel.editingRead
        }
        RowLayout {
            visible: panel.editingRead
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ActionButton {
                Layout.fillWidth: true
                text: qsTrId("genycaster.providers.raw_contract")
                enabled: providerProfiles.rawReadSupported
                selected: panel.readContract === "snapchain-v1"
                onClicked: panel.readContract = "snapchain-v1"
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTrId("genycaster.providers.indexed_contract")
                enabled: providerProfiles.indexedReadSupported
                selected: panel.readContract === "hypersnap-v2"
                onClicked: panel.readContract = "hypersnap-v2"
            }
        }
        DirectionAwareTextField {
            id: readIdField
            visible: panel.editingRead
            Layout.fillWidth: true
            forceLeftToRight: true
            placeholderText: qsTrId("genycaster.providers.id_placeholder")
            Accessible.name: qsTrId("genycaster.providers.id_placeholder")
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        }
        DirectionAwareTextField {
            id: readNameField
            visible: panel.editingRead
            Layout.fillWidth: true
            placeholderText: qsTrId("genycaster.providers.label_placeholder")
            Accessible.name: qsTrId("genycaster.providers.label_placeholder")
        }
        DirectionAwareTextField {
            id: readUrlField
            visible: panel.editingRead
            Layout.fillWidth: true
            forceLeftToRight: true
            placeholderText: "https://read.example.org"
            Accessible.name: qsTrId("genycaster.providers.read_endpoint")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
        }
        RowLayout {
            visible: panel.editingRead
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.providers.self_hosted")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            GenySwitch {
                checked: panel.readSelfHosted
                onToggled: panel.readSelfHosted = checked
                Accessible.name: qsTrId("genycaster.providers.self_hosted")
            }
            ActionButton {
                compact: true
                iconGlyph: Theme.iconPlus
                text: qsTrId("genycaster.providers.add")
                onClicked: {
                    if (providerProfiles.addReadProfile(
                            readIdField.text, readNameField.text, panel.readContract,
                            readUrlField.text, panel.readSelfHosted)) {
                        readIdField.clear()
                        readNameField.clear()
                        readUrlField.clear()
                        panel.editingRead = false
                    }
                }
            }
        }
    }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.write_title")
            color: Theme.ink
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.write_description")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        ActionButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.default_write")
            alignStart: true
            selected: providerProfiles.selectedWriteProviderId.length === 0
            enabled: providerProfiles.writeSupported
            onClicked: providerProfiles.selectWriteProvider("")
        }
        Repeater {
            model: providerProfiles.writeProfiles
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.label || modelData.id || "")
                        color: Theme.ink
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.contract || "") + " · " + String(modelData.endpoint || "")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                    }
                }
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
                    iconOnly: true
                    iconGlyph: Theme.iconTrash
                    text: qsTrId("genycaster.providers.remove")
                    onClicked: providerProfiles.removeWriteProfile(String(modelData.id))
                }
            }
        }
        DirectionAwareLabel {
            visible: !providerProfiles.writeSupported
            Layout.fillWidth: true
            text: qsTrId("genycaster.providers.error_unsupported")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        ActionButton {
            Layout.fillWidth: true
            alignStart: true
            enabled: providerProfiles.writeSupported
            iconGlyph: Theme.iconPlus
            text: qsTrId("genycaster.providers.add")
            selected: panel.editingWrite
            onClicked: panel.editingWrite = !panel.editingWrite
        }
        DirectionAwareTextField {
            id: writeIdField
            visible: panel.editingWrite
            Layout.fillWidth: true
            enabled: providerProfiles.writeSupported
            forceLeftToRight: true
            placeholderText: qsTrId("genycaster.providers.id_placeholder")
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        }
        DirectionAwareTextField {
            id: writeNameField
            visible: panel.editingWrite
            Layout.fillWidth: true
            enabled: providerProfiles.writeSupported
            placeholderText: qsTrId("genycaster.providers.label_placeholder")
        }
        DirectionAwareTextField {
            id: writeUrlField
            visible: panel.editingWrite
            Layout.fillWidth: true
            enabled: providerProfiles.writeSupported
            forceLeftToRight: true
            placeholderText: "https://validator.example.org"
            Accessible.name: qsTrId("genycaster.providers.write_endpoint")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
        }
        DirectionAwareTextField {
            id: writerReadUrlField
            visible: panel.editingWrite
            Layout.fillWidth: true
            enabled: providerProfiles.writeSupported
            forceLeftToRight: true
            placeholderText: qsTrId("genycaster.providers.optional_read_endpoint")
            Accessible.name: qsTrId("genycaster.providers.optional_read_endpoint")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
        }
        RowLayout {
            visible: panel.editingWrite
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.providers.self_hosted")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            GenySwitch {
                checked: panel.writeSelfHosted
                enabled: providerProfiles.writeSupported
                onToggled: panel.writeSelfHosted = checked
                Accessible.name: qsTrId("genycaster.providers.self_hosted")
            }
            ActionButton {
                compact: true
                enabled: providerProfiles.writeSupported
                iconGlyph: Theme.iconPlus
                text: qsTrId("genycaster.providers.add")
                onClicked: {
                    if (providerProfiles.addWriteProfile(
                            writeIdField.text, writeNameField.text, "snapchain-v1",
                            writeUrlField.text, writerReadUrlField.text, panel.writeSelfHosted)) {
                        writeIdField.clear()
                        writeNameField.clear()
                        writeUrlField.clear()
                        writerReadUrlField.clear()
                        panel.editingWrite = false
                    }
                }
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: providerProfiles.readRestartRequired || providerProfiles.writeRestartRequired
        text: qsTrId("genycaster.settings.restart_genycaster_to_apply_network_changes")
        color: Theme.warning
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
    }
    DirectionAwareLabel {
        Layout.fillWidth: true
        text: qsTrId("genycaster.providers.safety_note")
        color: Theme.inkMuted
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
    }
}
