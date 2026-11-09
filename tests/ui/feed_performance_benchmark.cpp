// Isolated CPU-side Qt feed model microbenchmark.
// No network, QML rendering, GPU work, or user-account data.
#include "../../ui/app/social/feed_rows.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numeric>
#include <utility>
#include <vector>

namespace {

using Samples = std::vector<double>;

[[nodiscard]] QVariantList makeRows(const int count)
{
    QVariantList rows;
    rows.reserve(count);
    for (int i = 0; i < count; ++i) {
        rows.append(QVariantMap{
            {QStringLiteral("stableKey"), QStringLiteral("cast:%1").arg(i)},
            {QStringLiteral("revisionKey"), QStringLiteral("r1")},
            {QStringLiteral("presentationKind"), QStringLiteral("cast")},
            {QStringLiteral("mediaLayoutKind"),
             i % 3 == 0 ? QStringLiteral("single-image") : QStringLiteral("other")},
        });
    }
    return rows;
}

template <typename Action>
void measure(Samples& measurements, Action&& operation)
{
    QElapsedTimer timer;
    timer.start();
    operation();
    measurements.push_back(static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0);
}

void report(const char* label, Samples samples, const int rows)
{
    std::ranges::sort(samples);
    const auto percentile = [&samples](const double fraction) -> double {
        const auto rank = static_cast<std::size_t>(
            std::ceil(static_cast<double>(samples.size()) * fraction));
        return samples.at(std::clamp(rank, std::size_t{1}, samples.size()) - 1);
    };

    const double sum = std::accumulate(samples.cbegin(), samples.cend(), 0.0);
    const double mean = sum / static_cast<double>(samples.size());
    std::printf("BENCH %-24s n=%d runs=%zu p50=%.4fms p95=%.4fms "
                "p99=%.4fms avg=%.4fms min=%.4fms max=%.4fms\n",
                label, rows, samples.size(), percentile(0.50), percentile(0.95),
                percentile(0.99), mean, samples.front(), samples.back());
}

[[nodiscard]] int positiveArgument(const char* text, const int fallback)
{
    if (!text) return fallback;
    bool valid = false;
    const int number = QString::fromUtf8(text).toInt(&valid);
    return valid ? std::max(10, number) : fallback;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const int rowCount = positiveArgument(argc > 1 ? argv[1] : nullptr, 2'000);
    const int repeatCount = positiveArgument(argc > 2 ? argv[2] : nullptr, 40);

    const QVariantList stable = makeRows(rowCount);
    QVariantList reordered = stable;
    const int first = rowCount / 3;
    const int length = std::min(90, rowCount / 3);
    std::reverse(reordered.begin() + first, reordered.begin() + first + length);

    FeedRowsModel model;
    MasonryWindowModel masonry;
    masonry.setSourceModel(&model);
    model.reconcile(stable);
    masonry.relayout(3, 410, 12, 60, 12, false);
    masonry.setViewport(150, 860, 420, 1'200, true);

    // Five unrecorded cycles stabilize allocation and Qt model state.
    for (int i = 0; i < 5; ++i) {
        model.reconcile(reordered);
        model.reconcile(stable);
        masonry.relayout(3, 410, 12, 60, 12, false);
        masonry.setViewport(1'200 + i * 50, 860, 420, 1'200, true);
    }

    Samples unchanged;
    Samples reorderedSamples;
    Samples relayout;
    Samples viewport;
    for (Samples* samples : {&unchanged, &reorderedSamples, &relayout, &viewport})
        samples->reserve(repeatCount);

    for (int i = 0; i < repeatCount; ++i) {
        measure(unchanged, [&] { model.reconcile(stable); });
        measure(reorderedSamples, [&] { model.reconcile(reordered); });
        model.reconcile(stable);
        measure(relayout, [&] { masonry.relayout(3, 410, 12, 60, 12, false); });
        measure(viewport, [&] {
            masonry.setViewport(1'000 + (i % 17) * 600, 860, 420, 1'200, true);
        });
    }

    report("reconcile-unchanged", std::move(unchanged), rowCount);
    report("reconcile-reordered90", std::move(reorderedSamples), rowCount);
    report("masonry-full-relayout", std::move(relayout), rowCount);
    report("masonry-scroll-window", std::move(viewport), rowCount);
    return 0;
}
