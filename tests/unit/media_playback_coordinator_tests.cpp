// Deterministic lifecycle tests for the app-wide media playback coordinator.
//
// The coordinator arbitrates one primary audible session across recycled feed
// delegates. Decoding itself is a Qt Multimedia platform concern; this test
// verifies the arbitration/lifecycle contract with lightweight stand-ins that
// expose the same invokable contract as GenyVideoPlayer.
#include <cassert>

#include <QCoreApplication>
#include <QObject>
#include <QSettings>
#include <QTemporaryDir>

#include "media_playback_coordinator.hpp"

namespace {

class FakePlayer final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    Q_INVOKABLE void suspendFromPlaybackCoordinator()
    {
        ++suspendCount;
    }

    int suspendCount{0};
};

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application{argc, argv};

    MediaPlaybackCoordinator coordinator{};
    FakePlayer first{};
    FakePlayer second{};

    // A new primary suspends the previous audible session.
    assert(coordinator.claimPrimary(&first));
    assert(coordinator.activePlayer() == &first);
    assert(first.suspendCount == 0);
    assert(coordinator.claimPrimary(&second));
    assert(coordinator.activePlayer() == &second);
    assert(first.suspendCount == 1);
    assert(second.suspendCount == 0);

    // Re-claiming the same player is idempotent and never re-suspends it.
    assert(coordinator.claimPrimary(&second));
    assert(second.suspendCount == 0);
    assert(coordinator.activePlayer() == &second);

    // Explicit pause / release relinquishes the session.
    coordinator.release(&second);
    assert(coordinator.activePlayer() == nullptr);
    assert(coordinator.isPrimary(&second) == false);

    // Releasing a non-active player is a no-op.
    coordinator.release(&first);
    assert(coordinator.activePlayer() == nullptr);

    // Null guards.
    assert(!coordinator.claimPrimary(nullptr));
    assert(!coordinator.isPrimary(nullptr));
    coordinator.release(nullptr);

    // Playback tuning is type-safe and never treats an arbitrary QML object as
    // a media backend. Actual FFmpeg option effects are integration-tested on
    // Qt 6.10+ targets where QMediaPlayer is available.
    assert(!coordinator.prepareForSource(&first, QStringLiteral("https://example.com/video.m3u8")));
    assert(!coordinator.prepareForSource(nullptr, QStringLiteral("https://example.com/video.m3u8")));

    // Remote source preparation is deferred, so QML receives the request id
    // before any backend completion signal can arrive.
    qulonglong backendRequestId{};
    QString backendSource{};
    coordinator.setSourcePreparationBackend({
        .start = [&backendRequestId, &backendSource](
            const qulonglong requestId,
            const QString& source) {
            backendRequestId = requestId;
            backendSource = source;
        },
    });
    qulonglong preparedRequestId{};
    QString preparedSource{};
    QObject::connect(
        &coordinator,
        &MediaPlaybackCoordinator::sourcePrepared,
        &coordinator,
        [&preparedRequestId, &preparedSource](const qulonglong requestId, const QString& source) {
            preparedRequestId = requestId;
            preparedSource = source;
        });
    bool preparationFailed{};
    QObject::connect(
        &coordinator,
        &MediaPlaybackCoordinator::sourcePreparationFailed,
        &coordinator,
        [&preparationFailed](qulonglong, const QString&, const QString&) {
            preparationFailed = true;
        });
    const QString remoteSource = QStringLiteral("https://media.example/video.m3u8");
    const qulonglong remoteRequestId = coordinator.prepareSource(remoteSource);
    assert(remoteRequestId != 0);
    assert(backendRequestId == 0);
    QCoreApplication::processEvents();
    assert(backendRequestId == remoteRequestId);
    assert(backendSource == remoteSource);
    coordinator.completeSourcePreparation(remoteRequestId, remoteSource, true);
    assert(preparedRequestId == remoteRequestId);
    assert(preparedSource == remoteSource);
    assert(!preparationFailed);

    // Local files never enter the network backend, while malformed remote
    // URLs fail asynchronously and remain recoverable.
    backendRequestId = 0;
    preparedRequestId = 0;
    const QString localSource = QStringLiteral("file:///tmp/video.mp4");
    const qulonglong localRequestId = coordinator.prepareSource(localSource);
    QCoreApplication::processEvents();
    assert(backendRequestId == 0);
    assert(preparedRequestId == localRequestId);
    preparationFailed = false;
    const qulonglong invalidRequestId = coordinator.prepareSource(QStringLiteral("https:///missing-host"));
    assert(invalidRequestId != 0);
    QCoreApplication::processEvents();
    assert(preparationFailed);

    MediaPlaybackCoordinator unavailableCoordinator{};
    bool unavailableFailed{};
    QObject::connect(
        &unavailableCoordinator,
        &MediaPlaybackCoordinator::sourcePreparationFailed,
        &unavailableCoordinator,
        [&unavailableFailed](qulonglong, const QString&, const QString&) {
            unavailableFailed = true;
        });
    assert(unavailableCoordinator.prepareSource(remoteSource) != 0);
    QCoreApplication::processEvents();
    assert(unavailableFailed);

    // Delegates can claim again after release.
    assert(coordinator.claimPrimary(&first));
    assert(coordinator.activePlayer() == &first);

    const int suspendedBeforeReset = first.suspendCount;
    coordinator.releaseAll();
    assert(coordinator.activePlayer() == nullptr);
    assert(first.suspendCount == suspendedBeforeReset + 1);
    coordinator.releaseAll();
    assert(first.suspendCount == suspendedBeforeReset + 1);

    // Session mute preference is app-wide and signalled.
    bool sessionChanged = false;
    QObject::connect(&coordinator, &MediaPlaybackCoordinator::sessionChanged,
        &coordinator, [&sessionChanged] { sessionChanged = true; });
    coordinator.setMuted(true);
    assert(coordinator.muted());
    assert(sessionChanged);
    sessionChanged = false;
    coordinator.setMuted(true); // no-op
    assert(!sessionChanged);
    coordinator.setMuted(false);

    // Quality preference is canonicalized and can persist independently from
    // the active player lifecycle.
    assert(coordinator.preferredQuality() == QStringLiteral("auto"));
    coordinator.setPreferredQuality(QStringLiteral("720P"));
    assert(coordinator.preferredQuality() == QStringLiteral("720p"));
    coordinator.setPreferredQuality(QStringLiteral("540P"));
    assert(coordinator.preferredQuality() == QStringLiteral("540p"));
    coordinator.setPreferredQuality(QStringLiteral("not-a-quality"));
    assert(coordinator.preferredQuality() == QStringLiteral("auto"));

    QTemporaryDir settingsDirectory{};
    assert(settingsDirectory.isValid());
    const QString settingsPath = settingsDirectory.filePath(QStringLiteral("media.ini"));
    {
        QSettings settings{settingsPath, QSettings::IniFormat};
        MediaPlaybackCoordinator persistent{settings};
        persistent.setPreferredQuality(QStringLiteral("1080p"));
        settings.sync();
        assert(settings.status() == QSettings::NoError);
    }
    QSettings restoredSettings{settingsPath, QSettings::IniFormat};
    MediaPlaybackCoordinator restored{restoredSettings};
    assert(restored.preferredQuality() == QStringLiteral("1080p"));

    // A suspended (paused) player relinquishes only via release(); the
    // coordinator keeps the session while a player remains the primary, so a
    // paused-but-active session is not replaced by silence during scrolling.
    assert(coordinator.claimPrimary(&second));
    coordinator.release(&second);
    assert(coordinator.activePlayer() == nullptr);

    return 0;
}

#include "media_playback_coordinator_tests.moc"
