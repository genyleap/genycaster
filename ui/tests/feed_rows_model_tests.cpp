#include "../mobile/feed_rows_model.hpp"

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
    return 0;
}
