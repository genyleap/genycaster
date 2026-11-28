#pragma once

#include <QMetaType>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace genycaster::presentation::detail {

// Only handle the canonical primitive scalar fields. For unusual provider
// records, decline the native path instead of guessing JavaScript coercion.
[[nodiscard]] inline std::optional<double> profileNumber(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) return 0.0;
    switch (value.metaType().id()) {
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::QString: {
        if (value.metaType().id() == QMetaType::QString
            && value.toString().trimmed().isEmpty()) return 0.0;
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (ok && std::isfinite(number)) return number;
        return std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

[[nodiscard]] inline bool profileTruthy(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) return false;
    if (value.metaType().id() == QMetaType::QString) return !value.toString().isEmpty();
    if (const auto number = profileNumber(value)) return *number != 0.0;
    return true; // Unknown nonempty field: numeric conversion will decline.
}

// An implicitly shared, immutable native snapshot paired with a token.
// Returning indices avoids sending every nested cast back through QML/C++
// twice. The token prevents applying an old permutation to new payloads.
class ProfileRowSnapshot final {
public:
    [[nodiscard]] QVariantMap capture(const QVariantList& source)
    {
        m_rows = source;
        ++m_generation;
        return {{QStringLiteral("rows"), m_rows},
                {QStringLiteral("token"), QString::number(m_generation)}};
    }

    void invalidate()
    {
        m_rows.clear();
        ++m_generation;
    }

    [[nodiscard]] QVariantList order(const QString& token, const bool popular,
                                     const QString& mode, const QString& metric) const
    {
        if (token.isEmpty() || token != QString::number(m_generation)) return {};
        struct Key { qsizetype index; double time; double score; };
        std::vector<Key> keys;
        keys.reserve(static_cast<std::size_t>(m_rows.size()));
        for (qsizetype i = 0; i < m_rows.size(); ++i) {
            const auto row = m_rows.at(i).toMap();
            const auto created = row.value(QStringLiteral("createdAtMs"));
            const auto time = profileNumber(profileTruthy(created)
                ? created : row.value(QStringLiteral("time")));
            if (!time) return {};
            double score = 0.0;
            if (popular) {
                const auto likes = profileNumber(row.value(QStringLiteral("likeCount")));
                const auto recasts = profileNumber(row.value(QStringLiteral("recastCount")));
                const auto replies = profileNumber(row.value(QStringLiteral("replyCount")));
                if (!likes || !recasts || !replies) return {};
                if (metric == QStringLiteral("likes")) score = *likes;
                else if (metric == QStringLiteral("recasts")) score = *recasts;
                else if (metric == QStringLiteral("replies")) score = *replies;
                else score = *likes + *recasts * 2.0 + *replies * 1.5;
                if (!std::isfinite(score)) return {};
            }
            keys.push_back({i, *time, score});
        }
        const bool oldest = !popular && mode == QStringLiteral("oldest");
        std::sort(keys.begin(), keys.end(), [&](const Key& left, const Key& right) {
            if (popular && left.score != right.score) return left.score > right.score;
            if (left.time != right.time) return oldest ? left.time < right.time : left.time > right.time;
            return left.index < right.index;
        });
        QVariantList result;
        result.reserve(m_rows.size());
        for (const auto& key : keys) result.append(key.index);
        return result;
    }

private:
    QVariantList m_rows;
    quint64 m_generation{0};
};

} // namespace genycaster::presentation::detail
