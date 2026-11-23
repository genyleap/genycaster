#include "stats_share_card_renderer.hpp"

#include <QBrush>
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

constexpr int CanvasWidth = 960;
constexpr int CanvasHeight = 600;

struct Palette final {
    QColor foreground;
    QColor muted;
    QColor accent;
    QColor panel;
    QColor tile;
    QColor tileBorder;
    QColor panelBorder;
};

[[nodiscard]] Palette colorsFor(const bool minimal)
{
    return {
        minimal ? QColor{"#151B2D"} : QColor{"#F8FAFF"},
        minimal ? QColor{"#536179"} : QColor{"#AEB9DA"},
        minimal ? QColor{"#594CE8"} : QColor{"#A88DFF"},
        minimal ? QColor{"#F4F4FF"} : QColor{"#222246"},
        minimal ? QColor{"#F0F3FB"} : QColor{"#182342"},
        minimal ? QColor{"#DDE3F3"} : QColor{"#344065"},
        minimal ? QColor{"#DCDCFB"} : QColor{"#56518F"},
    };
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
           const int pixelSize, const QColor& ink,
           const QFont::Weight weight = QFont::Normal,
           const Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    QFont font = painter.font();
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    painter.setFont(font);
    painter.setPen(ink);
    const QString safeText = QFontMetrics{font}.elidedText(
        text, Qt::ElideRight, std::max(0, static_cast<int>(rect.width())));
    painter.drawText(rect, alignment, safeText);
}

void roundedPanel(QPainter& painter, const QRectF& rect,
                  const qreal radius, const QColor& background,
                  const QColor& outline = {})
{
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter.fillPath(path, background);
    if (outline.isValid()) {
        painter.setPen(QPen{outline, 1.5});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        painter.setPen(Qt::NoPen);
    }
}

// Mirrors the actual vector mark in ui/wasm/genycaster-icon.svg.
// Vector paths avoid relying on the Qt SVG image plugin at raster time.
void brandMark(QPainter& painter, const QRectF& rect)
{
    painter.save();
    painter.translate(rect.x(), rect.y());
    painter.scale(rect.width() / 416.0, rect.height() / 416.0);
    painter.translate(-48, -48);
    painter.setPen(Qt::NoPen);

    QPainterPath square;
    square.addRoundedRect(QRectF{56, 56, 400, 400}, 102, 102);
    painter.fillPath(square, QColor{"#101010"});

    QPainterPath letter;
    letter.moveTo(366, 204);
    letter.cubicTo(342, 157, 298, 128, 249, 128);
    letter.cubicTo(178.30, 128, 121, 185.30, 121, 256);
    letter.cubicTo(121, 326.70, 178.30, 384, 249, 384);
    letter.cubicTo(294, 384, 331.8, 365.4, 354.3, 349.5);
    letter.lineTo(322, 315.2);
    letter.cubicTo(303.6, 330.2, 281.4, 336, 249, 336);
    letter.cubicTo(204.82, 336, 169, 300.18, 169, 256);
    letter.cubicTo(169, 211.82, 204.82, 176, 249, 176);
    letter.cubicTo(278, 176, 299.5, 185, 315.5, 204);
    letter.closeSubpath();
    painter.fillPath(letter, Qt::white);

    QPainterPath arrow;
    arrow.moveTo(254, 231);
    arrow.lineTo(376, 231);
    arrow.cubicTo(382.08, 231, 385, 233.92, 385, 240);
    arrow.lineTo(385, 362);
    arrow.cubicTo(385, 367.7, 383.1, 367.25, 379.4, 363.3);
    arrow.cubicTo(341.2, 321.9, 296.6, 280.8, 252.2, 245.8);
    arrow.cubicTo(245.6, 240.8, 247.0, 231, 254, 231);
    arrow.closeSubpath();
    painter.fillPath(arrow, QColor{"#BEBEBE"});
    painter.restore();
}

void metricTile(QPainter& painter, const Palette& palette, const QRectF& rect,
                const QString& value, const QString& caption)
{
    roundedPanel(painter, rect, 19, palette.tile, palette.tileBorder);
    const bool longNumber = value.size() >= 8;
    const int numberSize = longNumber ? 25 : value.size() >= 6 ? 29 : 37;
    const QFont font = painter.font();
    QFont metricFont = font;
    metricFont.setPixelSize(numberSize);
    metricFont.setWeight(QFont::Bold);
    QFont captionFont = font;
    captionFont.setPixelSize(23);
    const qreal valueWidth = QFontMetricsF{metricFont}.horizontalAdvance(value);
    const qreal captionWidth = QFontMetricsF{captionFont}.horizontalAdvance(caption);
    constexpr qreal gap = 12.0;
    const qreal available = rect.width() - 24.0;
    if (valueWidth + captionWidth + gap <= available) {
        const qreal x = rect.x() + (rect.width() - valueWidth - captionWidth - gap) / 2.0;
        label(painter, value, QRectF{x, rect.y(), valueWidth + 1, rect.height()},
              numberSize, palette.foreground, QFont::Bold);
        label(painter, caption,
              QRectF{x + valueWidth + gap, rect.y(), captionWidth + 1, rect.height()},
              23, palette.muted);
    } else {
        // Long values or localized names: never overflow the card edges.
        label(painter, value, QRectF{rect.x() + 12, rect.y() + 2, rect.width() - 24, 36},
              numberSize, palette.foreground, QFont::Bold, Qt::AlignCenter);
        label(painter, caption,
              QRectF{rect.x() + 12, rect.y() + 39, rect.width() - 24, 27},
              19, palette.muted, QFont::Normal, Qt::AlignCenter);
    }
}

void drawImage(QImage& image, const QVariantMap& stats, const QString& displayName,
               const QString& username, const int periodDays,
               const bool minimal, const qint64 capturedAtMs)
{
    const auto palette = colorsFor(minimal);
    QPainter painter{&image};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setPen(Qt::NoPen);

    const QRectF fullCard{0, 0, CanvasWidth, CanvasHeight};
    QPainterPath clip;
    clip.addRoundedRect(fullCard.adjusted(2, 2, -2, -2), 38, 38);
    painter.setClipPath(clip);

    QLinearGradient background{QPointF{0, 0}, QPointF{CanvasWidth, CanvasHeight}};
    background.setColorAt(0, minimal ? QColor{"#FFFFFF"} : QColor{"#0C142D"});
    background.setColorAt(1, minimal ? QColor{"#E9EEFF"} : QColor{"#18102F"});
    painter.fillRect(fullCard, background);

    QRadialGradient violet{QPointF{876, 76}, 260};
    violet.setColorAt(0, minimal ? QColor{165, 100, 255, 24} : QColor{165, 100, 255, 64});
    violet.setColorAt(1, Qt::transparent);
    painter.fillRect(fullCard, violet);
    QRadialGradient cyan{QPointF{90, 545}, 245};
    cyan.setColorAt(0, minimal ? QColor{40, 204, 255, 18} : QColor{40, 204, 255, 56});
    cyan.setColorAt(1, Qt::transparent);
    painter.fillRect(fullCard, cyan);

    painter.setClipping(false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen{minimal ? QColor{"#CDD6F6"} : QColor{"#5853AD"}, 2});
    painter.drawRoundedRect(fullCard.adjusted(2, 2, -2, -2), 38, 38);
    painter.setPen(Qt::NoPen);

    brandMark(painter, QRectF{46, 36, 60, 60});
    label(painter, displayName.isEmpty() ? QStringLiteral("Genycaster member")
                                         : displayName,
          QRectF{123, 40, 490, 33}, 25, palette.foreground, QFont::DemiBold);
    label(painter, username.isEmpty() ? QStringLiteral("Genycaster Stats")
          : (username.startsWith(u'@') ? username : QStringLiteral("@") + username),
          QRectF{123, 73, 400, 24}, 19, palette.muted);
    label(painter, QStringLiteral("GENYCASTER  /  STATS"),
          QRectF{685, 54, 230, 29}, 16, palette.accent, QFont::Bold, Qt::AlignRight);

    const QString period = periodDays == 7 ? QStringLiteral("7 days")
        : periodDays == 30 ? QStringLiteral("30 days")
        : periodDays == 90 ? QStringLiteral("90 days") : QStringLiteral("365 days");
    label(painter, period + QStringLiteral(" Recap"), QRectF{46, 117, 505, 54},
          40, palette.foreground, QFont::Bold);

    roundedPanel(painter, QRectF{535, 130, 125, 33}, 17,
                 minimal ? QColor{"#E5E4FF"} : QColor{"#302350"});
    label(painter, QStringLiteral("ACTIVITY"), QRectF{535, 131, 125, 31},
          15, palette.accent, QFont::Bold, Qt::AlignCenter);

    const QRectF hero{46, 187, 868, 166};
    roundedPanel(painter, hero, 28, palette.panel, palette.panelBorder);
    const bool hasEngagement = stats.value(QStringLiteral("hasEngagement")).toBool();
    const QString engagement = hasEngagement
        ? formattedCount(stats, "engagement") : QStringLiteral("—");
    const int engagementSize = engagement.size() > 8 ? 55
        : engagement.size() > 6 ? 68 : 92;
    label(painter, engagement, QRectF{69, 212, 270, 116},
          engagementSize, palette.foreground, QFont::Bold);
    painter.fillRect(QRectF{361, 224, 2, 88},
                     minimal ? QColor{"#D0D3EB"} : QColor{"#585783"});
    label(painter, QStringLiteral("Interactions"), QRectF{398, 228, 320, 47},
          34, palette.foreground, QFont::DemiBold);
    label(painter, QStringLiteral("received"), QRectF{398, 276, 290, 43},
          26, palette.muted);

    const QString casts = formattedCount(stats, "casts");
    const QString replies = formattedCount(stats, "replies");
    metricTile(painter, palette, QRectF{46, 369, 428, 70}, casts,
               QStringLiteral("Casts"));
    metricTile(painter, palette, QRectF{487, 369, 427, 70}, replies,
               QStringLiteral("Replies written"));

    const QString likes = hasEngagement
        ? formattedCount(stats, "likes") : QStringLiteral("—");
    const QString recasts = hasEngagement
        ? formattedCount(stats, "recasts") : QStringLiteral("—");
    const QString receivedReplies = hasEngagement
        ? formattedCount(stats, "receivedReplies") : QStringLiteral("—");
    metricTile(painter, palette, QRectF{46, 451, 279, 70}, likes,
               QStringLiteral("Likes"));
    metricTile(painter, palette, QRectF{338, 451, 279, 70}, recasts,
               QStringLiteral("Recasts"));
    metricTile(painter, palette, QRectF{630, 451, 284, 70}, receivedReplies,
               QStringLiteral("Replies received"));

    label(painter,
          QStringLiteral("Posts from this period · engagement counted to date"),
          QRectF{46, 534, 680, 34}, 16, palette.muted);
    if (capturedAtMs > 0) {
        const auto time = QDateTime::fromMSecsSinceEpoch(capturedAtMs);
        label(painter, QLocale{QLocale::English}.toString(time.date(),
              QStringLiteral("MMM d, yyyy")), QRectF{740, 534, 175, 34},
              16, palette.muted, QFont::Normal, Qt::AlignRight);
    }
}

} // namespace

StatsShareImage renderStatsShareCard(
    const QVariantMap& snapshot, const QString& displayName,
    const QString& username, const int periodDays,
    const QString& style, const double capturedAtMs)
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
    drawImage(output, snapshot, displayName.left(120),
              username.left(120), periodDays,
              style == QStringLiteral("minimal"),
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
