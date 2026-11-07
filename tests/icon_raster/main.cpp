#include "../../ui/app/media/android_icon.hpp"

#include <QGuiApplication>
#include <QImage>
#include <QSize>
#include <QtGlobal>

#include <iostream>

int main(int argc, char* argv[])
{
    QGuiApplication application{argc, argv};
    AndroidTintedIconProvider provider{};
    QSize reported{};
    const QImage tinted = provider.requestImage(
        QStringLiteral("system/check.svg?color=%23ff0000"),
        &reported, QSize{96, 96});
    if (tinted.isNull() || reported != QSize{96, 96}) {
        std::cerr << "Bundled icon rendering failed\n";
        return 1;
    }
    int transparentPixels = 0;
    int tintedPixels = 0;
    for (int y = 0; y < tinted.height(); ++y) {
        for (int x = 0; x < tinted.width(); ++x) {
            const QColor pixel = tinted.pixelColor(x, y);
            if (pixel.alpha() == 0) ++transparentPixels;
            if (pixel.alpha() > 80 && pixel.red() > 180
                && pixel.green() < 50 && pixel.blue() < 50) {
                ++tintedPixels;
            }
        }
    }
    if (transparentPixels < 100 || tintedPixels < 100) {
        std::cerr << "Icon is a solid quad or tint was lost: transparent="
                  << transparentPixels << ", colored=" << tintedPixels << '\n';
        return 2;
    }
    const QImage brand = provider.requestImage(
        QStringLiteral("brands/geny.svg?color=%23ffffff"),
        nullptr, QSize{64, 64});
    if (brand.isNull() || brand.pixelColor(0, 0).alpha() != 0) {
        std::cerr << "Brand SVG must render without a filled rectangle\n";
        return 5;
    }
    if (!provider.requestImage(QStringLiteral("../private.svg?color=%23fff"),
                               nullptr, QSize{48, 48}).isNull()) {
        std::cerr << "SVG provider accepted a traversal path\n";
        return 3;
    }
    if (!provider.requestImage(QStringLiteral("nonexistent.svg?color=%23fff"),
                               nullptr, QSize{48, 48}).isNull()) {
        std::cerr << "SVG provider accepted a missing resource\n";
        return 4;
    }
    std::cout << "Icon transparency/tint/local-resource boundaries passed\n";
    return 0;
}
