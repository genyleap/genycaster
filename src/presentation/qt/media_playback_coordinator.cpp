#include "media_playback_coordinator.hpp"

#include <QMetaMethod>
#include <QMetaObject>
#include <QMediaPlayer>
#include <QSettings>
#include <QUrl>

#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QPlaybackOptions>

#include <chrono>
#endif


namespace {

constexpr auto PreferredQualityKey = "media/preferredQuality";

[[nodiscard]] QString normalizedPreferredQuality(QString value)
{
    value = value.trimmed().toLower();
    if (value == QLatin1StringView{"auto"} || value == QLatin1StringView{"original"}) {
        return value;
    }
    if (!value.endsWith(QLatin1Char{'p'})) {
        return QStringLiteral("auto");
    }
    bool parsed{};
    const int height = value.chopped(1).toInt(&parsed);
    if (!parsed || height < 144 || height > 8640) {
        return QStringLiteral("auto");
    }
    return QString::number(height) + QLatin1Char{'p'};
}

} // namespace

MediaPlaybackCoordinator::MediaPlaybackCoordinator(QObject* const parent)
    : QObject(parent)
{
}

MediaPlaybackCoordinator::MediaPlaybackCoordinator(QSettings& settings, QObject* const parent)
    : QObject(parent),
      m_settings(&settings),
      m_preferredQuality(normalizedPreferredQuality(
          settings.value(QString::fromLatin1(PreferredQualityKey), QStringLiteral("auto")).toString()))
{
}

bool MediaPlaybackCoordinator::muted() const noexcept
{
    return m_muted;
}

void MediaPlaybackCoordinator::setMuted(const bool muted)
{
    if (m_muted == muted) {
        return;
    }
    m_muted = muted;
    emit sessionChanged();
}

QString MediaPlaybackCoordinator::preferredQuality() const
{
    return m_preferredQuality;
}

void MediaPlaybackCoordinator::setPreferredQuality(const QString& preferredQuality)
{
    const QString normalized = normalizedPreferredQuality(preferredQuality);
    if (normalized == m_preferredQuality) {
        return;
    }
    m_preferredQuality = normalized;
    if (m_settings != nullptr) {
        m_settings->setValue(QString::fromLatin1(PreferredQualityKey), m_preferredQuality);
    }
    emit sessionChanged();
}

QObject* MediaPlaybackCoordinator::activePlayer() const noexcept
{
    return m_activePlayer.data();
}

void MediaPlaybackCoordinator::suspendPlayer(QObject* const player)
{
    if (player == nullptr) {
        return;
    }
    const QMetaObject* const meta = player->metaObject();
    const int index = meta == nullptr
        ? -1 : meta->indexOfMethod("suspendFromPlaybackCoordinator()");
    if (index < 0) {
        return;
    }
    QMetaMethod method = meta->method(index);
    method.invoke(player, Qt::DirectConnection);
}

bool MediaPlaybackCoordinator::claimPrimary(QObject* const player)
{
    if (player == nullptr) {
        return false;
    }
    if (m_activePlayer == player) {
        return true;
    }
    if (m_activePlayer != nullptr) {
        suspendPlayer(m_activePlayer);
    }
    m_activePlayer = player;
    emit activePlayerChanged();
    return true;
}

void MediaPlaybackCoordinator::release(QObject* const player)
{
    if (player == nullptr || m_activePlayer != player) {
        return;
    }
    m_activePlayer = nullptr;
    emit activePlayerChanged();
}

bool MediaPlaybackCoordinator::isPrimary(QObject* const player) const
{
    return player != nullptr && m_activePlayer == player;
}

bool MediaPlaybackCoordinator::prepareForSource(
    QObject* const playerObject, const QString& source) const
{
    auto* const player = qobject_cast<QMediaPlayer*>(playerObject);
    if (player == nullptr) {
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    QPlaybackOptions options = player->playbackOptions();
    const QUrl url{source};
    const QString scheme = url.scheme().toLower();
    const bool remote = scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
    const QString path = url.path().toLower();
    const bool adaptiveHls = path.endsWith(QStringLiteral(".m3u8"));

    if (remote) {
        // Bounded socket waits turn dead CDN connections into a recoverable
        // player error instead of a long indeterminate startup. This is an
        // FFmpeg-backend hint and is ignored by unsupported backends.
        options.setNetworkTimeout(std::chrono::milliseconds{8'000});
    } else {
        options.resetNetworkTimeout();
    }

    if (adaptiveHls) {
        // Farcaster/Cloudflare VOD manifests expose stream metadata early. A
        // bounded probe avoids FFmpeg's larger generic probe delaying first
        // frame acquisition while retaining enough data for ordinary HLS.
        options.setProbeSize(512 * 1024);
    } else {
        options.resetProbeSize();
    }
    options.setPlaybackIntent(QPlaybackOptions::PlaybackIntent::Playback);
    player->setPlaybackOptions(options);
#else
    Q_UNUSED(source);
#endif
    return true;
}

qulonglong MediaPlaybackCoordinator::prepareSource(const QString& source)
{
    const qulonglong requestId = m_nextSourcePreparationId++;
    if (m_nextSourcePreparationId == 0) {
        m_nextSourcePreparationId = 1;
    }

    const QPointer<MediaPlaybackCoordinator> guard{this};
    QMetaObject::invokeMethod(
        this,
        [guard, requestId, source] {
            if (!guard) {
                return;
            }
            const QUrl url{source};
            const QString scheme = url.scheme().toLower();
            const bool remote = scheme == QStringLiteral("http")
                || scheme == QStringLiteral("https");
            if (source.trimmed().isEmpty() || (remote && (!url.isValid() || url.host().isEmpty()))) {
                emit guard->sourcePreparationFailed(requestId, source, QString{});
                return;
            }
            if (remote) {
                if (guard->m_sourcePreparationBackend.start) {
                    guard->m_sourcePreparationBackend.start(requestId, source);
                } else {
                    emit guard->sourcePreparationFailed(requestId, source, QString{});
                }
                return;
            }
            emit guard->sourcePrepared(requestId, source);
        },
        Qt::QueuedConnection);
    return requestId;
}

void MediaPlaybackCoordinator::setSourcePreparationBackend(SourcePreparationBackend backend)
{
    m_sourcePreparationBackend = std::move(backend);
}

void MediaPlaybackCoordinator::completeSourcePreparation(
    const qulonglong requestId,
    QString source,
    const bool available,
    QString errorText)
{
    if (requestId == 0) {
        return;
    }
    if (available) {
        emit sourcePrepared(requestId, source);
        return;
    }
    emit sourcePreparationFailed(requestId, source, errorText);
}
