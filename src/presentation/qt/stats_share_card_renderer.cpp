#include "stats_share_card_renderer.hpp"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QRectF>
#include <QTemporaryFile>

#include <algorithm>
#include <cmath>

namespace genycaster::presentation {
namespace {

constexpr int LayoutWidth = 960;
constexpr int LayoutHeight = 540;
// The preview is small, but the published PNG has 2x raster resolution.
constexpr int RasterScale = 2;
constexpr int CanvasWidth = LayoutWidth * RasterScale;
constexpr int CanvasHeight = LayoutHeight * RasterScale;

struct Palette final {
    QColor text;
    QColor secondary;
    QColor subtle;
    QColor accent;
    QColor rule;
};

[[nodiscard]] Palette colorsFor(bool minimal)
{
    return minimal
        ? Palette{QColor{"#182134"}, QColor{"#58647B"}, QColor{"#8490A5"},
                  QColor{"#6151D4"}, QColor{"#DDE2EC"}}
        : Palette{QColor{"#F8FAFF"}, QColor{"#B1BCDB"}, QColor{"#8A9BBC"},
                  QColor{"#C1ADFF"}, QColor{"#333B61"}};
}

[[nodiscard]] QString formattedCount(const QVariantMap& snapshot, const char* key)
{
    bool valid = false;
    const double value = snapshot.value(QString::fromLatin1(key)).toDouble(&valid);
    if (!valid || !std::isfinite(value) || value < 0.0) return QStringLiteral("—");
    constexpr double Maximum = 999'999'999'999.0;
    return QLocale{QLocale::English}.toString(
        static_cast<qulonglong>(std::floor(std::min(value, Maximum))));
}

void label(QPainter& painter, const QString& text, const QRectF& rect,
           int pixelSize, const QColor& color,
           QFont::Weight weight = QFont::Normal,
           Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    QFont font = painter.font();
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    painter.setFont(font);
    painter.setPen(color);
    const QString safe = QFontMetrics{font}.elidedText(
        text, Qt::ElideRight, std::max(0, static_cast<int>(rect.width())));
    painter.drawText(rect, alignment, safe);
}

void horizontalRule(QPainter& painter, const Palette& ink, qreal y)
{
    painter.fillRect(QRectF{44, y, 872, 1}, ink.rule);
}

void drawAvatar(QPainter& painter, const QImage& avatar, const QString& displayName,
                const Palette& colors, bool minimal)
{
    constexpr QRectF avatarRect{45, 36, 66, 66};
    QPainterPath silhouette;
    silhouette.addEllipse(avatarRect);
    painter.save();
    painter.setClipPath(silhouette);
    painter.fillPath(silhouette, minimal ? QColor{"#E5E6F6"} : QColor{"#302950"});
    if (!avatar.isNull() && avatar.width() > 0 && avatar.height() > 0) {
        // Centre-crop the actual account portrait. Never draw a generic logo
        // in an identity slot; if unavailable use the owner's monogram.
        const qreal ratio = std::max(avatarRect.width() / avatar.width(),
                                     avatarRect.height() / avatar.height());
        const qreal sourceWidth = avatarRect.width() / ratio;
        const qreal sourceHeight = avatarRect.height() / ratio;
        const QRectF source{(avatar.width() - sourceWidth) / 2,
                            (avatar.height() - sourceHeight) / 2,
                            sourceWidth, sourceHeight};
        painter.drawImage(avatarRect, avatar, source);
    } else {
        const QString initial = displayName.trimmed().isEmpty()
            ? QStringLiteral("·") : displayName.trimmed().left(1).toUpper();
        label(painter, initial, avatarRect, 32, colors.text,
              QFont::DemiBold, Qt::AlignCenter);
    }
    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen{colors.rule, 1.5});
    painter.drawEllipse(avatarRect);
}

void metric(QPainter& painter, const Palette& palette,
            qreal left, qreal width, const QString& value, const QString& title)
{
    const int valueSize = value.size() >= 9 ? 26 : value.size() >= 7 ? 30 : 39;
    label(painter, value, QRectF{left + 4, 355, width - 8, 53},
          valueSize, palette.text, QFont::DemiBold, Qt::AlignCenter);
    label(painter, title, QRectF{left + 3, 410, width - 6, 30},
          18, palette.secondary, QFont::Normal, Qt::AlignCenter);
}

void drawImage(QImage& image, const QVariantMap& stats, const QString& displayName,
               const QString& username, const QImage& avatar, int periodDays,
               bool minimal, qint64 capturedAtMs)
{
    const Palette palette = colorsFor(minimal);
    QPainter painter{&image};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.scale(RasterScale, RasterScale);
    painter.setPen(Qt::NoPen);

    const QRectF card{0, 0, LayoutWidth, LayoutHeight};
    QPainterPath clip;
    clip.addRoundedRect(card.adjusted(2, 2, -2, -2), 32, 32);
    painter.setClipPath(clip);

    // Two quiet, coordinated surfaces; Neon has ambience, not separate
    // glowing boxes around every number.
    QLinearGradient backdrop{QPointF{0, 0}, QPointF{960, 540}};
    backdrop.setColorAt(0, minimal ? QColor{"#FDFDFF"} : QColor{"#10172E"});
    backdrop.setColorAt(1, minimal ? QColor{"#F2F4F9"} : QColor{"#171730"});
    painter.fillRect(card, backdrop);
    if (!minimal) {
        QRadialGradient ambient{QPointF{907, 65}, 440};
        ambient.setColorAt(0, QColor{132, 85, 212, 37});
        ambient.setColorAt(1, Qt::transparent);
        painter.fillRect(card, ambient);
    }
    painter.setClipping(false);
    painter.setPen(QPen{minimal ? QColor{"#DBE0EC"} : QColor{"#554B90"}, 1.5});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(card.adjusted(2, 2, -2, -2), 32, 32);

    drawAvatar(painter, avatar, displayName, palette, minimal);
    label(painter, displayName.trimmed().isEmpty() ? QStringLiteral("Member") : displayName,
          QRectF{129, 39, 565, 35}, 27, palette.text, QFont::DemiBold);
    label(painter, username.isEmpty() ? QStringLiteral("Genycaster") :
          (username.startsWith(u'@') ? username : QStringLiteral("@") + username),
          QRectF{129, 76, 490, 25}, 18, palette.secondary);
    label(painter, QStringLiteral("GENYCASTER  /  STATS"),
          QRectF{702, 55, 212, 25}, 15, palette.accent,
          QFont::DemiBold, Qt::AlignRight);
    horizontalRule(painter, palette, 125);

    const QString period = periodDays == 7 ? QStringLiteral("7 days")
        : periodDays == 30 ? QStringLiteral("30 days")
        : periodDays == 90 ? QStringLiteral("90 days") : QStringLiteral("365 days");
    label(painter, period + QStringLiteral(" recap"), QRectF{45, 146, 660, 47},
          38, palette.text, QFont::DemiBold);
    label(painter, QStringLiteral("ACTIVITY"), QRectF{781, 155, 133, 31},
          15, palette.accent, QFont::DemiBold, Qt::AlignRight);

    const bool hasEngagement = stats.value(QStringLiteral("hasEngagement")).toBool();
    const QString interactions = hasEngagement
        ? formattedCount(stats, "engagement") : QStringLiteral("—");
    const int heroSize = interactions.size() >= 10 ? 58 :
                         interactions.size() >= 8 ? 70 : 87;
    label(painter, interactions, QRectF{46, 213, 460, 98}, heroSize,
          palette.text, QFont::Bold);
    label(painter, QStringLiteral("Interactions received"),
          QRectF{505, 240, 408, 49}, 29, palette.secondary);
    horizontalRule(painter, palette, 331);

    // A single typographic metric strip replaces five heavy nested tiles.
    // Consistent columns create rhythm at thumbnail and native PNG sizes.
    const QString values[]{
        formattedCount(stats, "casts"),
        formattedCount(stats, "replies"),
        hasEngagement ? formattedCount(stats, "likes") : QStringLiteral("—"),
        hasEngagement ? formattedCount(stats, "recasts") : QStringLiteral("—"),
        hasEngagement ? formattedCount(stats, "receivedReplies") : QStringLiteral("—")
    };
    const QString titles[]{
        QStringLiteral("Casts"), QStringLiteral("Replies written"),
        QStringLiteral("Likes"), QStringLiteral("Recasts"),
        QStringLiteral("Replies received")
    };
    constexpr qreal left = 44;
    constexpr qreal width = 872.0 / 5.0;
    for (int index = 0; index < 5; ++index) {
        metric(painter, palette, left + width * index, width, values[index], titles[index]);
        if (index > 0) {
            painter.fillRect(QRectF{left + width * index, 368, 1, 56}, palette.rule);
        }
    }
    horizontalRule(painter, palette, 468);

    label(painter, QStringLiteral("Posts in period · engagement counted to date"),
          QRectF{45, 485, 690, 29}, 16, palette.subtle);
    if (capturedAtMs > 0) {
        const auto date = QDateTime::fromMSecsSinceEpoch(capturedAtMs).date();
        label(painter, QLocale{QLocale::English}.toString(
                  date, QStringLiteral("MMM d, yyyy")),
              QRectF{737, 485, 177, 29}, 16, palette.subtle,
              QFont::Normal, Qt::AlignRight);
    }
}

} // namespace

StatsShareImage renderStatsShareCard(
    const QVariantMap& snapshot, const QString& displayName,
    const QString& username, const QImage& avatar, int periodDays,
    const QString& style, double capturedAtMs)
{
    if ((style != QStringLiteral("neon") && style != QStringLiteral("minimal"))
        || (periodDays != 7 && periodDays != 30 && periodDays != 90
            && periodDays != 365)
        || !std::isfinite(capturedAtMs) || capturedAtMs <= 0.0) {
        return {.error = QStringLiteral("invalid_share_card_parameters")};
    }
    QImage output{CanvasWidth, CanvasHeight, QImage::Format_ARGB32_Premultiplied};
    if (output.isNull())
        return {.error = QStringLiteral("share_card_image_unavailable")};
    output.fill(Qt::transparent);
    drawImage(output, snapshot, displayName.left(120), username.left(120),
              avatar, periodDays, style == QStringLiteral("minimal"),
              static_cast<qint64>(capturedAtMs));

    const QString directory = QDir::tempPath();
    if (directory.isEmpty() || !QDir{}.mkpath(directory))
        return {.error = QStringLiteral("share_card_temp_unavailable")};

    QTemporaryFile temporary{directory + QStringLiteral("/genycaster-stats-XXXXXX.png")};
    if (!temporary.open())
        return {.error = QStringLiteral("share_card_temp_unavailable")};
    if (!output.save(&temporary, "PNG") || temporary.size() <= 0
        || !temporary.flush())
        return {.error = QStringLiteral("share_card_png_failed")};

    const QUrl path = QUrl::fromLocalFile(temporary.fileName());
    temporary.setAutoRemove(false);
    temporary.close();
    return {.fileUrl = path};
}

} // namespace genycaster::presentation
