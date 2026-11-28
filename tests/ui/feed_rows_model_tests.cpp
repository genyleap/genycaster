#include "../../ui/app/social/feed_rows.hpp"
#include "../../src/presentation/qt/media_embed_equivalence.hpp"

#include <QCoreApplication>
#include <QThread>
#include <QSignalSpy>
#include <QAbstractItemModelTester>

#include <algorithm>
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

    // A disjoint viewport jump must evict before admission and batch
    // contiguous new rows. Overlapping windows retain persistent indexes.
    FeedRowsModel batchRows;
    for (int i = 0; i < 500; ++i)
        batchRows.append({{QStringLiteral("stableKey"), QString::number(i)},
                          {QStringLiteral("revisionKey"), QStringLiteral("r1")}});
    MasonryWindowModel batchWall;
    batchWall.setSourceModel(&batchRows);
    batchWall.relayout(2, 320, 12, 12, 12, false);
    QAbstractItemModelTester batchTester{&batchWall, QAbstractItemModelTester::FailureReportingMode::Fatal};
    QSignalSpy batchInserts{&batchWall, &QAbstractItemModel::rowsInserted};
    QSignalSpy batchRemoves{&batchWall, &QAbstractItemModel::rowsRemoved};
    QSignalSpy batchCounts{&batchWall, &MasonryWindowModel::countChanged};
    batchWall.setViewport(0, 1000, 0, 0, true);
    require(batchWall.count() > 2 && batchInserts.size() == 1 && batchCounts.size() == 1);
    const int originalCount = batchWall.count();
    const auto middle = batchWall.geometryAt(4).value(QStringLiteral("y")).toReal();
    const int retainedIndex = batchWall.windowIndexForSource(4);
    require(retainedIndex >= 0);
    const QPersistentModelIndex retained{batchWall.index(retainedIndex, 0)};
    batchWall.setViewport(middle, 1000, 0, 0, true);
    require(retained.isValid() && retained.data(MasonryWindowModel::StableKeyRole) == QStringLiteral("4"));
    batchInserts.clear();
    batchRemoves.clear();
    batchCounts.clear();
    int peak = batchWall.count();
    const auto monitor = QObject::connect(&batchWall, &QAbstractItemModel::rowsInserted,
        &batchWall, [&] { peak = std::max(peak, batchWall.count()); });
    const auto distant = batchWall.geometryAt(400).value(QStringLiteral("y")).toReal();
    batchWall.setViewport(distant, 1000, 0, 0, true);
    require(!retained.isValid());
    require(batchRemoves.size() == 1 && batchInserts.size() == 1 && batchCounts.size() <= 1);
    require(peak <= std::max(originalCount + 2, batchWall.count()));
    require(batchWall.windowIndexForSource(400) >= 0);
    QObject::disconnect(monitor);

    // Production admission budget: visible rows precede prefetch and no
    // transaction creates more than three slots, even after a large jump.
    MasonryWindowModel pacedWall;
    pacedWall.setSourceModel(&batchRows);
    pacedWall.relayout(2, 320, 12, 12, 12, false);
    QAbstractItemModelTester pacedTester{&pacedWall, QAbstractItemModelTester::FailureReportingMode::Fatal};
    QSignalSpy pacedInserts{&pacedWall, &QAbstractItemModel::rowsInserted};
    const auto topOf100 = pacedWall.geometryAt(100).value(QStringLiteral("y")).toReal();
    bool pending = pacedWall.setViewport(topOf100, 1000, 1200, 1600, true, false, 3);
    require(pending && pacedWall.count() == 3 && pacedWall.windowIndexForSource(100) >= 0);
    const auto oldSlot = QPersistentModelIndex{pacedWall.index(pacedWall.windowIndexForSource(100), 0)};
    int passes = 0;
    while (pending && passes++ < 100) {
        const int previousSlotCount = pacedWall.count();
        pending = pacedWall.setViewport(topOf100, 1000, 1200, 1600, true, false, 3);
        require(pacedWall.count() <= previousSlotCount + 3);
        require(oldSlot.isValid());
    }
    require(!pending && pacedWall.count() > 3);
    pacedInserts.clear();
    pending = pacedWall.setViewport(distant, 1000, 1200, 1600, true, false, 3);
    require(pending && pacedWall.count() == 3 && !oldSlot.isValid());
    require(pacedWall.windowIndexForSource(400) >= 0);
    require(!pacedWall.setViewport(0, 0, 0, 0, false, false, 3));
    require(pacedWall.count() == 0);

    // Eviction debt must never leave an old identity discoverable or
    // exceed the working set. Active replacements progress while draining.
    MasonryWindowModel retiredWall;
    retiredWall.setSourceModel(&batchRows);
    retiredWall.relayout(2, 320, 12, 12, 12, false);
    QAbstractItemModelTester retiredTester{&retiredWall, QAbstractItemModelTester::FailureReportingMode::Fatal};
    retiredWall.setViewport(0, 1000, 1200, 1600, true);
    const int originalSlots = retiredWall.count();
    require(originalSlots > 8);
    QSignalSpy retirementRemoves{&retiredWall, &QAbstractItemModel::rowsRemoved};
    bool debt = retiredWall.setViewport(distant, 1000, 1200, 1600, true, false, 3, 4);
    require(debt && retiredWall.count() <= originalSlots);
    require(retiredWall.windowIndexForSource(0) < 0);
    require(retiredWall.windowIndexForSource(400) >= 0);
    int removedInPass = 0;
    for (const auto& signal : retirementRemoves)
        removedInPass += signal.at(2).toInt() - signal.at(1).toInt() + 1;
    require(removedInPass <= 4);
    int drainPasses = 0;
    while (debt && drainPasses++ < 100)
        debt = retiredWall.setViewport(distant, 1000, 1200, 1600, true, false, 3, 4);
    require(!debt);
    for (int i = 0; i < retiredWall.count(); ++i)
        require(!retiredWall.index(i, 0).data(MasonryWindowModel::RetiringRole).toBool());
    const int beforeHide = retiredWall.count();
    debt = retiredWall.setViewport(0, 0, 0, 0, false, false, 3, 4);
    require(debt && retiredWall.count() == beforeHide - 4);
    require(retiredWall.windowIndexForSource(400) < 0);
    drainPasses = 0;
    while (debt && drainPasses++ < 100)
        debt = retiredWall.setViewport(0, 0, 0, 0, false, false, 3, 4);
    require(!debt && retiredWall.count() == 0);
    // A cooperative time budget uses single-row notifications, regardless
    // of the machine speed. No hard elapsed-time assertions in this test.
    retiredWall.setViewport(0, 1000, 1200, 1600, true);
    retirementRemoves.clear();
    debt = retiredWall.setViewport(distant, 1000, 1200, 1600, true, false, 3, 4, 5);
    require(debt && !retirementRemoves.isEmpty() && retirementRemoves.size() <= 4);
    for (const auto& signal : retirementRemoves)
        require(signal.at(1).toInt() == signal.at(2).toInt());
    require(retiredWall.windowIndexForSource(0) < 0);
    drainPasses = 0;
    while (debt && drainPasses++ < 100)
        debt = retiredWall.setViewport(distant, 1000, 1200, 1600, true, false, 3, 4, 5);
    require(!debt && retiredWall.windowIndexForSource(400) >= 0);

    // Simulate a slow model observer, not a slow machine. Once removal
    // consumes the deadline, at most one nearest newcomer is published.
    retiredWall.setViewport(0, 1000, 1200, 1600, true);
    const auto slowRemoval = QObject::connect(&retiredWall, &QAbstractItemModel::rowsRemoved,
        &retiredWall, [] { QThread::msleep(12); });
    QSignalSpy slowAdmissions{&retiredWall, &QAbstractItemModel::rowsInserted};
    debt = retiredWall.setViewport(distant, 1000, 1200, 1600, true, false, 3, 4, 5);
    QObject::disconnect(slowRemoval);
    require(debt && retiredWall.windowIndexForSource(0) < 0);
    int admittedAfterSlowRemoval = 0;
    for (const auto& signal : slowAdmissions)
        admittedAfterSlowRemoval += signal.at(2).toInt() - signal.at(1).toInt() + 1;
    require(admittedAfterSlowRemoval == 1);
    require(retiredWall.windowIndexForSource(400) >= 0);
    drainPasses = 0;
    while (debt && drainPasses++ < 100)
        debt = retiredWall.setViewport(distant, 1000, 1200, 1600, true, false, 3, 4, 5);
    require(!debt);

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

    // Revision-only changes must reach visible cards without rebuilding
    // numeric geometry, including a revision of an offscreen source row.
    FeedRowsModel revisionRows{};
    for (int i = 0; i < 5000; ++i)
        revisionRows.append({{QStringLiteral("stableKey"), QString::number(i)},
                             {QStringLiteral("revisionKey"), QStringLiteral("r1")}});
    MasonryWindowModel revisionWall{};
    revisionWall.setSourceModel(&revisionRows);
    revisionWall.relayout(3, 320, 12, 12, 12, false);
    revisionWall.setViewport(0, 720, 280, 950, true);
    QCoreApplication::processEvents();
    QSignalSpy invalidated{&revisionWall, &MasonryWindowModel::geometryInvalidated};
    QSignalSpy geometryChanges{&revisionWall, &MasonryWindowModel::geometryChanged};
    QSignalSpy revisions{&revisionWall, &QAbstractItemModel::dataChanged};
    const auto geometryBefore = revisionWall.geometryAt(0);
    const auto tailBefore = revisionWall.geometryAt(4999);
    const auto previousBottom = revisionWall.rowsBottom();
    const auto originalHeight = geometryBefore.value(QStringLiteral("height")).toReal();
    revisionRows.setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("r2"));
    require(revisions.size() == 1);
    require(revisions.first().at(2).value<QList<int>>() ==
            QList<int>{MasonryWindowModel::RevisionKeyRole});
    require(revisionWall.data(revisionWall.index(0,0), MasonryWindowModel::RevisionKeyRole)
            == QStringLiteral("r2"));
    require(!revisionWall.retainHeight(QStringLiteral("0"), QStringLiteral("r2"),
                                      320, originalHeight, true));
    revisionRows.setProperty(4999, QStringLiteral("revisionKey"), QStringLiteral("r3"));
    QCoreApplication::processEvents();
    require(invalidated.isEmpty() && geometryChanges.isEmpty() && revisions.size() == 1);
    require(revisionWall.geometryAt(0) == geometryBefore);
    require(revisionWall.geometryAt(4999) == tailBefore);
    require(revisionWall.rowsBottom() == previousBottom);
    revisionWall.setViewport(tailBefore.value(QStringLiteral("y")).toReal(), 720, 280, 950, true);
    const int tailWindowIndex = revisionWall.windowIndexForSource(4999);
    require(tailWindowIndex >= 0);
    require(revisionWall.data(revisionWall.index(tailWindowIndex,0),
            MasonryWindowModel::RevisionKeyRole) == QStringLiteral("r3"));
    // A genuine height change must still request layout. Returning to a
    // cached revision with a different size also takes the full path.
    require(revisionWall.retainHeight(QStringLiteral("0"), QStringLiteral("r2"),
                                      320, originalHeight + 300, true));
    revisionWall.relayout(3, 320, 12, 12, 12, false);
    require(revisionWall.geometryAt(0).value(QStringLiteral("height")).toReal()
            == originalHeight + 300);
    revisionWall.retainHeight(QStringLiteral("0"), QStringLiteral("r1"), 320,
                              originalHeight, true);
    revisionRows.setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("r1"));
    QCoreApplication::processEvents();
    require(invalidated.size() == 1);
    revisionWall.relayout(3, 320, 12, 12, 12, false);
    require(revisionWall.geometryAt(0).value(QStringLiteral("height")).toReal()
            == originalHeight);
    invalidated.clear();
    // A pending reorder remains dirty after its notification is dispatched.
    revisionRows.move(0, 1, 1);
    QCoreApplication::processEvents();
    require(invalidated.size() == 1);
    revisionRows.setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("moved"));
    QCoreApplication::processEvents();
    require(invalidated.size() == 2);
    revisionWall.relayout(3, 320, 12, 12, 12, false);
    require(revisionWall.geometryAt(0).value(QStringLiteral("stableKey")) == QStringLiteral("1"));
    revisionWall.setViewport(0, 720, 280, 950, true);
    require(revisionWall.data(revisionWall.index(0,0), MasonryWindowModel::RevisionKeyRole)
            == QStringLiteral("moved"));

    // Repeating a geometry commit for an explicit scroll jump must not
    // rebuild unchanged numeric rows or publish a redundant notification.
    geometryChanges.clear();
    const auto settledGeometry = revisionWall.geometryAt(0);
    revisionWall.relayout(3, 320, 12, 12, 12, false, true);
    require(geometryChanges.isEmpty());
    require(revisionWall.geometryAt(0) == settledGeometry);
    const auto settledHeight = settledGeometry.value(QStringLiteral("height")).toReal();
    // An actual height sample still invalidates geometry even when the
    // source model and the viewport dimensions have not changed.
    require(!revisionWall.retainHeight(QStringLiteral("1"), QStringLiteral("moved"),
                                       320, settledHeight, true));
    revisionWall.relayout(3, 320, 12, 12, 12, false, true);
    require(geometryChanges.isEmpty());
    revisionWall.setViewport(0, 720, 280, 950, true, true);
    require(revisionWall.retainHeight(QStringLiteral("1"), QStringLiteral("moved"),
                                      320, settledHeight + 120, true, true));
    revisionWall.relayout(3, 320, 12, 12, 12, false, true);
    require(geometryChanges.size() == 1);
    require(revisionWall.geometryAt(0).value(QStringLiteral("height")).toReal()
            == settledHeight + 120);
    // Clearing transient resize samples must also invalidate geometry.
    revisionWall.setViewport(0, 720, 280, 950, true, false);
    revisionWall.relayout(3, 320, 12, 12, 12, false, true);
    require(geometryChanges.size() == 2);
    require(revisionWall.geometryAt(0) == settledGeometry);

    // The caller can use the native structural outcome to avoid a second
    // per-row QVariantMap pass and unnecessary viewport restoration.
    FeedRowsModel structuralOutcome;
    const QVariantMap outcomeA{{QStringLiteral("stableKey"), QStringLiteral("a")},
                               {QStringLiteral("revisionKey"), QStringLiteral("r1")}};
    const QVariantMap outcomeB{{QStringLiteral("stableKey"), QStringLiteral("b")},
                               {QStringLiteral("revisionKey"), QStringLiteral("r1")}};
    QVariantMap updatedA = outcomeA;
    updatedA[QStringLiteral("revisionKey")] = QStringLiteral("r2");
    require(!structuralOutcome.reconcile({}));
    require(structuralOutcome.reconcile({outcomeA, outcomeB}));
    require(!structuralOutcome.reconcile({updatedA, outcomeB}));
    require(!structuralOutcome.reconcile({updatedA, outcomeB, outcomeB}));
    require(structuralOutcome.reconcile({outcomeB, updatedA}));
    require(structuralOutcome.reconcile({outcomeB}));
    require(structuralOutcome.reconcile({}));
    require(!structuralOutcome.reconcile({}));

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
    QSignalSpy coldInsert{&incremental, &QAbstractItemModel::rowsInserted};
    QSignalSpy coldReset{&incremental, &QAbstractItemModel::modelReset};
    incremental.reconcile({row(QStringLiteral("a"), QStringLiteral("r1")),
                           row(QStringLiteral("b"), QStringLiteral("r1")),
                           row(QStringLiteral("c"), QStringLiteral("r1"))});
    require(coldInsert.size() == 1 && coldReset.isEmpty());
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
    QSignalSpy sameCountReplacement{&incremental, &FeedRowsModel::countChanged};
    incremental.reconcile({row("different", "r1")});
    require(incrementalResets.size() == 2 && sameCountReplacement.isEmpty());
    FeedRowsModel rangeFeed;
    rangeFeed.reconcile({row("a","1"),row("b","1"),row("c","1"),row("d","1")});
    QAbstractItemModelTester rangeTester{&rangeFeed, QAbstractItemModelTester::FailureReportingMode::Fatal};
    const QPersistentModelIndex kept{rangeFeed.index(0,0)};
    QSignalSpy rangeInserts{&rangeFeed, &QAbstractItemModel::rowsInserted};
    QSignalSpy rangeRemoves{&rangeFeed, &QAbstractItemModel::rowsRemoved};
    rangeFeed.reconcile({row("a","1"),row("x","1"),row("y","1"),row("z","1"),row("d","1")});
    require(rangeRemoves.size() == 1 && rangeInserts.size() == 1);
    require(rangeFeed.count() == 5 && kept.isValid());
    require(rangeFeed.get(3).value(QStringLiteral("stableKey")) == QStringLiteral("z"));

    // A sparse intersection in an ordinary feed must not invalidate
    // its only retained delegate (e.g. a filtered or refreshed projection).
    FeedRowsModel sparse;
    QVariantList oldSparse, newSparse;
    for (int i = 0; i < 32; ++i) {
        oldSparse.append(row(QStringLiteral("old:%1").arg(i), "r1"));
        newSparse.append(row(QStringLiteral("new:%1").arg(i), "r1"));
    }
    newSparse[19] = oldSparse.at(7);
    sparse.reconcile(oldSparse);
    QAbstractItemModelTester sparseTester{&sparse, QAbstractItemModelTester::FailureReportingMode::Fatal};
    const QPersistentModelIndex survivor{sparse.index(7, 0)};
    QSignalSpy sparseResets{&sparse, &QAbstractItemModel::modelReset};
    require(sparse.reconcile(newSparse));
    require(sparseResets.isEmpty() && survivor.isValid() && survivor.row() == 19);
    require(survivor.data(FeedRowsModel::StableKeyRole).toString() == "old:7");
    require(sparse.count() == 32);
    for (int i = 0; i < sparse.count(); ++i)
        require(sparse.keyAt(i) == newSparse.at(i).toMap().value("stableKey").toString());

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
    // Adjacent revision-only updates publish one exact range; gaps and
    // presentation changes never cause unchanged rows to receive role signals.
    stableChanges.clear();
    stableSnapshot[4] = row("stable:4", "r3");
    stableSnapshot[5] = row("stable:5", "r3");
    stableSnapshot[7] = row("stable:7", "r3");
    require(!stableOrder.reconcile(stableSnapshot));
    require(stableChanges.size() == 2);
    require(stableChanges.at(0).at(0).value<QModelIndex>().row() == 4);
    require(stableChanges.at(0).at(1).value<QModelIndex>().row() == 5);
    require(stableChanges.at(1).at(0).value<QModelIndex>().row() == 7);
    require(stableChanges.at(1).at(1).value<QModelIndex>().row() == 7);
    require(stableChanges.at(0).at(2).value<QList<int>>() == QList<int>{FeedRowsModel::RevisionKeyRole});
    require(stableOrder.indexForKey("stable:7") == 7);
    require(stableOrder.indexForKey("STABLE:7") == -1);
    require(stableOrder.indexForKey("STABLE:7", true) == 7);
    require(stableOrder.keyAt(7) == "stable:7");
    require(stableOrder.keyAt(-1).isEmpty() && stableOrder.keyAt(2048).isEmpty());
    require(stableOrder.indexForKey({}) == -1);
    require(stableOrder.indexForKey("missing") == -1);
    // Equal media inputs must preserve the QML media projection; nested
    // metadata changes, reordering and coercive equality must not.
    using genycaster::presentation::detail::sameEmbedRecords;
    const QVariantMap embed{{"url", "https://example.invalid/art"},
                            {"meta", QVariantMap{{"width", 640}, {"height", 480}}}};
    const QVariantList embedList{embed, QVariantMap{{"url", "https://example.invalid/other"}}};
    require(sameEmbedRecords(embedList, embedList));
    require(sameEmbedRecords({}, {}));
    auto changedEmbed = embed;
    changedEmbed["meta"] = QVariantMap{{"width", 641}, {"height", 480}};
    require(!sameEmbedRecords(embedList, {changedEmbed, embedList.at(1)}));
    changedEmbed["meta"] = QVariantMap{{"width", QStringLiteral("640")}, {"height", 480}};
    require(!sameEmbedRecords(embedList, {changedEmbed, embedList.at(1)}));
    require(!sameEmbedRecords(embedList, {embedList.at(1), embedList.at(0)}));
    require(!sameEmbedRecords(embedList, {embedList.at(0)}));
    QVariant deep{1};
    for (int i = 0; i < 33; ++i) deep = QVariant::fromValue(QVariantList{deep});
    require(!sameEmbedRecords({deep}, {deep}));
    return 0;
}
