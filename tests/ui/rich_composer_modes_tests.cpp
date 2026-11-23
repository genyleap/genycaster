#include "rich_composer_controller.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariantMap>

#include <algorithm>

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    QTemporaryDir temporary{};
    if (!temporary.isValid()) return 1;
    QSettings settings{temporary.filePath(QStringLiteral("draft.ini")), QSettings::IniFormat};
    RichComposerController editor{settings};
    editor.newDocument();
    if (!editor.setBlockText(0, QStringLiteral("Original body"))) return 2;

    const QString image = QStringLiteral("https://media-farcaster.genyleap.com/m/public-image.png");
    if (!editor.insertMedia(image, QStringLiteral("image/png"), QStringLiteral("Photo"), -1))
        return 3;
    if (editor.markdownBodySource() != QStringLiteral("Original body"))
        return 4;

    const auto mediaCount = [&] {
        const QVariantList blocks = editor.blocks();
        return std::count_if(blocks.cbegin(), blocks.cend(), [](const QVariant& block) {
            return block.toMap().value(QStringLiteral("kind")).toString()
                == QStringLiteral("media");
        });
    };

    if (mediaCount() != 1) return 5;
    editor.setMarkdownBodySource(QStringLiteral("**Edited text**"));
    if (mediaCount() != 1 || !editor.safeText().contains(QStringLiteral("Edited text")))
        return 6;
    if (editor.markdownBodySource() != QStringLiteral("**Edited text**"))
        return 7;
    if (editor.protocolEmbeds().size() != 1) return 8;

    // Switching into a preview and back does not reconstruct a draft from a
    // bare URL; changing Markdown again must keep the same typed attachment.
    editor.setMarkdownBodySource(QStringLiteral("Final message"));
    if (mediaCount() != 1 || editor.protocolEmbeds().size() != 1) return 9;

    int mediaIndex = -1;
    const QVariantList blocks = editor.blocks();
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks.at(i).toMap().value(QStringLiteral("kind")).toString()
            == QStringLiteral("media")) {
            mediaIndex = i;
            break;
        }
    }
    if (mediaIndex < 0 || !editor.removeBlock(mediaIndex)) return 10;
    if (mediaCount() != 0 || !editor.protocolEmbeds().isEmpty()) return 11;

    // Ordinary draft cancellation queues an owner-scoped deletion capability
    // before it clears the document and emits the immediate cleanup request.
    int cleanupSignals = 0;
    QObject::connect(&editor, &RichComposerController::managedAssetCleanupRequested,
                     &app, [&cleanupSignals] { ++cleanupSignals; });
    const QString ordinaryToken = QStringLiteral("opaque-ordinary-test-management-token");
    if (!editor.insertManagedMedia(image, QStringLiteral("image/png"),
                                   ordinaryToken, QStringLiteral("Photo"), -1))
        return 12;
    editor.clearCurrentContent(false);
    if (mediaCount() != 0 || !editor.pendingManagedAssetCleanup().contains(ordinaryToken)
        || cleanupSignals != 1) return 13;
    editor.markManagedAssetCleanupCompleted(ordinaryToken);

    // Context replies/quotes must not clear the user's unrelated saved draft,
    // yet must also queue their ephemeral uploaded objects for deletion.
    editor.setBlockText(0, QStringLiteral("Ordinary draft"));
    editor.saveDraft();
    editor.beginContextDocument();
    const QString replyToken = QStringLiteral("opaque-reply-test-management-token");
    if (!editor.insertManagedMedia(image, QStringLiteral("image/png"),
                                   replyToken, QStringLiteral("Photo"), -1))
        return 14;
    editor.clearCurrentContent(true);
    if (mediaCount() != 0 || !editor.pendingManagedAssetCleanup().contains(replyToken)
        || cleanupSignals != 2) return 15;
    editor.markManagedAssetCleanupCompleted(replyToken);
    if (!editor.restoreDraft()
        || !editor.safeText().contains(QStringLiteral("Ordinary draft")))
        return 16;

    // A published sidecar retains ownership of its URL; clearing another
    // draft cannot delete the image while a confirmed Cast references it.
    const QString publishedToken = QStringLiteral("opaque-published-managed-token");
    if (!editor.insertManagedMedia(image, QStringLiteral("image/png"),
                                   publishedToken, QStringLiteral("Photo"), -1))
        return 17;
    editor.commitPublishedCast(QStringLiteral("confirmed-cast-for-regression"));
    editor.newDocument();
    if (!editor.insertManagedMedia(image, QStringLiteral("image/png"),
                                   publishedToken, QStringLiteral("Photo"), -1))
        return 18;
    editor.clearCurrentContent(false);
    if (editor.pendingManagedAssetCleanup().contains(publishedToken)) return 19;
    return 0;
}
