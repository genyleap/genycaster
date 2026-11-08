#include "../../ui/app/social/feed_rows.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QAbstractItemModelTester>

#include <cassert>

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
    assert(model.count() == n && model.rowCount() == n);
    // ModelTester performs global consistency checks on each insertion.
    // Attach after bulk fixture construction so this remains a bounded
    // 10k-row test, while still checking all targeted edits and moves.
    QAbstractItemModelTester tester{&model,
        QAbstractItemModelTester::FailureReportingMode::Fatal};
    assert(added.size() == n && counts.size() == n);
    assert(resets.isEmpty());
    const QVariantMap before = model.get(5000);
    assert(before.value(QStringLiteral("stableKey")) == QStringLiteral("0x5000"));
    assert(!model.get(-1).contains(QStringLiteral("stableKey")));
    assert(!model.get(n).contains(QStringLiteral("stableKey")));

    model.setProperty(5000, QStringLiteral("revisionKey"), QStringLiteral("liked"));
    assert(changed.size() == 1);
    const auto roles = changed.first().at(2).value<QList<int>>();
    assert(roles.size() == 1 && roles.first() == FeedRowsModel::RevisionKeyRole);
    assert(changed.first().at(0).value<QModelIndex>().row() == 5000);
    model.setProperty(5000, QStringLiteral("revisionKey"), QStringLiteral("liked"));
    model.setProperty(5000, QStringLiteral("mediaLayoutKind"), QStringLiteral("text"));
    model.setProperty(5000, QStringLiteral("stableKey"), QStringLiteral("changed"));
    model.setProperty(n + 1, QStringLiteral("revisionKey"), QStringLiteral("invalid"));
    assert(changed.size() == 1);
    assert(model.get(5000).value(QStringLiteral("stableKey")) == QStringLiteral("0x5000"));
    model.setProperty(5000, QStringLiteral("mediaLayoutKind"), QStringLiteral("single-image"));
    assert(changed.size() == 2);
    const auto mediaRoles = changed.last().at(2).value<QList<int>>();
    assert(mediaRoles.size() == 1 && mediaRoles.first() == FeedRowsModel::MediaLayoutKindRole);
    assert(added.size() == n && removed.isEmpty() && moved.isEmpty());

    model.move(9000, 100, 1);
    assert(moved.size() == 1 && model.get(100).value(QStringLiteral("stableKey"))
        == QStringLiteral("0x9000"));
    assert(model.get(101).value(QStringLiteral("stableKey")) == QStringLiteral("0x100"));
    model.move(100, 9000, 1);
    assert(moved.size() == 2 && model.get(9000).value(QStringLiteral("stableKey"))
        == QStringLiteral("0x9000"));
    assert(resets.isEmpty());
    model.remove(9000);
    assert(model.count() == n - 1 && removed.size() == 1);
    assert(!model.get(9000).value(QStringLiteral("stableKey")).toString().isEmpty());

    model.insert(0, {
        {QStringLiteral("stableKey"), QStringLiteral("0xnew")},
        {QStringLiteral("revisionKey"), QStringLiteral("incoming")},
        {QStringLiteral("mediaLayoutKind"), QStringLiteral("gallery")},
    });
    assert(model.count() == n);
    assert(model.get(0).value(QStringLiteral("stableKey")) == QStringLiteral("0xnew"));
    assert(model.get(1).value(QStringLiteral("stableKey")) == QStringLiteral("0x0"));
    assert(resets.isEmpty());
    model.clear();
    assert(model.count() == 0 && resets.size() == 1);
    assert(counts.size() == n + 3);

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
    assert(magazine.count() > 0 && magazine.count() < 60);
    const auto top = magazine.geometryAt(0);
    const auto nextInLane = magazine.geometryAt(3);
    assert(top.value(QStringLiteral("x")) == nextInLane.value(QStringLiteral("x")));
    const qreal previousLaneY = nextInLane.value(QStringLiteral("y")).toDouble();
    assert(magazine.retainHeight(QStringLiteral("cast:0"), QStringLiteral("r1"),
                                 320.0, 900.0, true));
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, false);
    const auto grown = magazine.geometryAt(3);
    assert(grown.value(QStringLiteral("y")).toDouble() > previousLaneY + 600.0);
    assert(magazine.geometryAt(1).value(QStringLiteral("y")) ==
           top.value(QStringLiteral("y")));
    magazine.setViewport(8000.0, 720.0, 280.0, 950.0, true);
    assert(magazine.count() > 0 && magazine.count() < 70);
    const int distantSource = magazine.data(magazine.index(0, 0),
                              MasonryWindowModel::SourceIndexRole).toInt();
    assert(distantSource > 20);
    magazine.setViewport(0.0, 720.0, 280.0, 950.0, true);
    assert(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 900.0);
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, true);
    assert(magazine.geometryAt(0).value(QStringLiteral("x")).toDouble() >
           magazine.geometryAt(1).value(QStringLiteral("x")).toDouble());
    casts.setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("r2"));
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, true);
    assert(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 900.0);
    // Viewer revisions preserve geometry until genuinely changed content
    // reports a new measured size.
    assert(magazine.retainHeight(QStringLiteral("cast:0"), QStringLiteral("r2"),
                                 320.0, 420.0, true));
    magazine.relayout(3, 320.0, 12.0, 12.0, 12.0, true);
    assert(magazine.geometryAt(0).value(QStringLiteral("height")).toDouble() == 420.0);
    magazine.setViewport(0.0, 720.0, 280.0, 950.0, false);
    assert(magazine.count() == 0);

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
    assert(incremental.count() == 4 && incrementalChanges.size() == 1);
    assert(incrementalResets.isEmpty());
    incremental.reconcile({row(QStringLiteral("c"), QStringLiteral("r1")),
                           row(QStringLiteral("a"), QStringLiteral("r1")),
                           row(QStringLiteral("b"), QStringLiteral("r2"))});
    assert(incremental.count() == 3);
    assert(incremental.get(0).value(QStringLiteral("stableKey")).toString()
           == QStringLiteral("c"));
    assert(incrementalMoves.size() >= 1 && incrementalResets.isEmpty());
    incremental.reconcile({row(QStringLiteral("other"), QStringLiteral("r1"))});
    assert(incrementalResets.size() == 1);
    return 0;
}
