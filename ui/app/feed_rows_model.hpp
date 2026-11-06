/*!
 * @file        ui/app/feed_rows_model.hpp
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

signals:
    void countChanged();

private:
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
