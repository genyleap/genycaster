#include "feed_rows.hpp"

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QTimer>
#include <QSet>

#include <algorithm>
#include <cmath>
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
    // from/to/amount are QML-callable signed integers. Validate without
    // first adding them: INT_MAX + positive amount is signed-overflow UB,
    // even when the resulting range would be rejected afterwards.
    const int rowCount = count();
    if (amount <= 0 || from < 0 || to < 0 || from >= rowCount
        || to >= rowCount || amount > rowCount - from
        || amount > rowCount - to || from == to) {
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

bool FeedRowsModel::reconcile(const QVariantList& rows)
{
    QList<Row> incoming{};
    incoming.reserve(rows.size());
    QHash<QString, bool> seen{};
    // Reconciliation always knows its upper bound. Reserving once avoids
    // repeated hash-table reallocation as long feeds are hydrated.
    seen.reserve(rows.size());
    for (const QVariant& value : rows) {
        Row row = fromMap(value.toMap());
        if (row.stableKey.isEmpty() || seen.contains(row.stableKey)) continue;
        seen.insert(row.stableKey, true);
        incoming.append(std::move(row));
    }
    if (incoming.isEmpty()) {
        const bool structureChanged = !m_rows.isEmpty();
        clear();
        return structureChanged;
    }

    // When hydration or reaction updates preserve stable row order, avoid
    // constructing the old-key hash and running the general
    // insert/move search when only a few primitive roles changed. Notifications
    // remain strictly per-role so a like cannot rebuild sibling media.
    const auto updateRoles = [this](const int index, const Row& desired) {
        Row& current = m_rows[index];
        QList<int> changedRoles{};
        if (current.revisionKey != desired.revisionKey) {
            current.revisionKey = desired.revisionKey;
            changedRoles.append(RevisionKeyRole);
        }
        if (current.presentationKind != desired.presentationKind) {
            current.presentationKind = desired.presentationKind;
            changedRoles.append(PresentationKindRole);
        }
        if (current.mediaLayoutKind != desired.mediaLayoutKind) {
            current.mediaLayoutKind = desired.mediaLayoutKind;
            changedRoles.append(MediaLayoutKindRole);
        }
        if (!changedRoles.isEmpty())
            emit dataChanged(createIndex(index, 0), createIndex(index, 0), changedRoles);
    };
    if (incoming.size() == m_rows.size()
        && std::equal(incoming.cbegin(), incoming.cend(), m_rows.cbegin(),
                      [](const Row& next, const Row& current) {
                          return next.stableKey == current.stableKey;
                      })) {
        for (int index = 0; index < incoming.size(); ++index)
            updateRoles(index, incoming.at(index));
        return false;
    }

    // Provider/audience switches with almost no common IDs should not
    // dispatch thousands of insert/move notifications to the scene graph.
    QHash<QString, bool> oldKeys{};
    oldKeys.reserve(m_rows.size());
    for (const Row& row : std::as_const(m_rows)) oldKeys.insert(row.stableKey, true);
    int retained = 0;
    int displaced = 0;
    for (int i = 0; i < incoming.size(); ++i) {
        if (oldKeys.contains(incoming.at(i).stableKey)) ++retained;
        if (i >= m_rows.size() || m_rows.at(i).stableKey != incoming.at(i).stableKey)
            ++displaced;
    }
    if (m_rows.isEmpty()
        || retained * 4 < std::min(m_rows.size(), incoming.size())
        || (incoming.size() > 512 && displaced > incoming.size() * 3 / 4)) {
        beginResetModel();
        m_rows = std::move(incoming);
        endResetModel();
        emit countChanged();
        return true;
    }

    bool countChangedValue = false;
    for (int i = 0; i < incoming.size(); ++i) {
        const Row& desired = incoming.at(i);
        if (i >= m_rows.size() || m_rows.at(i).stableKey != desired.stableKey) {
            int found = -1;
            for (int search = i + 1; search < m_rows.size(); ++search) {
                if (m_rows.at(search).stableKey == desired.stableKey) {
                    found = search;
                    break;
                }
            }
            if (found >= 0) {
                if (beginMoveRows({}, found, found, {}, i)) {
                    m_rows.move(found, i);
                    endMoveRows();
                }
            } else {
                beginInsertRows({}, i, i);
                m_rows.insert(i, desired);
                endInsertRows();
                countChangedValue = true;
                continue;
            }
        }
        updateRoles(i, desired);
    }
    if (m_rows.size() > incoming.size()) {
        beginRemoveRows({}, incoming.size(), m_rows.size() - 1);
        m_rows.resize(incoming.size());
        endRemoveRows();
        countChangedValue = true;
    }
    if (countChangedValue) emit countChanged();
    return true;
}

MasonryWindowModel::MasonryWindowModel(QObject* const parent) : QAbstractListModel(parent) {}

int MasonryWindowModel::count() const noexcept { return static_cast<int>(m_window.size()); }

int MasonryWindowModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant MasonryWindowModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.parent().isValid() || index.column() != 0
        || index.row() < 0 || index.row() >= count()) return {};
    const Slot& slot = m_window.at(index.row());
    switch (role) {
    case SourceIndexRole: return slot.sourceIndex;
    case StableKeyRole: return slot.key;
    case RevisionKeyRole: return slot.revision;
    case PresentationKindRole: return slot.kind;
    case MediaLayoutKindRole: return slot.media;
    case CardXRole: return slot.x;
    case CardYRole: return slot.y;
    case CardHeightRole: return slot.height;
    default: return {};
    }
}

QHash<int, QByteArray> MasonryWindowModel::roleNames() const
{
    return {
        {SourceIndexRole, QByteArrayLiteral("sourceIndex")},
        {StableKeyRole, QByteArrayLiteral("stableKey")},
        {RevisionKeyRole, QByteArrayLiteral("revisionKey")},
        {PresentationKindRole, QByteArrayLiteral("presentationKind")},
        {MediaLayoutKindRole, QByteArrayLiteral("mediaLayoutKind")},
        {CardXRole, QByteArrayLiteral("cardX")},
        {CardYRole, QByteArrayLiteral("cardY")},
        {CardHeightRole, QByteArrayLiteral("cardHeight")},
    };
}

FeedRowsModel* MasonryWindowModel::sourceModel() const noexcept { return m_source.data(); }

void MasonryWindowModel::setSourceModel(FeedRowsModel* const model)
{
    if (m_source == model) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = model;
    m_forceRepack = true;
    if (model) {
        connect(model, &QAbstractItemModel::rowsInserted, this,
            [this] { scheduleRelayout(); });
        connect(model, &QAbstractItemModel::rowsRemoved, this,
            [this] { scheduleRelayout(); });
        connect(model, &QAbstractItemModel::rowsMoved, this,
            [this] { scheduleRelayout(); });
        connect(model, &QAbstractItemModel::dataChanged, this,
            &MasonryWindowModel::sourceDataChanged);
        connect(model, &QAbstractItemModel::modelReset, this,
            [this] { m_forceRepack = true; scheduleRelayout(); });
        connect(model, &QObject::destroyed, this, [this] {
            m_source = nullptr;
            m_forceRepack = true;
            scheduleRelayout();
            emit sourceModelChanged();
        });
    }
    emit sourceModelChanged();
    scheduleRelayout();
}

qreal MasonryWindowModel::rowsBottom() const noexcept { return m_bottom; }

qreal MasonryWindowModel::shortestColumnBottom() const noexcept
{
    return m_shortestBottom;
}

void MasonryWindowModel::scheduleRelayout()
{
    m_sourceGeometryDirty = true;
    if (m_relayoutScheduled) return;
    m_relayoutScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_relayoutScheduled = false;
        if (m_sourceGeometryDirty)
            emit geometryInvalidated();
    });
}

void MasonryWindowModel::sourceDataChanged(const QModelIndex& first,
                                          const QModelIndex& last,
                                          const QList<int>& roles)
{
    // Revision includes viewer state AND structural hydration. Keep the
    // existing reservation only provisionally; the live card must still
    // measure its new content. Never bypass a pending structural layout.
    if (!m_source || m_sourceGeometryDirty || m_forceRepack
        || roles.size() != 1 || roles.front() != FeedRowsModel::RevisionKeyRole
        || !first.isValid() || !last.isValid()
        || first.row() < 0 || last.row() < first.row()
        || last.row() >= m_geometry.size()
        || m_source->count() != m_geometry.size()) {
        scheduleRelayout();
        return;
    }
    // Validate the entire changed range before touching any slot. A cached
    // height for a returning revision can require genuine reflow.
    for (int i = first.row(); i <= last.row(); ++i) {
        const auto& row = m_source->m_rows.at(i);
        const auto& slot = m_geometry.at(i);
        const QString id = heightKey(slot.key, row.revisionKey, m_cardWidth);
        const auto transient = m_resizeSamples.constFind(id);
        const auto measured = m_heightSamples.constFind(id);
        if (row.stableKey != slot.key
            || (transient != m_resizeSamples.cend() && *transient != slot.height)
            || (transient == m_resizeSamples.cend()
                && measured != m_heightSamples.cend() && *measured != slot.height)) {
            scheduleRelayout();
            return;
        }
    }
    for (int i = first.row(); i <= last.row(); ++i)
        m_geometry[i].revision = m_source->m_rows.at(i).revisionKey;

    // Only notify already admitted rows. Offscreen slots pick up the new
    // revision on entry, with no creation, lane changes or viewport restore.
    for (int i = 0; i < m_window.size(); ++i) {
        auto& slot = m_window[i];
        if (slot.sourceIndex < first.row() || slot.sourceIndex > last.row())
            continue;
        const auto& desired = m_geometry.at(slot.sourceIndex);
        if (slot.key != desired.key) {
            scheduleRelayout();
            return;
        }
        if (slot.revision == desired.revision)
            continue;
        slot.revision = desired.revision;
        emit dataChanged(createIndex(i, 0), createIndex(i, 0), {RevisionKeyRole});
    }
}

QString MasonryWindowModel::heightKey(const QString& key, const QString& revision,
                                    const qreal width) const
{
    return key + QLatin1Char('|') + revision + QLatin1Char('|')
        + QString::number(width, 'f', 2);
}

qreal MasonryWindowModel::fallbackHeight(const Slot& slot) const
{
    if (slot.kind == QStringLiteral("profile")) return 72.0;
    if (slot.media == QStringLiteral("gallery")) return std::max(360.0, m_cardWidth * 1.25);
    if (slot.media == QStringLiteral("single-image")) return std::max(260.0, m_cardWidth * 1.05);
    return slot.kind == QStringLiteral("developer") ? 350.0 : 210.0;
}

void MasonryWindowModel::requestRepack() noexcept
{
    // Only an explicit presentation reset may discard admitted lane identity.
    m_forceRepack = true;
}

bool MasonryWindowModel::retainHeight(const QString& key, const QString& revision,
                                      const qreal width, const qreal height,
                                      const bool measured, const bool transient)
{
    if (key.isEmpty() || !std::isfinite(width) || !std::isfinite(height) || height <= 0
        || width <= 0) return false;
    const QString id = heightKey(key, revision, width);
    if (transient) {
        if (std::abs(m_resizeSampleWidth - width) >= 0.01) {
            m_resizeSamples.clear();
            m_resizeSampleWidth = width;
        }
        const auto prior = m_resizeSamples.constFind(id);
        if (prior != m_resizeSamples.cend() && std::abs(*prior - height) < 1.0)
            return false;
        if (m_resizeSamples.size() >= 160 && prior == m_resizeSamples.cend())
            return false;
        m_resizeSamples.insert(id, height);
        return true;
    }
    const auto existing = m_heightSamples.constFind(id);
    if (existing != m_heightSamples.cend()
        && m_exactSamples.value(id) && !measured) return false;
    if (existing != m_heightSamples.cend() && std::abs(*existing - height) < 1.0) {
        if (measured) m_exactSamples[id] = true;
        return false;
    }
    if (existing == m_heightSamples.cend()) {
        m_sampleOrder.append(id);
        if (m_sampleOrder.size() > 600) {
            const QString expired = m_sampleOrder.takeFirst();
            m_heightSamples.remove(expired);
            m_exactSamples.remove(expired);
        }
    }
    m_heightSamples[id] = height;
    m_exactSamples[id] = measured;
    // A new revision needs its own cached sample even when its measured
    // geometry is unchanged. Storing that sample alone is not a reflow.
    if (!m_sourceGeometryDirty && !m_forceRepack && width == m_cardWidth) {
        const auto found = m_indexByKey.constFind(key);
        if (found != m_indexByKey.cend()) {
            const auto& slot = m_geometry.at(*found);
            if (slot.revision == revision && slot.height == height
                && !m_resizeSamples.contains(id))
                return false;
        }
    }
    return true;
}

void MasonryWindowModel::relayout(const int columns, const qreal cardWidth,
                                 const qreal gap, const qreal top,
                                 const qreal leadingInset, const bool rtl,
                                 const bool deferWindowUpdate)
{
    if (!(cardWidth > 0) || !std::isfinite(cardWidth)) return;
    const int nextColumns = std::clamp(columns, 1, 8);
    const bool packAgain = m_forceRepack || m_columns != nextColumns;
    // Preserve already measured geometry as a provisional estimate even
    // when width changes. Live delegates replace it with exact new heights.
    // Falling back to generic card heights on every resize collapses lanes.
    QHash<QString, qreal> previousHeights{};
    previousHeights.reserve(m_geometry.size());
    for (const Slot& slot : std::as_const(m_geometry))
        previousHeights.insert(slot.key, slot.height);
    QHash<QString, int> previousColumns{};
    if (!packAgain) {
        previousColumns.reserve(m_geometry.size());
        for (const Slot& slot : std::as_const(m_geometry)) {
            if (slot.laneCommitted)
                previousColumns.insert(slot.key, slot.column);
        }
    }
    m_columns = nextColumns;
    m_cardWidth = cardWidth;
    m_gap = std::max(0.0, gap);
    m_top = std::max(0.0, top);
    m_inset = std::max(0.0, leadingInset);
    m_rtl = rtl;
    m_forceRepack = false;
    QVector<qreal> bottoms(m_columns, m_top);
    m_lanes = QVector<QVector<int>>(m_columns);
    QVector<Slot> geometry{};
    QHash<QString, int> indexByKey{};
    if (m_source) {
        geometry.reserve(m_source->count());
        indexByKey.reserve(m_source->count());
    }
    for (int index = 0; m_source && index < m_source->count(); ++index) {
        // Same feature/translation unit: read primitive roles directly.
        // Constructing a QVariantMap for every row during each relayout
        // created avoidable GUI-thread allocations in long timelines.
        const FeedRowsModel::Row& data = m_source->m_rows.at(index);
        Slot slot{};
        slot.key = data.stableKey;
        slot.revision = data.revisionKey;
        slot.kind = data.presentationKind;
        slot.media = data.mediaLayoutKind;
        indexByKey.insert(slot.key, index);
        slot.sourceIndex = index;
        const auto old = previousColumns.constFind(slot.key);
        if (old != previousColumns.cend() && *old >= 0 && *old < m_columns) {
            slot.column = *old;
            slot.laneCommitted = true;
        } else {
            slot.column = static_cast<int>(std::min_element(bottoms.cbegin(),
                                                            bottoms.cend()) - bottoms.cbegin());
        }
        const QString id = heightKey(slot.key, slot.revision, m_cardWidth);
        const auto sample = m_heightSamples.constFind(id);
        // Engagement/viewer revisions do not imply a visual-height change.
        // Hold the same cast's previous geometry until its actual delegate
        // reports a new painted height instead of flashing to a placeholder.
        const auto resizeSample = m_resizeSamples.constFind(id);
        slot.height = resizeSample != m_resizeSamples.cend() ? *resizeSample
            : sample != m_heightSamples.cend() ? *sample
            : previousHeights.value(slot.key, fallbackHeight(slot));
        slot.y = bottoms[slot.column];
        const int visualColumn = m_rtl ? m_columns - 1 - slot.column : slot.column;
        slot.x = m_inset + visualColumn * (m_cardWidth + m_gap);
        bottoms[slot.column] += slot.height + m_gap;
        m_lanes[slot.column].append(index);
        geometry.append(std::move(slot));
    }
    m_geometry = std::move(geometry);
    m_indexByKey = std::move(indexByKey);
    m_sourceGeometryDirty = false;
    m_bottom = m_geometry.isEmpty() ? std::max(0.0, m_top - m_gap)
        : *std::max_element(bottoms.cbegin(), bottoms.cend());
    m_shortestBottom = m_geometry.size() >= m_columns
        ? *std::min_element(bottoms.cbegin(), bottoms.cend())
        : std::max(0.0, m_top - m_gap);
    emit geometryChanged();
    // QML restores the stable scroll anchor before committing admissions.
    // Avoid evicting delegates against the pre-relayout viewport.
    if (!deferWindowUpdate) synchronizeWindow();
}

int MasonryWindowModel::laneAt(const qreal x) const noexcept
{
    // Cast only after bounding the floating coordinate. C++ does not define
    // a floating-to-int conversion if the result is outside int's range.
    const qreal position = (x - m_inset) / std::max(1.0, m_cardWidth + m_gap);
    int visual = 0;
    if (position >= static_cast<qreal>(m_columns - 1))
        visual = m_columns - 1;
    else if (position > 0.0)
        visual = static_cast<int>(position); // 0 < position < 7
    return m_rtl ? m_columns - visual - 1 : visual;
}

int MasonryWindowModel::indexAt(const qreal x, const qreal y) const
{
    if (m_geometry.isEmpty() || !std::isfinite(x) || !std::isfinite(y))
        return -1;
    const auto probe = [this, y](const int column) {
        const auto& lane = m_lanes.at(column);
        const auto it = std::lower_bound(lane.cbegin(), lane.cend(), y,
            [this](const int index, const qreal position) {
                const Slot& slot = m_geometry.at(index);
                return slot.y + slot.height < position;
            });
        if (it != lane.cend() && m_geometry.at(*it).y <= y)
            return *it;
        return -1;
    };
    const int nearest = probe(laneAt(x));
    if (nearest >= 0) return nearest;
    for (int lane = 0; lane < m_columns; ++lane) {
        const int index = probe(lane);
        if (index >= 0) return index;
    }
    return -1;
}

QVariantMap MasonryWindowModel::geometryAt(const int sourceIndex) const
{
    if (sourceIndex < 0 || sourceIndex >= m_geometry.size()) return {};
    const Slot& slot = m_geometry.at(sourceIndex);
    return {{QStringLiteral("x"), slot.x}, {QStringLiteral("y"), slot.y},
            {QStringLiteral("height"), slot.height}, {QStringLiteral("stableKey"), slot.key}};
}

QVariantMap MasonryWindowModel::geometryForKey(const QString& key) const
{
    const auto found = m_indexByKey.constFind(key);
    return found == m_indexByKey.cend() ? QVariantMap{} : geometryAt(*found);
}

int MasonryWindowModel::windowIndexForSource(const int sourceIndex) const
{
    for (int index = 0; index < m_window.size(); ++index)
        if (m_window.at(index).sourceIndex == sourceIndex) return index;
    return -1;
}

void MasonryWindowModel::setViewport(const qreal top, const qreal height,
                                    const qreal enteringBuffer,
                                    const qreal retentionBuffer, const bool active,
                                    const bool retainExisting)
{
    m_viewportTop = std::isfinite(top) ? top : 0.0;
    m_viewportHeight = std::max(0.0, std::isfinite(height) ? height : 0.0);
    m_buffer = std::max(0.0, std::isfinite(retentionBuffer) ? retentionBuffer : 0.0);
    m_enterBuffer = std::max(0.0, std::isfinite(enteringBuffer) ? enteringBuffer : 0.0);
    m_active = active;
    if (!active || (!retainExisting && m_retainExisting)) {
        m_resizeSamples.clear();
        m_resizeSampleWidth = 0;
    }
    m_retainExisting = active && retainExisting;
    synchronizeWindow();
}

void MasonryWindowModel::synchronizeWindow()
{
    QVector<int> admitted{};
    if (m_active && m_viewportHeight > 0) {
        const qreal minimum = m_viewportTop - m_buffer;
        const qreal maximum = m_viewportTop + m_viewportHeight + m_buffer;
        const qreal enterTop = m_viewportTop - m_enterBuffer;
        const qreal enterBottom = m_viewportTop + m_viewportHeight + m_enterBuffer;
        QHash<QString, bool> retained{};
        for (const Slot& slot : std::as_const(m_window)) retained.insert(slot.key, true);
        for (const auto& lane : std::as_const(m_lanes)) {
            auto it = std::lower_bound(lane.cbegin(), lane.cend(), minimum,
                [this](const int index, const qreal position) {
                    const Slot& slot = m_geometry.at(index);
                    return slot.y + slot.height < position;
                });
            for (; it != lane.cend() && m_geometry.at(*it).y <= maximum; ++it) {
                const Slot& slot = m_geometry.at(*it);
                if ((slot.y + slot.height >= enterTop && slot.y <= enterBottom)
                    || retained.contains(slot.key)) admitted.append(*it);
            }
        }
        // During the short resize transaction retain the existing delegate
        // identities, but still admit newly visible cards. Removed source
        // rows are never retained. Normal bounded eviction resumes on settle.
        if (m_retainExisting) {
            constexpr qsizetype resizeRetentionLimit{160};
            for (const Slot& slot : std::as_const(m_window)) {
                if (admitted.size() >= resizeRetentionLimit) break;
                const auto found = m_indexByKey.constFind(slot.key);
                if (found != m_indexByKey.cend()
                    && std::find(admitted.cbegin(), admitted.cend(), *found) == admitted.cend())
                    admitted.append(*found);
            }
        }
        std::sort(admitted.begin(), admitted.end());
        admitted.erase(std::unique(admitted.begin(), admitted.end()), admitted.end());
    }
    // Evict departed ranges before inserting replacements. This avoids
    // temporarily holding both entire viewport windows during fast jumps.
    // A retained stable key keeps its persistent index and delegate.
    const auto previousCount = m_window.size();
    QSet<QString> desiredKeys;
    desiredKeys.reserve(admitted.size());
    for (const int sourceIndex : std::as_const(admitted))
        desiredKeys.insert(m_geometry.at(sourceIndex).key);
    for (int end = static_cast<int>(m_window.size()) - 1; end >= 0;) {
        if (desiredKeys.contains(m_window.at(end).key)) {
            --end;
            continue;
        }
        int first = end;
        while (first > 0 && !desiredKeys.contains(m_window.at(first - 1).key))
            --first;
        beginRemoveRows({}, first, end);
        m_window.remove(first, end - first + 1);
        endRemoveRows();
        end = first - 1;
    }
    QSet<QString> existingKeys;
    existingKeys.reserve(m_window.size());
    for (const auto& slot : std::as_const(m_window))
        existingKeys.insert(slot.key);
    int index = 0;
    while (index < admitted.size()) {
        const int sourceIndex = admitted.at(index);
        // Once admitted, a cast keeps its lane across hydration, eviction,
        // reverse scrolling and engagement revisions. Unseen rows can still
        // fill the shorter lane as real heights replace initial estimates.
        Slot& desired = m_geometry[sourceIndex];
        desired.laneCommitted = true;
        if (index >= m_window.size() || m_window.at(index).key != desired.key) {
            int found = -1;
            for (int search = index + 1; search < m_window.size(); ++search) {
                if (m_window.at(search).key == desired.key) {
                    found = search;
                    break;
                }
            }
            if (found >= 0) {
                if (beginMoveRows({}, found, found, {}, index)) {
                    m_window.move(found, index);
                    endMoveRows();
                }
            } else {
                // Batch consecutive new identities into one model signal.
                // QML still receives the exact roles and order for every row.
                int end = index;
                while (end + 1 < admitted.size()
                       && !existingKeys.contains(m_geometry.at(admitted.at(end + 1)).key))
                    ++end;
                beginInsertRows({}, index, end);
                for (int i = index; i <= end; ++i) {
                    auto& incoming = m_geometry[admitted.at(i)];
                    incoming.laneCommitted = true;
                    m_window.insert(i, incoming);
                }
                endInsertRows();
                index = end + 1;
                continue;
            }
        }
        Slot& current = m_window[index];
        QList<int> changedRoles{};
        if (current.sourceIndex != desired.sourceIndex) {
            changedRoles.append(SourceIndexRole);
        }
        if (current.revision != desired.revision) {
            changedRoles.append(RevisionKeyRole);
        }
        if (current.kind != desired.kind) {
            changedRoles.append(PresentationKindRole);
        }
        if (current.media != desired.media) {
            changedRoles.append(MediaLayoutKindRole);
        }
        if (std::abs(current.x - desired.x) >= 0.5) {
            changedRoles.append(CardXRole);
        }
        if (std::abs(current.y - desired.y) >= 0.5) {
            changedRoles.append(CardYRole);
        }
        if (std::abs(current.height - desired.height) >= 0.5) {
            changedRoles.append(CardHeightRole);
        }
        if (!changedRoles.isEmpty()) {
            current = desired;
            // Geometry corrections must not invalidate unchanged cast payloads
            // and rebuild their rich text/media bindings on the GUI thread.
            emit dataChanged(createIndex(index, 0), createIndex(index, 0), changedRoles);
        }
        ++index;
    }
    if (index < m_window.size()) {
        beginRemoveRows({}, index, m_window.size() - 1);
        m_window.resize(index);
        endRemoveRows();
    }
    if (previousCount != m_window.size())
        emit countChanged();
}
