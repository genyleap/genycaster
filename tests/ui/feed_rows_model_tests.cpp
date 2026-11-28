#include "../../ui/app/social/feed_rows.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QAbstractItemModelTester>

#include <limits>
#include <source_location>

namespace {
void require(const bool condition,
             const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Feed model runtime test failed at %s:%u", location.file_name(), location.line());
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    FeedRowsModel model{};
    QSignalSpy added{&model, &QAbstractItemModel::rowsInserted};
    QSignalSpy removed{&model, &QAbstractItemModel::rowsRemoved};
    QSignalSpy moved{&model, &QAbstractItemModel::rowsMoved};
    QSignalSpy changed{&model, &QAbstractItemModel::dataChanged};
    QSignalSpy resets{&model, &QAbstractItemModel::modelReset};
    QSignalSpy counts{&model, &FeedRowsModel::countChanged};

    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
        model.append({
            {QStringLiteral("stableKey"), QStringLiteral("0x%1").arg(i)},
            {QStringLiteral("revisionKey"), QStringLiteral("base")},
            {QStringLiteral("presentationKind"), QStringLiteral("conversation")},
            {QStringLiteral("mediaLayoutKind"), QStringLiteral("text")},
        });
    }
    require(model.count() == n && model.rowCount() == n);
    // ModelTester performs global consistency checks on each insertion.
    // Attach after bulk fixture construction so this remains a bounded
    // 10k-row test, while still checking all targeted edits and moves.
    QAbstractItemModelTester tester{&model,
        QAbstractItemModelTester::FailureReportingMode::Fatal};
    require(added.size() == n && counts.size() == n);
    require(resets.isEmpty());
    const QVariantMap before = model.get(5000);
    require(before.value(QStringLiteral("stableKey")) == QStringLiteral("0x5000"));
    require(!model.get(-1).contains(QStringLiteral("stableKey")));
    require(!model.get(n).contains(QStringLiteral("stableKey")));

    model.setProperty(5000, QStringLiteral("revisionKey"), QStringLiteral("liked"));
    require(changed.size() == 1);
    const auto roles = changed.first().at(2).value<QList<int>>();
    require(roles.size() == 1 && roles.first() == FeedRowsModel::RevisionKeyRole);
    require(changed.first().at(0).value<QModelIndex>().row() == 5000);
    model.setProperty(5000, QStringLiteral("revisionKey"), QStringLiteral("liked"));
    model.setProperty(5000, QStringLiteral("mediaLayoutKind"), QStringLiteral("text"));
    model.setProperty(5000, QStringLiteral("stableKey"), QStringLiteral("changed"));
    model.setProperty(n + 1, QStringLiteral("revisionKey"), QStringLiteral("invalid"));
    require(changed.size() == 1);
    require(model.get(5000).value(QStringLiteral("stableKey")) == QStringLiteral("0x5000"));
    model.setProperty(5000, QStringLiteral("mediaLayoutKind"), QStringLiteral("single-image"));
    require(changed.size() == 2);
    const auto mediaRoles = changed.last().at(2).value<QList<int>>();
    require(mediaRoles.size() == 1 && mediaRoles.first() == FeedRowsModel::MediaLayoutKindRole);
    require(added.size() == n && removed.isEmpty() && moved.isEmpty());

    model.move(9000, 100, 1);
    require(moved.size() == 1 && model.get(100).value(QStringLiteral("stableKey"))
        == QStringLiteral("0x9000"));
    require(model.get(101).value(QStringLiteral("stableKey")) == QStringLiteral("0x100"));
    model.move(100, 9000, 1);
    require(moved.size() == 2 && model.get(9000).value(QStringLiteral("stableKey"))
        == QStringLiteral("0x9000"));
    require(resets.isEmpty());
    // A QML caller can supply arbitrary 32-bit integers. Previously these
    // calls evaluated from + amount / to + amount before rejecting the input.
    // UndefinedBehaviorSanitizer must not flag signed overflow here.
    const int maximum = std::numeric_limits<int>::max();
    const qsizetype movesBeforeInvalidInput = moved.size();
    model.move(maximum, 0, maximum);
    model.move(0, maximum, maximum);
    model.move(maximum, maximum, 1);
    model.move(0, 1, maximum);
    model.move(1, 0, maximum);
    model.move(-1, 0, maximum);
    require(moved.size() == movesBeforeInvalidInput);
    require(model.count() == n);
    require(model.get(9000).value(QStringLiteral("stableKey"))
            == QStringLiteral("0x9000"));
    model.remove(9000);
    require(model.count() == n - 1 && removed.size() == 1);
    require(!model.get(9000).value(QStringLiteral("stableKey")).toString().isEmpty());

    model.insert(0, {
        {QStringLiteral("stableKey"), QStringLiteral("0xnew")},
        {QStringLiteral("revisionKey"), QStringLiteral("incoming")},
        {QStringLiteral("mediaLayoutKind"), QStringLiteral("gallery")},
    });
    require(model.count() == n);
    require(model.get(0).value(QStringLiteral("stableKey")) == QStringLiteral("0xnew"));
    require(model.get(1).value(QStringLiteral("stableKey")) == QStringLiteral("0x0"));
    require(resets.isEmpty());
    model.clear();
    require(model.count() == 0 && resets.size() == 1);
    require(counts.size() == n + 3);

    // The native magazine owns geometry for the whole feed but QML objects
    // only for the viewport. Mixed measured heights never overlap lanes.
    FeedRowsModel casts{};
    for (int i = 0; i < 600; ++i) {
        casts.append({
            {QStringLiteral("stableKey"), QStringLiteral("cast:%1").arg(i)},
            {QStringLiteral("revisionKey"), QStringLiteral("r1")},
            {QStringLiteral("presentationKind"), QStringLiteral("conversation")},
            {QStringLiteral("mediaLayoutKind"), QStringLiteral("other")},
        });
    }
    MasonryWindowModel magazine{};
    magazine.setSourceModel(&casts);
    QAbstractItemModelTester magazineTester{&magazine,
        QAbstractItemModelTester::FailureReportingMode::Fatal};
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, false);
    magazine.setViewport(0.0, 720.0, 280.0, 950.0, true);
    require(magazine.count() > 0 && magazine.count() < 60);
    // indexAt crosses the QML/C++ boundary; do not cast an unbounded double
    // to int inside laneAt, and reject non-finite viewport coordinates.
    const qreal extreme = 1.0e300;
    require(magazine.indexAt(extreme, 12.0) >= 0);
    require(magazine.indexAt(-extreme, 12.0) >= 0);
    require(magazine.indexAt(std::numeric_limits<qreal>::infinity(), 12.0) == -1);
    require(magazine.indexAt(std::numeric_limits<qreal>::quiet_NaN(), 12.0) == -1);
    require(magazine.indexAt(0.0, std::numeric_limits<qreal>::quiet_NaN()) == -1);
    const auto top = magazine.geometryAt(0);
    const auto nextInLane = magazine.geometryAt(3);
    require(top.value(QStringLiteral("x")) == nextInLane.value(QStringLiteral("x")));
    const qreal previousLaneY = nextInLane.value(QStringLiteral("y")).toDouble();
    require(magazine.retainHeight(QStringLiteral("cast:0"), QStringLiteral("r1"),
                                 320.0, 900.0, true));
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, false);
    const auto grown = magazine.geometryAt(3);
    require(grown.value(QStringLiteral("y")).toDouble() > previousLaneY + 600.0);
    require(magazine.geometryAt(1).value(QStringLiteral("y")) ==
           top.value(QStringLiteral("y")));

    // A previously admitted row holds its lane after a height change.
    require(grown.value(QStringLiteral("x")) == top.value(QStringLiteral("x")));
    // The explicit reset API is only for changing presentations, never
    // for image hydration. It may redistribute the numeric layout.
    magazine.requestRepack();
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, false);
    const auto rebalanced = magazine.geometryAt(3);
    require(rebalanced.value(QStringLiteral("x")) != top.value(QStringLiteral("x")));
    require(rebalanced.value(QStringLiteral("y")).toDouble() < previousLaneY + 90.0);
    require(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 900.0);
    // A resize can change both lane count and viewport position. Numeric
    // geometry must not evict the old viewport before QML restores its anchor.
    const int originalWindowCount = magazine.count();
    QSignalSpy resizeRemovals{&magazine, &QAbstractItemModel::rowsRemoved};
    magazine.relayout(2, 470.0, 12.0, 12.0, 12.0, false, true);
    require(magazine.count() == originalWindowCount && resizeRemovals.isEmpty());
    require(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 900.0);
    magazine.setViewport(8000.0, 720.0, 280.0, 950.0, true, true);
    require(magazine.windowIndexForSource(0) >= 0);
    require(resizeRemovals.isEmpty());
    magazine.setViewport(8000.0, 720.0, 280.0, 950.0, true, false);
    require(magazine.windowIndexForSource(0) < 0);
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, false);
    magazine.setViewport(8000.0, 720.0, 280.0, 950.0, true);
    require(magazine.count() > 0 && magazine.count() < 70);
    const int distantSource = magazine.data(magazine.index(0, 0),
                              MasonryWindowModel::SourceIndexRole).toInt();
    require(distantSource > 20);
    magazine.setViewport(0.0, 720.0, 280.0, 950.0, true);
    require(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 900.0);
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, true);
    require(magazine.geometryAt(0).value(QStringLiteral("x")).toDouble() >
           magazine.geometryAt(1).value(QStringLiteral("x")).toDouble());
    casts.setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("r2"));
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, true);
    require(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 900.0);
    // Viewer revisions preserve geometry until genuinely changed content
    // reports a new measured size.
    require(magazine.retainHeight(QStringLiteral("cast:0"), QStringLiteral("r2"),
                                 320.0, 420.0, true));
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, true);
    require(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 420.0);
    magazine.setViewport(0.0, 720.0, 280.0, 950.0, false);
    require(magazine.count() == 0);

    // Batch reconciliation preserves rows across hydration, append and
    // reordering; only an unrelated scope replaces the entire model.
    FeedRowsModel incremental{};
    QAbstractItemModelTester incrementalTester{&incremental,
        QAbstractItemModelTester::FailureReportingMode::Fatal};
    const auto row = [](const QString& key, const QString& revision) {
        return QVariantMap{{QStringLiteral("stableKey"), key},
                           {QStringLiteral("revisionKey"), revision},
                           {QStringLiteral("presentationKind"), QStringLiteral("conversation")},
                           {QStringLiteral("mediaLayoutKind"), QStringLiteral("other")}};
    };
    incremental.reconcile({row(QStringLiteral("a"), QStringLiteral("r1")),
                           row(QStringLiteral("b"), QStringLiteral("r1")),
                           row(QStringLiteral("c"), QStringLiteral("r1"))});
    QSignalSpy incrementalResets{&incremental, &QAbstractItemModel::modelReset};
    QSignalSpy incrementalMoves{&incremental, &QAbstractItemModel::rowsMoved};
    QSignalSpy incrementalChanges{&incremental, &QAbstractItemModel::dataChanged};
    incremental.reconcile({row(QStringLiteral("a"), QStringLiteral("r1")),
                           row(QStringLiteral("b"), QStringLiteral("r2")),
                           row(QStringLiteral("c"), QStringLiteral("r1")),
                           row(QStringLiteral("d"), QStringLiteral("r1"))});
    require(incremental.count() == 4 && incrementalChanges.size() == 1);
    require(incrementalResets.isEmpty());
    incremental.reconcile({row(QStringLiteral("c"), QStringLiteral("r1")),
                           row(QStringLiteral("a"), QStringLiteral("r1")),
                           row(QStringLiteral("b"), QStringLiteral("r2"))});
    require(incremental.count() == 3);
    require(incremental.get(0).value(QStringLiteral("stableKey")).toString()
           == QStringLiteral("c"));
    require(incrementalMoves.size() >= 1 && incrementalResets.isEmpty());
    incremental.reconcile({row(QStringLiteral("other"), QStringLiteral("r1"))});
    require(incrementalResets.size() == 1);

    // A large stable-order hydration must notify ONLY the mutable row roles;
    // reprocessing an unchanged snapshot must never reset the view or emit
    // redundant updates. Use runtime checks independent of NDEBUG so this
    // verification also runs in optimized CI/benchmark configurations.
    FeedRowsModel stableOrder{};
    QVariantList stableSnapshot{};
    stableSnapshot.reserve(2048);
    for (int index = 0; index < 2048; ++index) {
        stableSnapshot.append(row(QStringLiteral("stable:%1").arg(index),
                                  QStringLiteral("r1")));
    }
    stableOrder.reconcile(stableSnapshot);
    QAbstractItemModelTester stableTester{
        &stableOrder, QAbstractItemModelTester::FailureReportingMode::Fatal};
    QSignalSpy stableChanges{&stableOrder, &QAbstractItemModel::dataChanged};
    QSignalSpy stableResets{&stableOrder, &QAbstractItemModel::modelReset};
    QSignalSpy stableMoves{&stableOrder, &QAbstractItemModel::rowsMoved};
    QSignalSpy stableInserts{&stableOrder, &QAbstractItemModel::rowsInserted};
    stableOrder.reconcile(stableSnapshot);
    if (!stableChanges.isEmpty() || !stableResets.isEmpty()
        || !stableMoves.isEmpty() || !stableInserts.isEmpty()) {
        qFatal("Stable unchanged snapshot emitted unnecessary model notifications");
    }
    stableSnapshot[1024] = row(QStringLiteral("stable:1024"), QStringLiteral("r2"));
    stableOrder.reconcile(stableSnapshot);
    if (stableChanges.size() != 1 || !stableResets.isEmpty()
        || !stableMoves.isEmpty() || !stableInserts.isEmpty()) {
        qFatal("Stable-order hydration did not deliver one targeted role update");
    }
    const auto roleChange = stableChanges.first();
    const auto changedRoles = roleChange.at(2).value<QList<int>>();
    if (roleChange.at(0).value<QModelIndex>().row() != 1024
        || changedRoles != QList<int>{FeedRowsModel::RevisionKeyRole}
        || stableOrder.get(1024).value(QStringLiteral("revisionKey"))
           != QStringLiteral("r2")) {
        qFatal("Stable-order hydration changed the wrong role or row");
    }
    stableOrder.reconcile(stableSnapshot);
    if (stableChanges.size() != 1) {
        qFatal("Repeated stable-order hydration re-notified an unchanged row");
    }

    // Geometry and presentation roles may change together without changing
    // the cast's identity. Both roles must reach the existing delegate.
    auto updatedMedia = stableSnapshot[1024].toMap();
    updatedMedia.insert(QStringLiteral("presentationKind"),
                        QStringLiteral("visual"));
    updatedMedia.insert(QStringLiteral("mediaLayoutKind"),
                        QStringLiteral("gallery"));
    stableSnapshot[1024] = updatedMedia;
    stableOrder.reconcile(stableSnapshot);
    if (stableChanges.size() != 2 || !stableResets.isEmpty()
        || !stableMoves.isEmpty() || !stableInserts.isEmpty()) {
        qFatal("Same-key media/presentation hydration changed model structure");
    }
    const auto mediaChange = stableChanges.last();
    if (mediaChange.at(0).value<QModelIndex>().row() != 1024
        || mediaChange.at(2).value<QList<int>>()
           != QList<int>{FeedRowsModel::PresentationKindRole,
                         FeedRowsModel::MediaLayoutKindRole}) {
        qFatal("Same-key media/presentation hydration emitted incorrect role IDs");
    }
    return 0;
}
