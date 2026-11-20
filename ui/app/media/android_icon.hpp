/*!
 * @file        ui/app/media/android_icon.hpp
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
        QImage image{QSize{width, height}, QImage::Format_RGBA8888_Premultiplied};
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


/**
 * Qt Quick SVG decoding may upload BGRA textures through the host GLES
 * translator on Android emulators. Convert the small, bundled brand assets to
 * byte-ordered RGBA8 before passing them to the scene graph. Keep the original
 * SVG colors and transparency; unlike semantic icons, these are not tinted.
 */
class AndroidBrandImageProvider final : public QQuickImageProvider {
public:
    AndroidBrandImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    [[nodiscard]] QImage requestImage(
        const QString& id, QSize* const size, const QSize& requestedSize) override
    {
        QString resource;
        if (id == QStringLiteral("mark")) {
            resource = QStringLiteral(":/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg");
        } else if (id == QStringLiteral("wordmark")) {
            resource = QStringLiteral(":/qt/qml/Farcaster/Client/wasm/genycaster-wordmark.svg");
        } else if (id == QStringLiteral("wordmark-dark")) {
            resource = QStringLiteral(":/qt/qml/Farcaster/Client/wasm/genycaster-wordmark-dark.svg");
        } else {
            return {};
        }

        QSvgRenderer svg{resource};
        if (!svg.isValid()) {
            qWarning().noquote() << "Android brand SVG unavailable:" << resource;
            return {};
        }
        const QSize fallback = svg.defaultSize().isValid() ? svg.defaultSize() : QSize{128, 128};
        const QSize preferred = requestedSize.isValid() ? requestedSize : fallback;
        const QSize extent{std::clamp(preferred.width(), 1, 2048),
                           std::clamp(preferred.height(), 1, 2048)};
        QImage image{extent, QImage::Format_RGBA8888_Premultiplied};
        image.fill(Qt::transparent);
        QPainter painter{&image};
        painter.setRenderHint(QPainter::Antialiasing);
        svg.render(&painter);
        painter.end();
        if (size != nullptr) *size = image.size();
        return image;
    }
};
