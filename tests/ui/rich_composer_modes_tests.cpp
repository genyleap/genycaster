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
    return 0;
}
