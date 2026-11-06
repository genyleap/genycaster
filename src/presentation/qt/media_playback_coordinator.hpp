/*!
 * @file        src/presentation/qt/media_playback_coordinator.hpp
 * @brief       Centralized media playback coordination.
 *
 * @details
 * Owns Qt-facing playback focus and activation policy so concurrent media
 * delegates respect application visibility and user intent.
 *
 * @author      Kambiz Asadzadeh
 * @since       05 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QSettings;

/**
 * @brief App-wide coordination of primary media playback sessions.
 *
 * Ensures that only one audible video session is active at a time across
 * recycled feed delegates, detail, profile and fullscreen surfaces. Feed
 * players claim the primary session on explicit play; a new primary suspends
 * the previous one. Session mute/volume preferences are owned here so every
 * Cast surface follows one audio policy instead of conflicting local policies.
 *
 * The coordinator is intentionally small and QML-facing: real decoding stays
 * in the canonical GenyVideoPlayer component; this object only arbitrates
 * which player owns the audible session.
 */
class MediaPlaybackCoordinator final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY sessionChanged)
    Q_PROPERTY(QString preferredQuality READ preferredQuality WRITE setPreferredQuality NOTIFY sessionChanged)

public:
    /**
     * @brief Asynchronous remote-source preparation supplied by composition.
     *
     * start must return immediately. Completion is delivered later through
     * completeSourcePreparation() on the coordinator's thread.
     */
    struct SourcePreparationBackend final {
        std::function<void(qulonglong requestId, const QString& source)> start{};
    };

    /** @brief Creates an idle coordinator (no active session). */
    explicit MediaPlaybackCoordinator(QObject* parent = nullptr);
    /** @brief Creates a coordinator whose playback preferences persist in QSettings. */
    explicit MediaPlaybackCoordinator(QSettings& settings, QObject* parent = nullptr);

    /** @brief Whether new playback sessions start muted (session preference). */
    [[nodiscard]] bool muted() const noexcept;
    /** @brief Sets the session-wide muted preference. */
    void setMuted(bool muted);
    /** @brief Preferred video quality: auto, original, or a canonical height such as 720p. */
    [[nodiscard]] QString preferredQuality() const;
    /** @brief Sets and persists the session-wide video quality preference. */
    void setPreferredQuality(const QString& preferredQuality);
    /** @brief Current primary player, or null when nothing is playing. */
    [[nodiscard]] QObject* activePlayer() const noexcept;

    /**
     * @brief Claims the primary audible session for one player.
     * Any previously active player is suspended through its
     * `suspendFromPlaybackCoordinator()` invokable.
     * @return false when the caller is not a live QObject.
     */
    Q_INVOKABLE bool claimPrimary(QObject* player);

    /**
     * @brief Releases a player from the primary session.
     * Called on explicit pause, source replacement and delegate destruction.
     */
    Q_INVOKABLE void release(QObject* player);

    /** @brief Convenience QML helper returning whether a player owns the session. */
    Q_INVOKABLE bool isPrimary(QObject* player) const;

    /**
     * @brief Applies bounded startup/network hints before assigning a new media source.
     *
     * On Qt 6.10+ the FFmpeg backend receives a bounded network timeout and a
     * reduced probe budget for HLS VOD. Older Qt versions keep their backend
     * defaults. The function performs no network I/O and must be called before
     * QMediaPlayer::setSource(), because playback options apply to the next
     * source assignment.
     *
     * @param player QMediaPlayer instance exposed to QML as QObject.
     * @param source Decoder-facing URL that will be assigned next.
     * @return true when @p player is a QMediaPlayer; false for null/other objects.
     */
    Q_INVOKABLE bool prepareForSource(QObject* player, const QString& source) const;

    /** Use native browser HLS / locally bundled MediaSource fallback on WASM. */
    Q_INVOKABLE bool openBrowserHls(const QString& source) const;

    /**
     * @brief Starts non-blocking preparation of a decoder-facing source.
     *
     * Local sources complete on the next event-loop turn. HTTP(S) sources are
     * delegated to composition so DNS, proxy, TLS and redirects can settle
     * away from the GUI thread before QMediaPlayer receives the URL. A remote
     * source fails recoverably while that backend is unavailable.
     *
     * @return Non-zero request identity used to reject stale completions.
     */
    Q_INVOKABLE qulonglong prepareSource(const QString& source);

    /** @brief Replaces the borrowed asynchronous source-preparation callback. */
    void setSourcePreparationBackend(SourcePreparationBackend backend);

    /**
     * @brief Completes one source-preparation request on the coordinator thread.
     *
     * available selects sourcePrepared versus sourcePreparationFailed. The
     * caller retains no ownership after this function returns.
     */
    void completeSourcePreparation(
        qulonglong requestId,
        QString source,
        bool available,
        QString errorText = {});

signals:
    /** @brief Emitted when a session-wide playback preference changes. */
    void sessionChanged();
    /** @brief Emitted when the primary player changes. */
    void activePlayerChanged();
    /** @brief Emitted after a source is safe to assign to QMediaPlayer. */
    void sourcePrepared(qulonglong requestId, const QString& source);
    /** @brief Emitted when remote preparation failed before decoder startup. */
    void sourcePreparationFailed(
        qulonglong requestId,
        const QString& source,
        const QString& errorText);

private:
    void suspendPlayer(QObject* player);

    QPointer<QObject> m_activePlayer{};
    QSettings* m_settings{};
    bool m_muted{false};
    QString m_preferredQuality{QStringLiteral("auto")};
    SourcePreparationBackend m_sourcePreparationBackend{};
    qulonglong m_nextSourcePreparationId{1};
};
