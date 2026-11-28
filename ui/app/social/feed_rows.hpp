/*!
 * @file        ui/app/social/feed_rows.hpp
 * @brief       Stable Qt feed row model.
 *
 * @details
 * Adapts canonical feed snapshots to a QAbstractListModel, preserving
 * stable row identity and incremental updates across Qt views.
 *
 * @author      Kambiz Asadzadeh
 * @since       20 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Qt MOC/QML textual compatibility boundary: QAbstractListModel is a Qt-owned
// presentation interface, not a C++ domain/application named-module contract.
#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QHash>
#include <QPointer>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * @brief One stable-ID Qt Quick feed row-order model with targeted role updates.
 *
 * The owning QML FeedSurface keeps heavyweight cast metadata in its keyed
 * payload store. This model holds only the four primitive delegate roles.
 * Qt GUI-thread ownership is exclusive: workers never touch this QObject.
 */
class FeedRowsModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(FeedRowsModel)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit FeedRowsModel(QObject* parent = nullptr);

    enum Role : int {
        StableKeyRole = Qt::UserRole + 1,
        RevisionKeyRole,
        PresentationKindRole,
        MediaLayoutKindRole,
    };

    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /** @brief Returns one shallow primitive row, or an empty map for an invalid index. */
    Q_INVOKABLE [[nodiscard]] QVariantMap get(int index) const;
    /** @brief Finds an anchor without constructing a QVariantMap for each row. */
    Q_INVOKABLE [[nodiscard]] int indexForKey(const QString& key, bool ignoreCase = false) const;
    Q_INVOKABLE [[nodiscard]] QString keyAt(int index) const;

    /** @brief Clears all visible rows; only broad source replacement should use this. */
    Q_INVOKABLE void clear();

    /** @brief Appends a primitive role map and emits a single-row insert signal. */
    Q_INVOKABLE void append(const QVariantMap& row);

    /** @brief Inserts a primitive role map; invalid positions have no effect. */
    Q_INVOKABLE void insert(int index, const QVariantMap& row);

    /** @brief Removes one existing visible row; invalid positions have no effect. */
    Q_INVOKABLE void remove(int index);

    /** @brief Moves count rows to final index to, preserving stable delegate identity. */
    Q_INVOKABLE void move(int from, int to, int count);

    /** @brief Updates one mutable role only if its primitive value changes. */
    Q_INVOKABLE void setProperty(int index, const QString& role, const QVariant& value);

    /** @brief Reconciles a snapshot; returns true if stable row order/count changed. */
    Q_INVOKABLE bool reconcile(const QVariantList& rows);

signals:
    void countChanged();

private:
    friend class MasonryWindowModel;
    struct Row {
        QString stableKey{};
        QString revisionKey{};
        QString presentationKind{};
        QString mediaLayoutKind{};
    };
    [[nodiscard]] static Row fromMap(const QVariantMap& row);
    [[nodiscard]] static QVariantMap toMap(const Row& row);

    QList<Row> m_rows{};
};

/*!
 * @brief Virtualized, measured multi-column projection of a stable feed model.
 *
 * The source preserves canonical row identity/order. This GUI-thread adapter
 * owns only lightweight layout slots and exposes a bounded window of delegates.
 * Offscreen slots remain numeric geometry, never QQuickItems.
 */
class MasonryWindowModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(MasonryWindowModel)
    Q_PROPERTY(FeedRowsModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qreal rowsBottom READ rowsBottom NOTIFY geometryChanged)
    Q_PROPERTY(qreal shortestColumnBottom READ shortestColumnBottom NOTIFY geometryChanged)

public:
    explicit MasonryWindowModel(QObject* parent = nullptr);

    enum Role : int {
        SourceIndexRole = Qt::UserRole + 1,
        StableKeyRole,
        RevisionKeyRole,
        PresentationKindRole,
        MediaLayoutKindRole,
        CardXRole,
        CardYRole,
        CardHeightRole,
        RetiringRole
    };
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] FeedRowsModel* sourceModel() const noexcept;
    void setSourceModel(FeedRowsModel* model);
    [[nodiscard]] qreal rowsBottom() const noexcept;
    [[nodiscard]] qreal shortestColumnBottom() const noexcept;

    /** @brief Rebuilds numeric geometry; never instantiates visual delegates. */
    Q_INVOKABLE void relayout(int columns, qreal cardWidth, qreal gap, qreal top,
                            qreal leadingInset, bool rtl, bool deferWindowUpdate = false);

    /** @brief Explicitly resets lane assignment for a new presentation, not hydration. */
    Q_INVOKABLE void requestRepack() noexcept;

    /** @brief Updates the visible window; true means further admission is pending.
     * Positive budgets cap new and removed slots per transaction. Retiring
     * slots are invisible and noninteractive until their bounded destruction.
     * A positive evictionTimeBudgetMs also yields between individual removals.
     * It is cooperative: one synchronous observer can exceed the deadline.
     * Zero preserves synchronous behavior for native callers and tests.
     */
    Q_INVOKABLE bool setViewport(qreal top, qreal height, qreal enteringBuffer,
                                 qreal retentionBuffer, bool active, bool retainExisting = false,
                                 int admissionBudget = 0, int evictionBudget = 0,
                                 int evictionTimeBudgetMs = 0);

    /** @brief Records a width/revision-specific estimate or exact painted height. */
    Q_INVOKABLE bool retainHeight(const QString& key, const QString& revision,
                                 qreal width, qreal height, bool measured, bool transient = false);

    /** @brief Returns the source row at a content-space coordinate or -1. */
    Q_INVOKABLE [[nodiscard]] int indexAt(qreal x, qreal y) const;

    /** @brief Returns layout coordinates for any source row, visible or not. */
    Q_INVOKABLE [[nodiscard]] QVariantMap geometryAt(int sourceIndex) const;

    /** @brief Finds a cast by stable identity after a source insertion or move. */
    Q_INVOKABLE [[nodiscard]] QVariantMap geometryForKey(const QString& key) const;

    /** @brief Returns the current viewport delegate row for a source index. */
    Q_INVOKABLE [[nodiscard]] int windowIndexForSource(int sourceIndex) const;

signals:
    void sourceModelChanged();
    void countChanged();
    void geometryChanged();
    void geometryInvalidated();

private:
    struct Slot {
        QString key{};
        QString revision{};
        QString kind{};
        QString media{};
        int sourceIndex{-1};
        int column{-1};
        bool laneCommitted{false};
        bool retiring{false};
        qreal x{0};
        qreal y{0};
        qreal height{0};
    };
    void scheduleRelayout();
    void sourceDataChanged(const QModelIndex& first, const QModelIndex& last,
                           const QList<int>& roles);
    bool synchronizeWindow(int admissionBudget = 0, int evictionBudget = 0,
                           int evictionTimeBudgetMs = 0);
    void updateWindowSlot(int index, const Slot& desired);
    [[nodiscard]] QString heightKey(const QString& key, const QString& revision,
                                    qreal width) const;
    [[nodiscard]] qreal fallbackHeight(const Slot& slot) const;
    [[nodiscard]] int laneAt(qreal x) const noexcept;

    QPointer<FeedRowsModel> m_source{};
    QVector<Slot> m_geometry{};
    QHash<QString, int> m_indexByKey{};
    QVector<Slot> m_window{};
    QVector<QVector<int>> m_lanes{};
    QHash<QString, qreal> m_heightSamples{};
    // The transient map contains only the current resize width. Do not evict
    // useful settled samples for every intermediate pixel of window geometry.
    QHash<QString, qreal> m_resizeSamples{};
    qreal m_resizeSampleWidth{0};
    QHash<QString, bool> m_exactSamples{};
    QList<QString> m_sampleOrder{};
    int m_columns{1};
    qreal m_cardWidth{400};
    qreal m_gap{12};
    qreal m_top{12};
    qreal m_inset{12};
    qreal m_bottom{0};
    qreal m_shortestBottom{0};
    qreal m_viewportTop{0};
    qreal m_viewportHeight{0};
    qreal m_buffer{0};
    qreal m_enterBuffer{0};
    bool m_rtl{false};
    bool m_active{false};
    bool m_retainExisting{false};
    bool m_relayoutScheduled{false};
    // Stays dirty even after the invalidation signal: QML may defer layout.
    bool m_sourceGeometryDirty{true};
    // Height samples can change independently of the source model roles.
    bool m_heightGeometryDirty{false};
    bool m_forceRepack{false};
};
