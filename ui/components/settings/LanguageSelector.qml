import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GenyComboBox {
    id: root
    property string languageId: "auto"
    signal languageSelected(string language)

    model: richComposer.languages
    textRole: "label"
    valueRole: "id"
    popupMaximumWidth: 420
    implicitWidth: 210

    Component.onCompleted: syncIndex()
    onLanguageIdChanged: syncIndex()
    onActivated: {
        const selected = currentValue === undefined ? "auto" : String(currentValue)
        root.languageId = selected
        root.languageSelected(selected)
    }

    function syncIndex() {
        for (let i = 0; i < count; ++i) {
            if (String(valueAt(i)) === root.languageId) {
                currentIndex = i
                return
            }
        }
        currentIndex = 0
    }
}
