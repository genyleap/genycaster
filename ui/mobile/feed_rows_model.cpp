#include "feed_rows_model.hpp"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <utility>

FeedRowsModel::FeedRowsModel(QObject* const parent) : QAbstractListModel(parent) {}

int FeedRowsModel::count() const noexcept { return static_cast<int>(m_rows.size()); }

int FeedRowsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant FeedRowsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.parent().isValid() ||
        index.row() < 0 || index.row() >= count() || index.column() != 0) {
        return {};
    }
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case StableKeyRole: return row.stableKey;
    case RevisionKeyRole: return row.revisionKey;
    case PresentationKindRole: return row.presentationKind;
    case MediaLayoutKindRole: return row.mediaLayoutKind;
    default: return {};
    }
}

QHash<int, QByteArray> FeedRowsModel::roleNames() const
{
    return {
        {StableKeyRole, QByteArrayLiteral("stableKey")},
        {RevisionKeyRole, QByteArrayLiteral("revisionKey")},
        {PresentationKindRole, QByteArrayLiteral("presentationKind")},
        {MediaLayoutKindRole, QByteArrayLiteral("mediaLayoutKind")},
    };
}

FeedRowsModel::Row FeedRowsModel::fromMap(const QVariantMap& value)
{
    return Row{
        .stableKey = value.value(QStringLiteral("stableKey")).toString(),
        .revisionKey = value.value(QStringLiteral("revisionKey")).toString(),
        .presentationKind = value.value(QStringLiteral("presentationKind")).toString(),
        .mediaLayoutKind = value.value(QStringLiteral("mediaLayoutKind")).toString(),
    };
}

QVariantMap FeedRowsModel::toMap(const Row& row)
{
    return {
        {QStringLiteral("stableKey"), row.stableKey},
        {QStringLiteral("revisionKey"), row.revisionKey},
        {QStringLiteral("presentationKind"), row.presentationKind},
        {QStringLiteral("mediaLayoutKind"), row.mediaLayoutKind},
    };
}

QVariantMap FeedRowsModel::get(const int index) const
{
    if (index < 0 || index >= count()) return {};
    return toMap(m_rows.at(index));
}

void FeedRowsModel::clear()
{
    if (m_rows.isEmpty()) return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

void FeedRowsModel::append(const QVariantMap& row) { insert(count(), row); }

void FeedRowsModel::insert(const int index, const QVariantMap& row)
{
    if (index < 0 || index > count()) return;
    const Row converted = fromMap(row);
    if (converted.stableKey.isEmpty()) return;
    beginInsertRows({}, index, index);
    m_rows.insert(index, converted);
    endInsertRows();
    emit countChanged();
}

void FeedRowsModel::remove(const int index)
{
    if (index < 0 || index >= count()) return;
    beginRemoveRows({}, index, index);
    m_rows.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

void FeedRowsModel::move(const int from, const int to, const int amount)
{
    if (amount <= 0 || from < 0 || to < 0 ||
        from + amount > count() || to + amount > count() ||
        (from == to)) {
        return;
    }
    const int destinationChild = from < to ? to + amount : to;
    if (!beginMoveRows({}, from, from + amount - 1, {}, destinationChild)) return;
    QList<Row> moved{};
    moved.reserve(amount);
    for (int offset = 0; offset < amount; ++offset)
        moved.append(m_rows.at(from + offset));
    for (int offset = 0; offset < amount; ++offset)
        m_rows.removeAt(from);
    for (int offset = 0; offset < amount; ++offset)
        m_rows.insert(to + offset, std::move(moved[offset]));
    endMoveRows();
}

void FeedRowsModel::setProperty(const int index, const QString& role, const QVariant& value)
{
    if (index < 0 || index >= count()) return;
    Row& row = m_rows[index];
    const QString updated = value.toString();
    int roleId{};
    QString* destination{};
    if (role == QStringLiteral("revisionKey")) {
        roleId = RevisionKeyRole;
        destination = &row.revisionKey;
    } else if (role == QStringLiteral("presentationKind")) {
        roleId = PresentationKindRole;
        destination = &row.presentationKind;
    } else if (role == QStringLiteral("mediaLayoutKind")) {
        roleId = MediaLayoutKindRole;
        destination = &row.mediaLayoutKind;
    } else {
        // Stable identity is immutable. A new ID requires a structural update.
        return;
    }
    if (*destination == updated) return;
    *destination = updated;
    const QModelIndex changed = createIndex(index, 0);
    emit dataChanged(changed, changed, {roleId});
}
