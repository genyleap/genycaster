/*!
 * @file        src/presentation/qt/emoji_controller.hpp
 * @brief       Qt emoji presentation controller.
 *
 * @details
 * Exposes emoji assets, playback state and settings to QML while keeping
 * validation and cache orchestration in native service modules.
 *
 * @author      Kambiz Asadzadeh
 * @since       05 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires this QObject declaration in
// a .hpp file. All emoji logic lives in native C++ modules; this adapter only
// owns Qt-facing state, borrowed asset callbacks and QSettings persistence.
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <functional>

class QSettings;

/**
 * @brief QML-facing animated emoji platform adapter.
 *
 * Catalog/search/recents and motion decisions depend only on native application
 * modules. Executable composition supplies provider-neutral asset callbacks;
 * no provider, network or cache implementation is linked into presentation.
 */
class EmojiController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool lottieAvailable READ lottieAvailable CONSTANT)
    Q_PROPERTY(bool spriteAvailable READ spriteAvailable CONSTANT)
    Q_PROPERTY(QString styleId READ styleId CONSTANT)
    Q_PROPERTY(QVariantList emojiGroups READ emojiGroups NOTIFY emojiGroupsChanged)
    Q_PROPERTY(QVariantList specialEmojiGroups READ specialEmojiGroups CONSTANT)
    Q_PROPERTY(QVariantList recents READ recents NOTIFY recentsChanged)
    Q_PROPERTY(QVariantMap diagnostics READ diagnostics NOTIFY diagnosticsChanged)
    Q_PROPERTY(int pendingFetches READ pendingFetches NOTIFY fetchStateChanged)

public:
    /** @brief Provider-neutral Qt callback boundary; installed by executable composition. */
    struct AssetBackend final {
        std::function<QString(const QString&)> source{};
        std::function<void(const QString&)> request{};
        std::function<int()> pending{};
        std::function<QVariantMap(const QString&)> sprite{};
        std::function<bool(const QString&, bool)> requestSprite{};
        std::function<void(const QString&)> releaseSprite{};
        std::function<void(const QString&)> requestCached{};
    };

    /** @brief Replaces borrowed backend callbacks on the GUI thread; clear before backend teardown. */
    void setAssetBackend(AssetBackend backend);

    /** @brief Native motion policy: 0 disables playback, 1 plays once, -1 loops. */
    Q_INVOKABLE int playbackLoops(int surface, bool reducedMotion, bool active, bool visible) const;

    /** @brief Creates the emoji adapter over the borrowed settings store. */
    explicit EmojiController(QSettings& settings, QObject* parent = nullptr);
    ~EmojiController() override;

    EmojiController(const EmojiController&) = delete;
    auto operator=(const EmojiController&) -> EmojiController& = delete;
    EmojiController(EmojiController&&) = delete;
    auto operator=(EmojiController&&) -> EmojiController& = delete;

    /** @brief True when the native emoji platform is composed and usable. */
    [[nodiscard]] bool available() const noexcept;

    /** @brief True when this build supports VectorImage Lottie playback (Qt 6.11+). */
    [[nodiscard]] bool lottieAvailable() const noexcept;

    /** @brief True when the Qt GIF decoder needed for worker-prepared sprites is installed. */
    [[nodiscard]] bool spriteAvailable() const noexcept;

    /** @brief Returns cached local atlas metadata (source, frameSize, frameCount, frameDuration).
     * GUI-thread memory lookup only; an empty map means not prepared yet.
     */
    Q_INVOKABLE QVariantMap spriteFor(const QString& sequence) const;

    /** @brief Requests bounded asynchronous download/decode/cache work.
     * Returns false on backpressure, allowing visible callers to retry later.
     * A true return acquires viewport interest and must be paired with releaseSprite.
     * Completion or recoverable failure is delivered by spriteReady/spriteFailed.
     * retry bypasses a previous failure cooldown for an explicit user retry;
     * admission limits and in-flight deduplication still apply.
     */
    Q_INVOKABLE bool requestSprite(const QString& sequence, bool retry = false);

    /** @brief Active emoji style identifier (future-proof: "noto-animated"). */
    [[nodiscard]] QString styleId() const;

    /** @brief Picker model: localized categories with Unicode values. */
    [[nodiscard]] QVariantList emojiGroups() const;

    /** @brief Manifest-derived animated catalog, kept in a separate picker mode. */
    [[nodiscard]] QVariantList specialEmojiGroups() const;

    /** @brief Recent emoji sequences, most relevant first. */
    [[nodiscard]] QVariantList recents() const;

    /** @brief Renderer availability and pending work for the diagnostics surface. */
    [[nodiscard]] QVariantMap diagnostics() const;

    /** @brief Number of in-flight asset fetches. */
    [[nodiscard]] int pendingFetches() const noexcept;

    /**
     * @brief Renderer source for one sequence: bundled qrc, cached file, or empty.
     *
     * An empty source means the system Unicode glyph must be used.
     */
    Q_INVOKABLE QString assetSourceFor(const QString& sequence) const;

    /** @brief Releases viewport interest and permits queued work to be cancelled. */
    Q_INVOKABLE void releaseSprite(const QString& sequence);

    /** @brief True when a validated animated asset is renderable right now. */
    Q_INVOKABLE bool animatedAvailableFor(const QString& sequence) const;

    /**
     * @brief Requests the animated asset for one sequence.
     *
     * Deduplicates concurrent requests, checks the disk cache off the UI
     * thread, downloads through the shared async transport, validates and
     * stores the asset, then emits assetReady/assetFailed. Never blocks.
     */
    Q_INVOKABLE void requestAsset(const QString& sequence);

    /** @brief Resolves only bundled/disk-cached Lottie off-thread; never downloads on a miss.
     * Emits assetReady on a hit. Intended for one selected preview, not a grid.
     */
    Q_INVOKABLE void requestCachedAsset(const QString& sequence);

    /** @brief Records one emoji usage for the Recent category (Unicode identity). */
    Q_INVOKABLE void recordUsage(const QString& sequence);

    /** @brief Searches the catalog (names, tags, key, unicode). */
    Q_INVOKABLE QVariantList search(const QString& query, int limit = 64) const;

    /** @brief Searches the manifest-derived animated catalog. */
    Q_INVOKABLE QVariantList searchSpecial(const QString& query, int limit = 64) const;

    /** @brief Catalog metadata for one sequence (name/category for a11y). */
    Q_INVOKABLE QVariantMap entryFor(const QString& sequence) const;

    /** @brief Batch source lookup for many sequences (renderer overlay). */
    Q_INVOKABLE QVariantMap sourcesForSequences(const QVariantList& sequences) const;

    /** @brief Clears persisted recent emoji. */
    Q_INVOKABLE void clearRecents();

signals:
    /** @brief Prepared, validated local atlas metadata; receivers own a value copy. */
    void spriteReady(const QString& sequence, const QVariantMap& sprite);

    /** @brief Preparation failed; callers keep the selectable Unicode fallback. */
    void spriteFailed(const QString& sequence, const QString& reason);

    /** @brief Emitted when a validated animated asset becomes available. */
    void assetReady(const QString& sequence, const QString& source);

    /** @brief Emitted when an asset fetch failed and fell back to the glyph. */
    void assetFailed(const QString& sequence);
    void recentsChanged();
    void emojiGroupsChanged();
    void diagnosticsChanged();
    void fetchStateChanged();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
