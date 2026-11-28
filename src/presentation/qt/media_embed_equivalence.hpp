#pragma once

#include <QMetaType>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace genycaster::presentation::detail {

// Do not use coercive QVariant equality across different types. An unknown
// object (e.g. QJSValue) is not a proven immutable record; fail closed to a
// normal content refresh. Bound recursive work on unexpectedly nested data.
[[nodiscard]] inline bool sameEmbedValue(const QVariant& left, const QVariant& right,
                                         const int depth = 0)
{
    if (depth >= 32 || left.metaType() != right.metaType()) return false;
    if (!left.isValid()) return !right.isValid();
    switch (left.metaType().id()) {
    case QMetaType::QVariantList: {
        const auto a = left.toList();
        const auto b = right.toList();
        if (a.size() != b.size()) return false;
        for (qsizetype i = 0; i < a.size(); ++i)
            if (!sameEmbedValue(a.at(i), b.at(i), depth + 1)) return false;
        return true;
    }
    case QMetaType::QVariantMap: {
        const auto a = left.toMap();
        const auto b = right.toMap();
        if (a.size() != b.size()) return false;
        auto ai = a.cbegin();
        auto bi = b.cbegin();
        for (; ai != a.cend(); ++ai, ++bi)
            if (ai.key() != bi.key()
                || !sameEmbedValue(ai.value(), bi.value(), depth + 1)) return false;
        return true;
    }
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::QString:
    case QMetaType::QByteArray:
    case QMetaType::Nullptr:
        return left.isNull() == right.isNull() && left == right;
    default:
        return false;
    }
}

[[nodiscard]] inline bool sameEmbedRecords(const QVariantList& left, const QVariantList& right)
{
    if (left.size() != right.size()) return false;
    for (qsizetype i = 0; i < left.size(); ++i)
        if (!sameEmbedValue(left.at(i), right.at(i))) return false;
    return true;
}

} // namespace genycaster::presentation::detail
