/*!
 * @file        ui/app/browser_emoji_recovery.hpp
 * @brief       Browser emoji text-input recovery adapter.
 *
 * @details
 * Reconciles committed emoji input across WebAssembly and mobile Safari
 * event differences without duplicating text entered by Qt Quick.
 *
 * @author      Kambiz Asadzadeh
 * @since       26 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

// A narrow iOS Safari / Qt WebAssembly input adapter. Emoji keys are often
// delivered as beforeinput text without a corresponding Qt Quick key event.
// Only recover a committed emoji if Qt's focused composer still has exactly
// the text it had before that event; ordinary IME, paste and native Qt clients
// are completely untouched.
#if defined(__EMSCRIPTEN__)

#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QObject>
#include <QPointer>
#include <QString>
#include <emscripten.h>

namespace genycaster::browser_emoji {
inline QPointer<QObject> pendingEditor{};
inline QString pendingBeforeText{};
inline QString pendingEmoji{};
inline int serial = 0;

inline QObject* focusedComposerEditor()
{
    QObject* focused = QGuiApplication::focusObject();
    for (QObject* node = focused; node; node = node->parent()) {
        if (node->objectName() == QStringLiteral("composerPrimaryMobileEditor")
            && node->property("activeFocus").toBool())
            return node;
    }
    return nullptr;
}
} // namespace genycaster::browser_emoji

extern "C" EMSCRIPTEN_KEEPALIVE int genycasterCaptureBrowserEmoji(const char* utf8)
{
    auto* editor = genycaster::browser_emoji::focusedComposerEditor();
    if (!editor || !utf8 || !*utf8) return 0;
    const QString emoji = QString::fromUtf8(utf8);
    if (emoji.isEmpty() || emoji.size() > 32) return 0;
    genycaster::browser_emoji::pendingEditor = editor;
    genycaster::browser_emoji::pendingBeforeText = editor->property("text").toString();
    genycaster::browser_emoji::pendingEmoji = emoji;
    return ++genycaster::browser_emoji::serial;
}

extern "C" EMSCRIPTEN_KEEPALIVE void genycasterRecoverBrowserEmoji(int nonce)
{
    namespace bridge = genycaster::browser_emoji;
    QObject* editor = bridge::pendingEditor;
    if (!editor || nonce != bridge::serial || editor != bridge::focusedComposerEditor())
        return;
    if (editor->property("text").toString() != bridge::pendingBeforeText
        || !editor->property("preeditText").toString().isEmpty())
        return;

    // Deliver Unicode through Qt's normal edit handling so cursor position,
    // selection and the QML onTextChanged/undo path remain authoritative.
    QKeyEvent event(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier,
                    bridge::pendingEmoji);
    QCoreApplication::sendEvent(editor, &event);

    // Route a committed UTF-8 string through Qt's IME handler when the
    // synthetic key does not reach a WebKit-backed editor.
    if (editor->property("text").toString() == bridge::pendingBeforeText) {
        QInputMethodEvent committed{};
        committed.setCommitString(bridge::pendingEmoji);
        QCoreApplication::sendEvent(editor, &committed);
    }

    // TextArea's regular insertion API is the last bounded fallback.
    if (editor->property("text").toString() == bridge::pendingBeforeText) {
        const int position = editor->property("cursorPosition").toInt();
        QMetaObject::invokeMethod(editor, "insert", Qt::DirectConnection,
                                  Q_ARG(int, position),
                                  Q_ARG(QString, bridge::pendingEmoji));
    }
}

EM_JS(void, genycasterInstallBrowserEmojiRecovery, (), {
    if (typeof document === "undefined" || window.__genycasterEmojiRecoveryInstalled)
        return;
    window.__genycasterEmojiRecoveryInstalled = true;
    const isEmoji = /[\u{1F000}-\u{1FAFF}\u2600-\u27BF]/u;
    document.addEventListener("beforeinput", function(event) {
        if (event.inputType !== "insertText" || !event.data
            || !isEmoji.test(event.data)) return;
        const capture = Module["_genycasterCaptureBrowserEmoji"];
        const recover = Module["_genycasterRecoverBrowserEmoji"];
        if (typeof capture !== "function" || typeof recover !== "function")
            return;
        const value = String(event.data);
        const bytes = lengthBytesUTF8(value) + 1;
        const buffer = _malloc(bytes);
        if (!buffer) return;
        stringToUTF8(value, buffer, bytes);
        const nonce = capture(buffer);
        _free(buffer);
        if (nonce > 0) setTimeout(function() { recover(nonce); }, 70);
    }, true);
});

#endif
