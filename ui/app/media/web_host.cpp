#include "web_host.hpp"

#include <QDesktopServices>
#include <QTimer>
#include <QUrlQuery>

#include <utility>

namespace {
[[nodiscard]] bool approvedHttpsUrl(const QUrl& url)
{
    const QString host = url.host().trimmed().toLower();
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.userInfo().isEmpty() && !host.isEmpty() && host.contains(QLatin1Char('.'))
        && url.port(443) == 443;
}
}

MediaWebHostController::MediaWebHostController(
    const bool available, QUrl clientOrigin, QObject* const parent)
    : QObject{parent}, m_available{available}, m_clientOrigin{std::move(clientOrigin)}
{
    if (!m_clientOrigin.isValid()
        || (m_clientOrigin.scheme() != QStringLiteral("https")
            && m_clientOrigin.scheme() != QStringLiteral("http"))
        || m_clientOrigin.userInfo().size() > 0 || m_clientOrigin.host().isEmpty()) {
        m_clientOrigin = QUrl{};
    } else {
        m_clientOrigin.setPath(QStringLiteral("/"));
        m_clientOrigin.setQuery(QString{});
        m_clientOrigin.setFragment(QString{});
    }
}

MediaWebHostController::~MediaWebHostController() = default;

void MediaWebHostController::setReduceMotion(const bool reduceMotion) noexcept
{
    m_reduceMotion = reduceMotion;
}

QString MediaWebHostController::youtubeVideoId(const QUrl& url)
{
    const QString host = url.host().toLower();
    const QString path = url.path();
    if (host == QStringLiteral("youtu.be")) {
        return path.section(QLatin1Char('/'), 1, 1).trimmed();
    }
    if (host == QStringLiteral("youtube.com") || host == QStringLiteral("www.youtube.com")
        || host == QStringLiteral("m.youtube.com") || host == QStringLiteral("music.youtube.com")
        || host == QStringLiteral("youtube-nocookie.com") || host == QStringLiteral("www.youtube-nocookie.com")) {
        if (path == QStringLiteral("/watch")) {
            return QUrlQuery{url}.queryItemValue(QStringLiteral("v")).trimmed();
        }
        if (path.startsWith(QStringLiteral("/shorts/")) || path.startsWith(QStringLiteral("/embed/"))
            || path.startsWith(QStringLiteral("/live/"))) {
            return path.section(QLatin1Char('/'), 2, 2).trimmed();
        }
    }
    return {};
}

QUrl MediaWebHostController::canonicalMediaUrl(const QString& rawUrl) const
{
    const QUrl parsed{rawUrl.trimmed()};
    if (!approvedHttpsUrl(parsed)) {
        return {};
    }

    const QString videoId = youtubeVideoId(parsed);
    if (videoId.isEmpty()) {
        return {};
    }

    const QString encodedVideoId = QString::fromLatin1(QUrl::toPercentEncoding(videoId));
    QUrl embed{QStringLiteral("https://www.youtube.com/embed/") + encodedVideoId};
    QUrlQuery query{};
    query.addQueryItem(QStringLiteral("playsinline"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("rel"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("modestbranding"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("autoplay"), QStringLiteral("0"));
    if (m_clientOrigin.isValid()) {
        QUrl origin = m_clientOrigin;
        origin.setPath(QString{});
        query.addQueryItem(QStringLiteral("origin"), origin.toString(QUrl::FullyEncoded));
        query.addQueryItem(
            QStringLiteral("widget_referrer"), m_clientOrigin.toString(QUrl::FullyEncoded));
    }
    embed.setQuery(query);
    return embed;
}

bool MediaWebHostController::open(const QString& rawUrl, const QString& title)
{
    if (!m_available) {
        return false;
    }
    const QUrl url = canonicalMediaUrl(rawUrl);
    if (!url.isValid()) {
        return false;
    }
    openBrowser(url, title.trimmed());
    return m_browser != nullptr;
}

void MediaWebHostController::openBrowser(const QUrl& url, const QString& title)
{
#if GENYCASTER_HAS_MINIAPP_HOST
    m_browser.reset();
    m_currentUrl = url;
    m_currentTitle = title.isEmpty() ? url.host() : title;

    using farcaster::platform::miniapp::BrowserCallbacks;
    using farcaster::platform::miniapp::BrowserRole;
    using farcaster::platform::miniapp::PresentationMode;

    BrowserCallbacks callbacks{};
    callbacks.closed = [this] {
        QTimer::singleShot(0, this, [this] {
            m_browser.reset();
            m_currentUrl = QUrl{};
            m_currentTitle.clear();
        });
    };
    callbacks.retry = [this] {
        QTimer::singleShot(0, this, [this] {
            if (m_browser && m_currentUrl.isValid()) {
                m_browser->status(tr("Loading media…"), false);
                m_browser->load(m_currentUrl, m_clientOrigin);
            }
        });
    };
    callbacks.share = [this] {
        if (m_currentUrl.isValid()) {
            QDesktopServices::openUrl(m_currentUrl);
        }
    };
    callbacks.navigationStarted = [this] {
        if (m_browser) {
            m_browser->status(tr("Loading media…"), false);
        }
    };
    callbacks.failed = [this](const QString& error) {
        if (m_browser) {
            m_browser->status(error.isEmpty() ? tr("This media could not be loaded in Genycaster.") : error, true);
        }
    };

    m_browser = farcaster::platform::miniapp::createBrowser(
        m_currentTitle,
        url,
        std::move(callbacks),
        PresentationMode::Window,
        m_reduceMotion,
        true,
        BrowserRole::Media);
    if (m_browser) {
        m_browser->status(tr("Loading media…"), false);
        m_browser->load(url, m_clientOrigin);
    }
#else
    Q_UNUSED(url)
    Q_UNUSED(title)
#endif
}

void MediaWebHostController::close()
{
    m_browser.reset();
    m_currentUrl = QUrl{};
    m_currentTitle.clear();
}
