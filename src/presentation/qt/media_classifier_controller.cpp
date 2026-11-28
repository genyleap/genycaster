#include "media_classifier_controller.hpp"
#include "media_embed_equivalence.hpp"

#include <QString>
#include <QVariant>

#include <string>

import farcaster.presentation.media;

namespace {
[[nodiscard]] QString fieldValue(const QVariantMap& source, const QString& key)
{
    const QVariant value = source.value(key);
    if (value.isValid()) {
        return value.toString();
    }
    return {};
}

[[nodiscard]] QVariantMap resultMap(const farcaster::presentation::media::Classification& result)
{
    const QString label = QString::fromLatin1(
        farcaster::presentation::media::kindLabel(result.kind));
    return QVariantMap{
        {QStringLiteral("family"), label.isEmpty() ? QStringLiteral("web") : label},
        {QStringLiteral("visual"), result.visual},
        {QStringLiteral("directPlayable"), result.directPlayable},
    };
}

struct ClassificationInputs final {
    std::string kind;
    std::string url;
    std::string mediaType;
    std::string contentType;
    std::string providerKind;
};
} // namespace

namespace {
[[nodiscard]] ClassificationInputs collectInputs(
    const QVariantMap& embed,
    const QVariantMap& metadata)
{
    ClassificationInputs inputs{};
    QString url = fieldValue(embed, QStringLiteral("url"));
    if (url.isEmpty()) {
        url = fieldValue(embed, QStringLiteral("value"));
    }
    inputs.kind = fieldValue(embed, QStringLiteral("kind")).toStdString();
    inputs.url = url.toStdString();
    inputs.mediaType = fieldValue(embed, QStringLiteral("mediaType")).toStdString();
    inputs.contentType = fieldValue(embed, QStringLiteral("contentType")).toStdString();
    if (inputs.contentType.empty()) {
        inputs.contentType = fieldValue(metadata, QStringLiteral("contentType")).toStdString();
    }
    inputs.providerKind = fieldValue(metadata, QStringLiteral("providerKind")).toStdString();
    if (inputs.providerKind.empty()) {
        inputs.providerKind = fieldValue(embed, QStringLiteral("providerKind")).toStdString();
    }
    return inputs;
}
} // namespace

MediaClassifierController::MediaClassifierController(QObject* const parent)
    : QObject(parent)
{
}

QVariantMap MediaClassifierController::classify(
    const QVariantMap& embed,
    const QVariantMap& metadata) const
{
    const ClassificationInputs inputs = collectInputs(embed, metadata);
    return resultMap(farcaster::presentation::media::classify(
        inputs.kind, inputs.url, inputs.mediaType, inputs.contentType,
        inputs.providerKind));
}

QVariantMap MediaClassifierController::classifyUrl(const QString& url) const
{
    return resultMap(
        farcaster::presentation::media::classifyUrl(url.toStdString()));
}


bool MediaClassifierController::sameEmbedRecords(const QVariantList& left,
                                                 const QVariantList& right) const
{
    return genycaster::presentation::detail::sameEmbedRecords(left, right);
}
