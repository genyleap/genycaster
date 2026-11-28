#include "../../ui/app/network/link_preview.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QStringList>
#include <QVariantMap>
#include <vector>
#include <algorithm>
#include <source_location>

namespace {
void require(bool value, std::source_location where = std::source_location::current())
{
    if (!value) qFatal("Link seed contract failed at line %u", where.line());
}
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    LinkPreviewResolver resolver{true};
    QSignalSpy revisions{&resolver, &LinkPreviewResolver::revisionChanged};
    QSignalSpy completions{&resolver, &LinkPreviewResolver::previewReady};
    const auto github = QStringLiteral("https://github.com/genyleap/genycaster#readme");
    const auto seed = resolver.previewFor(github);
    require(seed.value(QStringLiteral("providerKind")).toString() == QStringLiteral("github"));
    require(seed.value(QStringLiteral("title")).toString() == QStringLiteral("genyleap/genycaster"));
    require(seed == resolver.previewFor(QStringLiteral("https://github.com/genyleap/genycaster")));
    require(seed == resolver.previewFor(github));
    require(resolver.previewFor(QStringLiteral("http://127.0.0.1/private")).isEmpty());
    require(resolver.previewFor(QStringLiteral("http://127.0.0.1/private")).isEmpty());
    const auto longUrl = QStringLiteral("https://example.invalid/") + QString(3000, QLatin1Char('a'));
    require(resolver.previewFor(longUrl) == resolver.previewFor(longUrl));

    QStringList urls;
    for (int i = 0; i < 512; ++i)
        urls.append(QStringLiteral("https://example.invalid/artwork-%1?q=one#part").arg(i));
    std::vector<QVariantMap> expected;
    expected.reserve(512);
    QElapsedTimer clock;
    clock.start();
    for (const auto& url : urls) expected.push_back(resolver.previewFor(url));
    const double cold = clock.nsecsElapsed() / 1e6;
    std::vector<double> warm;
    for (int repeat = 0; repeat < 40; ++repeat) {
        clock.restart();
        for (const auto& url : urls) {
            const auto actual = resolver.previewFor(url);
            require(!actual.isEmpty());
        }
        warm.push_back(clock.nsecsElapsed() / 1e6);
    }
    std::sort(warm.begin(), warm.end());
    for (int i = 0; i < urls.size(); ++i)
        require(resolver.previewFor(urls.at(i)) == expected.at(static_cast<std::size_t>(i)));
    for (int i = 0; i < 1500; ++i)
        require(!resolver.previewFor(QStringLiteral("https://example.invalid/other-%1").arg(i)).isEmpty());
    require(resolver.previewFor(github) == seed);
    require(resolver.previewFor(urls.front()) == expected.front());
    require(revisions.isEmpty() && completions.isEmpty() && resolver.revision() == 0);
    qInfo("Link preview 512 URLs: cold %.3f ms; warm median %.3f ms", cold, warm.at(20));
    return 0;
}
