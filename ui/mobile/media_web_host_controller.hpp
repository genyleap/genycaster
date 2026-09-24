#pragma once

#include "miniapp_browser.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

/**
 * @brief Owns one isolated in-app web surface for rich media playback.
 *
 * The controller deliberately reuses the platform WebKit adapter without the
 * Farcaster Mini App JavaScript bridge. Media storage may persist provider
 * cookies so first-party playback challenges do not restart for every video;
 * Mini App storage remains ephemeral. Calls are GUI-thread only.
 */
class MediaWebHostController final : public QObject {
    Q_OBJECT

public:
    explicit MediaWebHostController(bool available, QUrl clientOrigin, QObject* parent = nullptr);
    ~MediaWebHostController() override;

    /** @brief Updates the host animation preference without recreating active media. */
    void setReduceMotion(bool reduceMotion) noexcept;
    /**
     * @brief Opens a supported YouTube destination in the isolated native host.
     * @return true when the request was accepted; false when no native host is available
     * or the URL cannot be canonicalized to an approved YouTube embed.
     */
    [[nodiscard]] bool open(const QString& rawUrl, const QString& title = {});
    /** @brief Closes the active media host, if any. */
    void close();

private:
    [[nodiscard]] QUrl canonicalMediaUrl(const QString& rawUrl) const;
    [[nodiscard]] static QString youtubeVideoId(const QUrl& url);
    void openBrowser(const QUrl& url, const QString& title);

    bool m_available{false};
    bool m_reduceMotion{false};
    QUrl m_clientOrigin{};
    QUrl m_currentUrl{};
    QString m_currentTitle{};
    std::unique_ptr<farcaster::platform::miniapp::Browser> m_browser{};
};
