#include <QBuffer>
#include <QByteArray>
#include <QCache>
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QSize>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
#include "raster_cache_fixture.inc"

void require(bool condition, int line)
{
    if (!condition)
        qFatal("Raster cache regression at line %d", line);
}
#define CHECK(condition) require((condition), __LINE__)

QImage fixtureImage(QSize size)
{
    QImage image(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x)
            image.setPixel(x, y, qRgba(x % 256, y % 256, (x+y) % 256, 255));
    }
    return image;
}

QByteArray png(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    CHECK(buffer.open(QIODevice::WriteOnly));
    CHECK(image.save(&buffer, "PNG"));
    return bytes;
}

CachedImage load(const QString& url, const QSize& request)
{
    return cachedImage(imageCacheKey(url, request), url, request);
}
void save(const QString& url, const QSize& request, const CachedImage& decoded)
{
    rememberImage(imageCacheKey(url, request), url, request, decoded.image, decoded.originalSize);
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString url = QStringLiteral("https://example.invalid/avatar.png");
    const auto image = fixtureImage({128, 96});
    const auto bytes = png(image);
    QString error;
    const QSize first{128,128};
    const QSize larger{256,256};
    const QSize largest{512,512};
    const auto original = decodeImage(bytes, first, &error);
    CHECK(!original.image.isNull());
    CHECK(original.originalSize == image.size());
    CHECK(original.image.size() == image.size());
    save(url, first, original);
    CHECK(imageCache.size() == 1);
    const auto cost = imageCache.totalCost();
    CHECK(cost == imageCostKiB(original.image));
    for (const QSize request : {first, larger, largest, QSize{0,0}, QSize{128,0}}) {
        const auto cached = load(url, request);
        CHECK(!cached.image.isNull());
        CHECK(cached.image == decodeImage(bytes, request, &error).image);
        CHECK(cached.image.cacheKey() == original.image.cacheKey());
        save(url, request, cached);
        CHECK(imageCache.size() == 1);
        CHECK(imageCache.totalCost() == cost);
    }
    CHECK(load(QStringLiteral("https://example.invalid/other.png"), larger).image.isNull());

    // A smaller request must not receive the original or another scaled bucket.
    const QSize smaller{64,64};
    CHECK(load(url, smaller).image.isNull());
    CHECK(load(url, {128,64}).image.isNull());
    const auto reduced = decodeImage(bytes, smaller, &error);
    CHECK(reduced.image.size() == QSize(64,48));
    save(url, smaller, reduced);
    CHECK(imageCache.size() == 2);
    CHECK(load(url, smaller).image == reduced.image);
    CHECK(load(url, larger).image == original.image);
    CHECK(load(url, {32,32}).image.isNull());

    // Exercise the actual 128-pixel canonical request buckets too.
    const QString mediumUrl = QStringLiteral("https://example.invalid/medium.png");
    const auto medium = fixtureImage({192,144});
    const auto mediumBytes = png(medium);
    const QSize mediumRequest = canonicalRequestedSize({160,160});
    CHECK(mediumRequest == QSize(256,256));
    save(mediumUrl, mediumRequest, decodeImage(mediumBytes, mediumRequest, &error));
    CHECK(load(mediumUrl, canonicalRequestedSize({200,200})).image == medium);
    const QSize smallRequest = canonicalRequestedSize({90,90});
    CHECK(smallRequest == QSize(128,128));
    CHECK(load(mediumUrl, smallRequest).image.isNull());
    const auto smallMedium = decodeImage(mediumBytes, smallRequest, &error);
    CHECK(smallMedium.image.size() == QSize(128,96));
    save(mediumUrl, smallRequest, smallMedium);
    CHECK(load(mediumUrl, smallRequest).image == smallMedium.image);
    CHECK(load(mediumUrl, mediumRequest).image == medium);

    // Preserve exact keys if the codec returned unexpected geometry.
    const QString rotatedUrl = QStringLiteral("https://example.invalid/rotated.png");
    save(rotatedUrl, larger, CachedImage{fixtureImage({96,128}), {128,96}});
    CHECK(!load(rotatedUrl, larger).image.isNull());
    CHECK(load(rotatedUrl, largest).image.isNull());
    const QString codecUrl = QStringLiteral("https://example.invalid/codec.png");
    save(codecUrl, smaller, original);
    CHECK(!load(codecUrl, smaller).image.isNull());
    CHECK(load(codecUrl, larger).image.isNull());

    // Large originals stay on the previous policy; no implicit promotion.
    const QString bigUrl = QStringLiteral("https://example.invalid/large.png");
    const auto big = fixtureImage({320,256});
    const auto bigDecoded = decodeImage(png(big), largest, &error);
    save(bigUrl, largest, bigDecoded);
    CHECK(!load(bigUrl, largest).image.isNull());
    CHECK(load(bigUrl, {640,640}).image.isNull());

    // Natural entries obey the same cost and eviction rules.
    imageCache.clear();
    imageCache.setMaxCost(cost);
    save(url, first, original);
    CHECK(imageCache.size() == 1);
    save(QStringLiteral("https://example.invalid/second.png"), first, original);
    CHECK(imageCache.size() == 1);
    CHECK(imageCache.totalCost() == cost);
    CHECK(load(url, larger).image.isNull());
    CHECK(!load(QStringLiteral("https://example.invalid/second.png"), larger).image.isNull());
    qInfo("PASS: natural pixel equivalence, size isolation, URL isolation and bounded LRU");
    return 0;
}
