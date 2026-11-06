/*!
 * @file        ui/mobile/android_tinted_icon_provider.hpp
 * @brief       Android adaptive icon tinting image provider.
 *
 * @details
 * Creates themed Qt Quick icon images while preserving alpha and SVG
 * geometry in an Android-specific presentation adapter.
 *
 * @author      Kambiz Asadzadeh
 * @since       29 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QColor>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QQuickImageProvider>
#include <QString>
#include <QSvgRenderer>
#include <QUrlQuery>

#include <algorithm>

/**
 * Android's Qt Quick MultiEffect colorization can replace a translucent SVG
 * with a solid colored quad on some graphics drivers. Render icons through
 * QtSvg into an alpha-preserving image and apply the theme tint before the
 * image is sent to the GPU. Only embedded Genyleap icon resources are accepted.
 */
class AndroidTintedIconProvider final : public QQuickImageProvider {
public:
    AndroidTintedIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    [[nodiscard]] QImage requestImage(
        const QString& id, QSize* const size, const QSize& requestedSize) override
    {
        const QString path = id.section(QLatin1Char('?'), 0, 0);
        if (!path.endsWith(QStringLiteral(".svg")) || path.startsWith(QLatin1Char('/'))
            || path.contains(QStringLiteral("..")) || path.contains(QLatin1Char(':'))
            || path.contains(QLatin1Char('\\'))) {
            return {};
        }

        const QUrlQuery query{id.section(QLatin1Char('?'), 1)};
        const QColor tint{query.queryItemValue(QStringLiteral("color"), QUrl::FullyDecoded)};
        if (!tint.isValid()) return {};

        const QString resource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/") + path;
        QSvgRenderer svg{resource};
        if (!svg.isValid()) {
            qWarning().noquote() << "Android icon SVG resource unavailable:" << resource;
            return {};
        }

        const int width = std::clamp(requestedSize.width(), 1, 512);
        const int height = std::clamp(requestedSize.height(), 1, 512);
        QImage image{QSize{width, height}, QImage::Format_ARGB32_Premultiplied};
        image.fill(Qt::transparent);

        QPainter painter{&image};
        painter.setRenderHint(QPainter::Antialiasing);
        svg.render(&painter);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), tint);
        painter.end();

        if (size != nullptr) *size = image.size();
        return image;
    }
};
