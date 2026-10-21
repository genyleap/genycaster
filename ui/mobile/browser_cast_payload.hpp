#pragma once

#if defined(__EMSCRIPTEN__)

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <optional>

namespace genycaster::ui::browser_signing {

[[nodiscard]] inline std::optional<QJsonObject> makeCastReference(
    qulonglong fid, const QString& hash);

[[nodiscard]] inline std::optional<QJsonObject> makeCastPayload(
    const QString& text,
    const QVariantList& mentionFids,
    const QVariantList& mentionPositions,
    const QVariantList& embeds,
    const std::optional<QJsonObject>& parent = std::nullopt,
    const QString& parentUrl = {})
{
    if (mentionFids.size() != mentionPositions.size() ||
        mentionFids.size() > 10 || embeds.size() > 2 ||
        text.toUtf8().size() > 1024 || (text.isEmpty() && embeds.isEmpty())) {
        return std::nullopt;
    }
    QJsonArray serializedFids{};
    QJsonArray serializedPositions{};
    QJsonArray serializedEmbeds{};
    std::uint32_t previousPosition{};
    for (qsizetype i = 0; i < mentionFids.size(); ++i) {
        bool validFid = false;
        bool validPosition = false;
        const auto fid = mentionFids.at(i).toULongLong(&validFid);
        const auto pos = mentionPositions.at(i).toUInt(&validPosition);
        if (!validFid || !validPosition || fid == 0 ||
            pos > static_cast<std::uint32_t>(text.toUtf8().size()) ||
            (i > 0 && pos < previousPosition)) return std::nullopt;
        serializedFids.append(QString::number(fid)); // preserve uint64 precision
        serializedPositions.append(static_cast<int>(pos));
        previousPosition = pos;
    }

    for (const QVariant& value : embeds) {
        const QVariantMap embed = value.toMap();
        const QString kind = embed.value(QStringLiteral("kind")).toString();
        const QString payload = embed.value(QStringLiteral("value")).toString();
        if (payload.isEmpty()) return std::nullopt;
        if (kind == QStringLiteral("url")) {
            if (payload.toUtf8().size() > 256) return std::nullopt;
            serializedEmbeds.append(QJsonObject{{QStringLiteral("url"), payload}});
        } else if (kind == QStringLiteral("cast-reference")) {
            bool valid = false;
            const auto author = embed.value(QStringLiteral("authorFid")).toULongLong(&valid);
            if (!valid || author == 0 || !makeCastReference(author,payload)) return std::nullopt;
            serializedEmbeds.append(QJsonObject{
                {QStringLiteral("cast"), QJsonObject{
                    {QStringLiteral("fid"), QString::number(author)},
                    {QStringLiteral("hash"), payload}}}});
        } else {
            return std::nullopt;
        }
    }
    QJsonObject result{
        {QStringLiteral("text"), text},
        {QStringLiteral("mentions"), serializedFids},
        {QStringLiteral("mentionPositions"), serializedPositions},
        {QStringLiteral("embeds"), serializedEmbeds},
    };
    if (parent && !parentUrl.isEmpty()) return std::nullopt;
    if (!parentUrl.isEmpty()) {
        if (parentUrl.toUtf8().size() > 256
                || !(parentUrl.startsWith(QStringLiteral("https://"))
                     || parentUrl.startsWith(QStringLiteral("chain://"))))
            return std::nullopt;
        result.insert(QStringLiteral("parentUrl"), parentUrl);
    }
    if (parent) result.insert(QStringLiteral("parent"), *parent);
    return result;
}

[[nodiscard]] inline std::optional<QJsonObject> makeCastReference(
    const qulonglong fid, const QString& hash)
{
    if (fid == 0) return std::nullopt;
    QString normalized = hash;
    if (normalized.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        normalized.remove(0, 2);
    }
    if (normalized.size() != 40) return std::nullopt;
    for (const QChar character : normalized) {
        const char16_t code = character.unicode();
        if (!((code >= '0' && code <= '9')
              || (code >= 'a' && code <= 'f')
              || (code >= 'A' && code <= 'F'))) return std::nullopt;
    }
    return QJsonObject{{QStringLiteral("fid"), QString::number(fid)},
                       {QStringLiteral("hash"), QStringLiteral("0x") + normalized.toLower()}};
}

} // namespace genycaster::ui::browser_signing

#endif
