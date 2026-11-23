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
constexpr int LayoutHeight = 960;
constexpr int RasterScale = 2;
constexpr int CanvasWidth = LayoutWidth * RasterScale;
constexpr int CanvasHeight = LayoutHeight * RasterScale;

struct Palette final {
    QColor primary;
    QColor secondary;
    QColor subtle;
    QColor accent;
    QColor rule;
};

[[nodiscard]] Palette colorsFor(const bool minimal)
{
    return minimal
        ? Palette{QColor{"#182236"}, QColor{"#53627A"}, QColor{"#8690A1"},
                  QColor{"#6552D7"}, QColor{"#DBE2ED"}}
        : Palette{QColor{"#F8F9FF"}, QColor{"#B8C3E4"}, QColor{"#8895B7"},
                  QColor{"#C6B3FF"}, QColor{"#394163"}};
}

[[nodiscard]] QString formattedCount(const QVariantMap& stats, const char* key)
{
    bool valid = false;
    const double value = stats.value(QString::fromLatin1(key)).toDouble(&valid);
    if (!valid || !std::isfinite(value) || value < 0.0) return QStringLiteral("—");
    return QLocale{QLocale::English}.toString(
        static_cast<qulonglong>(std::floor(std::min(value, 999'999'999'999.0))));
}

void label(QPainter& painter, const QString& value, const QRectF& rect,
           const int pixelSize, const QColor& color,
           const QFont::Weight weight = QFont::Normal,
           const Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    QFont font = painter.font();
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, static_cast<int>(alignment), QFontMetrics{font}.elidedText(
        value, Qt::ElideRight, std::max(0, static_cast<int>(rect.width()))));
}

void rule(QPainter& painter, const Palette& palette, const qreal y)
{
    painter.fillRect(QRectF{56, y, 848, 1}, palette.rule);
}

void avatar(QPainter& painter, const QImage& source,
            const QString& displayName, const Palette& palette,
            const bool minimal)
{
    const QRectF destination{57, 49, 83, 83};
    QPainterPath circle;
    circle.addEllipse(destination);
    painter.save();
    painter.setClipPath(circle);
    painter.fillPath(circle, minimal ? QColor{"#E8E8F5"} : QColor{"#2B2B50"});
    if (!source.isNull() && source.width() > 0 && source.height() > 0) {
        const qreal zoom = std::max(destination.width() / source.width(),
                                    destination.height() / source.height());
        const qreal sw = destination.width() / zoom;
        const qreal sh = destination.height() / zoom;
        painter.drawImage(destination, source,
                          QRectF{(source.width() - sw) / 2,
                                 (source.height() - sh) / 2, sw, sh});
    } else {
        label(painter, displayName.trimmed().left(1).toUpper(), destination,
              42, palette.primary, QFont::DemiBold, Qt::AlignCenter);
    }
    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen{palette.rule, 1.3});
    painter.drawEllipse(destination);
}

void statistic(QPainter& painter, const Palette& palette,
               const QRectF& area, const QString& value,
               const QString& title)
{
    const int size = value.size() > 8 ? 34 : value.size() > 5 ? 41 : 53;
    label(painter, value,
          QRectF{area.left() + 6, area.top() + 10, area.width() - 12, 70},
          size, palette.primary, QFont::DemiBold, Qt::AlignCenter);
    label(painter, title,
          QRectF{area.left() + 6, area.top() + 78, area.width() - 12, 38},
          21, palette.secondary, QFont::Normal, Qt::AlignCenter);
}

// Minimal is a separate editorial composition, not a recolored Neon card.
// Its information hierarchy (single hero + two publishing figures + three
// reaction figures) is intentionally different from Neon's stacked dashboard.
void paintMinimalCard(QImage& output, const QVariantMap& stats,
                      const QString& displayName, const QString& username,
                      const QImage& portrait, const int periodDays,
                      const qint64 capturedAt)
{
    const Palette palette = colorsFor(true);
    QPainter painter{&output};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.scale(RasterScale, RasterScale);
    const QRectF canvas{0, 0, LayoutWidth, LayoutHeight};
    painter.fillRect(canvas, QColor{"#FFFFFF"});

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor{"#F7F8FB"});
    painter.drawRoundedRect(QRectF{56, 301, 848, 252}, 24, 24);
    painter.fillRect(QRectF{915, 58, 7, 75}, palette.accent);

    avatar(painter, portrait, displayName, palette, true);
    label(painter, displayName.isEmpty() ? QStringLiteral("Member") : displayName,
          QRectF{161, 62, 498, 40}, 31, palette.primary, QFont::DemiBold);
    label(painter, username.startsWith(u'@') ? username : QStringLiteral("@") + username,
          QRectF{161, 103, 485, 27}, 21, palette.secondary);
    label(painter, QStringLiteral("GENYCASTER  /  STATS"),
          QRectF{665, 74, 225, 32}, 16, palette.accent,
          QFont::DemiBold, Qt::AlignRight);
    rule(painter, palette, 161);

    const QString period = periodDays == 7 ? QStringLiteral("7")
        : periodDays == 30 ? QStringLiteral("30")
        : periodDays == 90 ? QStringLiteral("90") : QStringLiteral("365");
    label(painter, QStringLiteral("ACTIVITY REPORT"),
          QRectF{58, 187, 500, 32}, 18, palette.accent, QFont::DemiBold);
    label(painter, period + QStringLiteral("-day overview"),
          QRectF{56, 224, 720, 63}, 45, palette.primary, QFont::DemiBold);

    const bool hasEngagement = stats.value(QStringLiteral("hasEngagement")).toBool();
    const QString interactions = hasEngagement
        ? formattedCount(stats, "engagement") : QStringLiteral("—");
    label(painter, interactions, QRectF{87, 327, 538, 144},
          interactions.size() > 7 ? 88 : 132, palette.primary, QFont::Bold);
    label(painter, QStringLiteral("Interactions received"),
          QRectF{92, 472, 530, 45}, 27, palette.secondary);
    painter.setPen(QPen{palette.rule, 1.5});
    painter.drawLine(QPointF{632, 352}, QPointF{632, 500});
    label(painter, period, QRectF{656, 367, 211, 81},
          58, palette.accent, QFont::DemiBold, Qt::AlignCenter);
    label(painter, QStringLiteral("DAYS"), QRectF{657, 439, 210, 32},
          17, palette.secondary, QFont::DemiBold, Qt::AlignCenter);

    label(painter, QStringLiteral("PUBLISHING"),
          QRectF{58, 574, 400, 32}, 17, palette.accent, QFont::DemiBold);
    label(painter, formattedCount(stats, "casts"),
          QRectF{61, 616, 382, 79}, 57, palette.primary, QFont::DemiBold);
    label(painter, QStringLiteral("Casts"),
          QRectF{61, 688, 382, 35}, 21, palette.secondary);
    painter.setPen(QPen{palette.rule, 1.4});
    painter.drawLine(QPointF{473, 622}, QPointF{473, 720});
    label(painter, formattedCount(stats, "replies"),
          QRectF{516, 616, 369, 79}, 57, palette.primary, QFont::DemiBold);
    label(painter, QStringLiteral("Replies written"),
          QRectF{516, 688, 360, 35}, 21, palette.secondary);
    rule(painter, palette, 749);

    label(painter, QStringLiteral("AUDIENCE RESPONSE"),
          QRectF{58, 762, 610, 31}, 17, palette.accent, QFont::DemiBold);
    const QString values[]{
        hasEngagement ? formattedCount(stats, "likes") : QStringLiteral("—"),
        hasEngagement ? formattedCount(stats, "recasts") : QStringLiteral("—"),
        hasEngagement ? formattedCount(stats, "receivedReplies") : QStringLiteral("—")
    };
    const QString captions[]{
        QStringLiteral("Likes"), QStringLiteral("Recasts"),
        QStringLiteral("Replies received")
    };
    constexpr qreal column = 848.0 / 3.0;
    for (int i = 0; i < 3; ++i) {
        const QRectF area{56 + column * i, 798, column, 94};
        label(painter, values[i], QRectF{area.left(), area.top(), column - 12, 57},
              42, palette.primary, QFont::DemiBold);
        label(painter, captions[i],
              QRectF{area.left(), area.top() + 52, column - 12, 31},
              19, palette.secondary);
    }
    rule(painter, palette, 900);
    label(painter, QStringLiteral("Posts in period · engagement counted to date"),
          QRectF{57, 908, 660, 31}, 16, palette.subtle);
    if (capturedAt > 0) {
        const auto date = QDateTime::fromMSecsSinceEpoch(capturedAt).date();
        label(painter, QLocale{QLocale::English}.toString(
                  date, QStringLiteral("MMM d, yyyy")),
              QRectF{725, 908, 178, 31}, 16, palette.subtle,
              QFont::Normal, Qt::AlignRight);
    }
}

void paintCard(QImage& output, const QVariantMap& stats,
               const QString& displayName, const QString& username,
               const QImage& portrait, const int periodDays,
               const bool minimal, const qint64 capturedAt)
{
    if (minimal) {
        paintMinimalCard(output, stats, displayName, username,
                         portrait, periodDays, capturedAt);
        return;
    }
    const Palette palette = colorsFor(minimal);
    QPainter painter{&output};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.scale(RasterScale, RasterScale);
    painter.setPen(Qt::NoPen);

    const QRectF canvas{0, 0, LayoutWidth, LayoutHeight};
    QPainterPath clip;
    clip.addRoundedRect(canvas.adjusted(2, 2, -2, -2), 36, 36);
    painter.setClipPath(clip);
    QLinearGradient background{QPointF{0, 0}, QPointF{960, 960}};
    background.setColorAt(0.0, minimal ? QColor{"#FFFFFF"} : QColor{"#101831"});
    background.setColorAt(1.0, minimal ? QColor{"#F0F3FA"} : QColor{"#18162D"});
    painter.fillRect(canvas, background);
    if (!minimal) {
        QRadialGradient ambient{QPointF{880, 180}, 560};
        ambient.setColorAt(0.0, QColor{139, 91, 204, 49});
        ambient.setColorAt(1.0, Qt::transparent);
        painter.fillRect(canvas, ambient);

        // Quiet concentric light rings occupy the spare hero space, never
        // representing invented chart data or a fabricated engagement trend.
        painter.save();
        painter.setBrush(Qt::NoBrush);
        for (int ringIndex = 0; ringIndex < 3; ++ringIndex) {
            QColor halo{"#A68DFF"};
            halo.setAlpha(67 - ringIndex * 19);
            painter.setPen(QPen{halo, 2.0});
            const qreal radius = 82.0 + 23.0 * ringIndex;
            painter.drawEllipse(QPointF{779, 397}, radius, radius);
        }
        painter.restore();
    }
    painter.setClipping(false);
    painter.setPen(QPen{minimal ? QColor{"#DDE3EF"} : QColor{"#564B90"}, 1.4});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(canvas.adjusted(2, 2, -2, -2), 36, 36);

    avatar(painter, portrait, displayName, palette, minimal);
    label(painter, displayName.isEmpty() ? QStringLiteral("Member") : displayName,
          QRectF{161, 62, 518, 38}, 31, palette.primary, QFont::DemiBold);
    label(painter, username.startsWith(u'@') ? username : QStringLiteral("@") + username,
          QRectF{161, 103, 485, 27}, 21, palette.secondary);
    label(painter, QStringLiteral("GENYCASTER / STATS"),
          QRectF{674, 74, 230, 30}, 17, palette.accent,
          QFont::DemiBold, Qt::AlignRight);
    rule(painter, palette, 165);

    const QString period = periodDays == 7 ? QStringLiteral("7 days")
        : periodDays == 30 ? QStringLiteral("30 days")
        : periodDays == 90 ? QStringLiteral("90 days") : QStringLiteral("365 days");
    label(painter, period + QStringLiteral(" recap"),
          QRectF{57, 197, 620, 65}, 44, palette.primary, QFont::DemiBold);
    label(painter, QStringLiteral("ACTIVITY"), QRectF{750, 214, 154, 31},
          17, palette.accent, QFont::DemiBold, Qt::AlignRight);

    const bool hasEngagement = stats.value(QStringLiteral("hasEngagement")).toBool();
    const QString interactions = hasEngagement
        ? formattedCount(stats, "engagement") : QStringLiteral("—");
    label(painter, interactions, QRectF{56, 300, 830, 145},
          interactions.size() > 7 ? 91 : 126,
          palette.primary, QFont::Bold);
    label(painter, QStringLiteral("Interactions received"),
          QRectF{61, 453, 820, 55}, 30, palette.secondary);
    rule(painter, palette, 544);

    const QString values[]{
        formattedCount(stats, "casts"), formattedCount(stats, "replies"),
        hasEngagement ? formattedCount(stats, "likes") : QStringLiteral("—"),
        hasEngagement ? formattedCount(stats, "recasts") : QStringLiteral("—"),
        hasEngagement ? formattedCount(stats, "receivedReplies") : QStringLiteral("—")
    };
    const QString captions[]{
        QStringLiteral("Casts"), QStringLiteral("Replies written"),
        QStringLiteral("Likes"), QStringLiteral("Recasts"),
        QStringLiteral("Replies received")
    };
    // First row 3 metrics, second row 2. No five independent floating boxes.
    constexpr qreal column = 848.0 / 3.0;
    for (int i = 0; i < 3; ++i)
        statistic(painter, palette, QRectF{56 + column * i, 565, column, 125},
                  values[i], captions[i]);
    rule(painter, palette, 715);
    for (int i = 0; i < 2; ++i)
        statistic(painter, palette, QRectF{56 + 424.0 * i, 731, 424, 121},
                  values[i + 3], captions[i + 3]);
    rule(painter, palette, 864);

    label(painter, QStringLiteral("Posts in period · engagement counted to date"),
          QRectF{57, 880, 670, 38}, 17, palette.subtle);
    if (capturedAt > 0) {
        const auto date = QDateTime::fromMSecsSinceEpoch(capturedAt).date();
        label(painter, QLocale{QLocale::English}.toString(
                  date, QStringLiteral("MMM d, yyyy")),
              QRectF{724, 880, 180, 38}, 17, palette.subtle,
              QFont::Normal, Qt::AlignRight);
    }
}

} // namespace

StatsShareImage renderStatsShareCard(
    const QVariantMap& snapshot, const QString& displayName,
    const QString& username, const QImage& avatarImage, const int periodDays,
    const QString& style, const double capturedAtMs)
{
    if ((style != QStringLiteral("neon") && style != QStringLiteral("minimal"))
        || (periodDays != 7 && periodDays != 30 && periodDays != 90
            && periodDays != 365)
        || !std::isfinite(capturedAtMs) || capturedAtMs <= 0.0)
        return {.error = QStringLiteral("invalid_share_card_parameters")};

    QImage output{CanvasWidth, CanvasHeight, QImage::Format_ARGB32_Premultiplied};
    if (output.isNull()) return {.error = QStringLiteral("share_card_image_unavailable")};
    output.fill(Qt::transparent);
    paintCard(output, snapshot, displayName.left(120), username.left(120),
              avatarImage, periodDays, style == QStringLiteral("minimal"),
              static_cast<qint64>(capturedAtMs));
    const QString tempDir = QDir::tempPath();
    if (tempDir.isEmpty() || !QDir{}.mkpath(tempDir))
        return {.error = QStringLiteral("share_card_temp_unavailable")};
    QTemporaryFile temp{tempDir + QStringLiteral("/genycaster-stats-XXXXXX.png")};
    if (!temp.open()) return {.error = QStringLiteral("share_card_temp_unavailable")};
    if (!output.save(&temp, "PNG") || temp.size() <= 0 || !temp.flush())
        return {.error = QStringLiteral("share_card_png_failed")};
    const QUrl localUrl = QUrl::fromLocalFile(temp.fileName());
    temp.setAutoRemove(false);
    temp.close();
    return {.fileUrl = localUrl};
}

} // namespace genycaster::presentation
