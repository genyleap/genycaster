#include "../../src/transport/qt_network/media_json_numbers.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <cstdint>
#include <limits>
#include <source_location>

namespace {
void require(const bool condition,
             const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Media JSON numeric safety failed at %s:%u",
               location.file_name(), location.line());
}

QJsonValue jsonNumber(const QByteArray& literal)
{
    const QByteArray json = '[' + literal + ']';
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    require(error.error == QJsonParseError::NoError && document.isArray());
    require(document.array().size() == 1);
    return document.array().at(0);
}
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    using farcaster::transport::qtnetwork::media::detail::boundedJsonUnsigned;
    constexpr std::uint64_t maximumPartSize = 64U * 1024U * 1024U;
    const auto accepted = [](const QJsonValue& value, std::uint64_t maximum,
                             std::uint64_t expected) {
        const auto decoded = boundedJsonUnsigned(value, maximum);
        require(decoded && *decoded == expected);
    };
    const auto rejected = [](const QJsonValue& value, std::uint64_t maximum) {
        require(!boundedJsonUnsigned(value, maximum));
    };

    accepted(jsonNumber("0"), maximumPartSize, 0);
    accepted(jsonNumber("1"), maximumPartSize, 1);
    accepted(jsonNumber("1.0"), maximumPartSize, 1);
    accepted(jsonNumber("67108864"), maximumPartSize, maximumPartSize);
    accepted(jsonNumber("10000"), 10000U, 10000U);
    rejected(jsonNumber("67108865"), maximumPartSize);
    rejected(jsonNumber("10001"), 10000U);
    rejected(jsonNumber("-1"), maximumPartSize);
    rejected(jsonNumber("-2.5"), maximumPartSize);
    rejected(jsonNumber("0.5"), maximumPartSize);
    rejected(jsonNumber("1.5"), maximumPartSize);
    rejected(jsonNumber("1e100"), maximumPartSize);
    rejected(jsonNumber("9223372036854775808"),
             std::numeric_limits<std::uint64_t>::max());
    rejected(QJsonValue{QStringLiteral("12")}, maximumPartSize);
    rejected(QJsonValue{true}, maximumPartSize);
    rejected(QJsonValue{}, maximumPartSize);
    rejected(QJsonValue{QJsonValue::Null}, maximumPartSize);
    rejected(QJsonValue{std::numeric_limits<double>::quiet_NaN()}, maximumPartSize);
    rejected(QJsonValue{std::numeric_limits<double>::infinity()}, maximumPartSize);
    // Server part numbers and sizes can be bounded by the local expectation:
    // reject a mismatched oversized value without ever casting the double.
    accepted(jsonNumber("256"), 256U, 256U);
    rejected(jsonNumber("257"), 256U);
    return 0;
}
