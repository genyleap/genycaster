#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QString>
#include <QUrl>

#include <cassert>
#include <utility>

#include "media_upload_controller.hpp"
#include "rich_composer_controller.hpp"

namespace {

void testPublishedAssetCleanupIsDurableAndIdempotent()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};

    assert(composer.insertManagedMedia(
        QStringLiteral("https://media-farcaster.genyleap.com/m/v1/2026/08/11111111-1111-1111-1111-111111111111.png"),
        QStringLiteral("image/png"),
        QStringLiteral("managed-token-1")));
    composer.commitPublishedCast(QStringLiteral("cast-1"));
    assert(composer.pendingManagedAssetCleanup().isEmpty());

    const auto released = composer.releaseManagedAssetsForCast(QStringLiteral("cast-1"));
    assert(released == QStringList{QStringLiteral("managed-token-1")});
    assert(composer.pendingManagedAssetCleanup() == QStringList{QStringLiteral("managed-token-1")});

    const auto releasedAgain = composer.releaseManagedAssetsForCast(QStringLiteral("cast-1"));
    assert(releasedAgain.isEmpty());
    composer.markManagedAssetCleanupCompleted(QStringLiteral("managed-token-1"));
    assert(composer.pendingManagedAssetCleanup().isEmpty());
}

void testSharedManagedAssetIsDeletedOnlyAfterLastLocalCastReference()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};
    const auto url = QStringLiteral(
        "https://media-farcaster.genyleap.com/m/v1/2026/08/22222222-2222-2222-2222-222222222222.png");
    const auto token = QStringLiteral("managed-token-shared");

    assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    composer.commitPublishedCast(QStringLiteral("cast-a"));
    composer.newDocument();
    assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    composer.commitPublishedCast(QStringLiteral("cast-b"));

    assert(composer.releaseManagedAssetsForCast(QStringLiteral("cast-a")).isEmpty());
    assert(composer.pendingManagedAssetCleanup().isEmpty());

    const auto finalRelease = composer.releaseManagedAssetsForCast(QStringLiteral("cast-b"));
    assert(finalRelease == QStringList{token});
    assert(composer.pendingManagedAssetCleanup() == QStringList{token});
}

void testDiscardedDraftQueuesManagedAssetForCleanup()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};

    assert(composer.insertManagedMedia(
        QStringLiteral("https://media-farcaster.genyleap.com/m/v1/2026/08/33333333-3333-3333-3333-333333333333.png"),
        QStringLiteral("image/png"),
        QStringLiteral("managed-token-draft")));
    composer.saveDraft();
    composer.clearDraft();

    assert(composer.pendingManagedAssetCleanup() ==
        QStringList{QStringLiteral("managed-token-draft")});
}

void testStartingNewDocumentQueuesAbandonedManagedAsset()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};

    assert(composer.insertManagedMedia(
        QStringLiteral("https://media-farcaster.genyleap.com/m/v1/2026/08/55555555-5555-5555-5555-555555555555.png"),
        QStringLiteral("image/png"),
        QStringLiteral("managed-token-new-document")));

    composer.newDocument();

    assert(composer.pendingManagedAssetCleanup() ==
        QStringList{QStringLiteral("managed-token-new-document")});
    assert(!composer.restoreDraft());
}

void testRemovingManagedMediaQueuesImmediateCleanup()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};
    const auto url = QStringLiteral(
        "https://media-farcaster.genyleap.com/m/v1/2026/08/66666666-6666-6666-6666-666666666666.png");
    const auto token = QStringLiteral("managed-token-remove-block");
    int cleanupRequests = 0;
    QObject::connect(
        &composer,
        &RichComposerController::managedAssetCleanupRequested,
        &composer,
        [&cleanupRequests] { ++cleanupRequests; });

    assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    assert(composer.removeBlock(1));

    assert(cleanupRequests == 1);
    assert(composer.pendingManagedAssetCleanup() == QStringList{token});

    RichComposerController restored{settings};
    assert(restored.restoreDraft());
    for (const auto& block : restored.blocks()) {
        assert(block.toMap().value(QStringLiteral("url")).toString() != url);
    }
}

void testRemovingOneOfMultipleManagedMediaReferencesRetainsObject()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};
    const auto url = QStringLiteral(
        "https://media-farcaster.genyleap.com/m/v1/2026/08/77777777-7777-7777-7777-777777777777.png");
    const auto token = QStringLiteral("managed-token-duplicate-media");
    int cleanupRequests = 0;
    QObject::connect(
        &composer,
        &RichComposerController::managedAssetCleanupRequested,
        &composer,
        [&cleanupRequests] { ++cleanupRequests; });

    assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    assert(composer.duplicateBlock(1));
    assert(composer.removeBlock(1));
    assert(cleanupRequests == 0);
    assert(composer.pendingManagedAssetCleanup().isEmpty());

    assert(composer.removeBlock(1));
    assert(cleanupRequests == 1);
    assert(composer.pendingManagedAssetCleanup() == QStringList{token});
}

void testDraftRemovalDoesNotDeleteAssetReferencedByPublishedCast()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    RichComposerController composer{settings};
    const auto url = QStringLiteral(
        "https://media-farcaster.genyleap.com/m/v1/2026/08/88888888-8888-8888-8888-888888888888.png");
    const auto token = QStringLiteral("managed-token-published-reference");
    int cleanupRequests = 0;
    QObject::connect(
        &composer,
        &RichComposerController::managedAssetCleanupRequested,
        &composer,
        [&cleanupRequests] { ++cleanupRequests; });

    assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    composer.commitPublishedCast(QStringLiteral("cast-with-managed-media"));
    composer.newDocument();
    assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    assert(composer.removeBlock(1));

    assert(cleanupRequests == 0);
    assert(composer.pendingManagedAssetCleanup().isEmpty());
}

void testManagedAssetTokenSurvivesImmediateRestart()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    const auto url = QStringLiteral(
        "https://media-farcaster.genyleap.com/m/v1/2026/08/44444444-4444-4444-4444-444444444444.png");
    const auto token = QStringLiteral("managed-token-restart");

    {
        RichComposerController composer{settings};
        assert(composer.insertManagedMedia(url, QStringLiteral("image/png"), token));
    }

    RichComposerController restored{settings};
    assert(restored.restoreDraft());
    restored.commitPublishedCast(QStringLiteral("cast-after-restart"));
    assert(restored.releaseManagedAssetsForCast(QStringLiteral("cast-after-restart")) ==
        QStringList{token});
}

void testMediaUploadControllerClassifiesPreviewBeforeNativeUpload()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    const QString videoPath = directory.filePath(QStringLiteral("preview.mp4"));
    QFile video{videoPath};
    assert(video.open(QIODevice::WriteOnly));
    assert(video.write("test-video-bytes") > 0);
    video.close();

    MediaUploadController controller{};
    controller.setAvailable(true);
    QString requestedPath{};
    int requestedIndex = -2;
    QObject::connect(
        &controller,
        &MediaUploadController::uploadRequested,
        &controller,
        [&requestedPath, &requestedIndex](QString path, const int afterIndex) {
            requestedPath = std::move(path);
            requestedIndex = afterIndex;
        });

    assert(controller.beginUpload(QUrl::fromLocalFile(videoPath), 3));
    assert(controller.busy());
    assert(controller.previewKind() == QStringLiteral("video"));
    assert(controller.selectedContentType() == QStringLiteral("video/mp4"));
    assert(controller.localPreviewUrl() == QUrl::fromLocalFile(videoPath));
    assert(controller.totalBytes() > 0U);
    assert(requestedPath == videoPath);
    assert(requestedIndex == 3);
}

void testMediaUploadControllerRejectsUnsupportedFilesBeforePreviewOrTransport()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    const QString audioPath = directory.filePath(QStringLiteral("unsupported.mp3"));
    QFile audio{audioPath};
    assert(audio.open(QIODevice::WriteOnly));
    assert(audio.write("test-audio-bytes") > 0);
    audio.close();

    MediaUploadController controller{};
    controller.setAvailable(true);
    int uploadRequests = 0;
    QObject::connect(
        &controller,
        &MediaUploadController::uploadRequested,
        &controller,
        [&uploadRequests] { ++uploadRequests; });

    assert(!controller.beginUpload(QUrl::fromLocalFile(audioPath)));
    assert(!controller.busy());
    assert(uploadRequests == 0);
    assert(controller.localPreviewUrl().isEmpty());
    assert(controller.previewKind().isEmpty());
    assert(controller.selectedContentType().isEmpty());
    assert(!controller.errorText().isEmpty());
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    testPublishedAssetCleanupIsDurableAndIdempotent();
    testSharedManagedAssetIsDeletedOnlyAfterLastLocalCastReference();
    testDiscardedDraftQueuesManagedAssetForCleanup();
    testStartingNewDocumentQueuesAbandonedManagedAsset();
    testRemovingManagedMediaQueuesImmediateCleanup();
    testRemovingOneOfMultipleManagedMediaReferencesRetainsObject();
    testDraftRemovalDoesNotDeleteAssetReferencedByPublishedCast();
    testManagedAssetTokenSurvivesImmediateRestart();
    testMediaUploadControllerClassifiesPreviewBeforeNativeUpload();
    testMediaUploadControllerRejectsUnsupportedFilesBeforePreviewOrTransport();
    return 0;
}
