#include "../../ui/app/miniapp/swap_intent.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <cstdio>
#include <cstdlib>
#include <optional>

namespace {
using farcaster::platform::miniapp::parseSwapIntent;

void check(const bool condition, const char* message)
{
    if (condition) return;
    std::fprintf(stderr, "swap_intent_contract: %s\n", message);
    std::abort();
}

bool accepted(const QJsonValue& value)
{
    QString error{};
    return parseSwapIntent(value, error).has_value();
}

QJsonObject valid()
{
    return {
        {QStringLiteral("sellToken"),
         QStringLiteral("eip155:8453/erc20:0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913")},
        {QStringLiteral("buyToken"), QStringLiteral("eip155:8453/native")},
        {QStringLiteral("sellAmount"), QStringLiteral("1000000")},
    };
}

void replace(QJsonObject object, const QString& field, const QJsonValue& value,
             const bool expected, const char* explanation)
{
    object.insert(field, value);
    check(accepted(object) == expected, explanation);
}
}

int main()
{
    const QJsonObject options = valid();
    QString rejection{};
    const auto parsed = parseSwapIntent(options, rejection);
    check(parsed.has_value(), "valid Base swap intent");
    check(parsed->sellAmount == QStringLiteral("1000000"), "preserve exact string base units");
    check(parsed->chain == QStringLiteral("eip155:8453"), "Base-only scope");
    check(parsed->toVariantMap().value(QStringLiteral("sellToken")).toString()
          == options.value(QStringLiteral("sellToken")).toString(), "preserve CAIP-19");
    check(accepted(QJsonObject{}), "optional tokens can be left blank for user choice");
    check(accepted(QJsonValue::Null), "null options accepted as empty prefill");
    check(accepted(QJsonValue::Undefined), "omitted options accepted");
    check(!accepted(QJsonValue{QStringLiteral("ETH")}), "reject nonobject payloads");
    check(!accepted(QJsonValue{123}), "reject numeric payloads");
    check(!accepted(QJsonValue{true}), "reject booleans");
    check(!accepted(QJsonObject{{QStringLiteral("unknown"), QStringLiteral("a")}}),
          "unknown fields fail closed");
    check(!accepted(QJsonObject{{QStringLiteral("sellToken"), QJsonValue{123}}}),
          "token fields must be strings");
    check(!accepted(QJsonObject{{QStringLiteral("sellAmount"), QStringLiteral("123")}}),
          "amount requires a sell asset");
    check(!accepted(QJsonObject{{QStringLiteral("sellToken"),
                                 QStringLiteral("eip155:8453/native")},
                                {QStringLiteral("sellAmount"), QStringLiteral("0")}}),
          "no zero amount");
    replace(options, QStringLiteral("sellAmount"), QStringLiteral("1.2"), false,
            "fractional base units invalid");
    replace(options, QStringLiteral("sellAmount"), QStringLiteral("-1"), false,
            "negative base units invalid");
    replace(options, QStringLiteral("sellAmount"), QStringLiteral("01"), false,
            "leading zero invalid");
    replace(options, QStringLiteral("sellAmount"), QStringLiteral("100000000000000000000"), true,
            "no IEEE-754 precision loss");
    replace(options, QStringLiteral("sellAmount"),
            QStringLiteral("115792089237316195423570985008687907853269984665640564039457584007913129639935"),
            true, "max uint256 amount accepted");
    replace(options, QStringLiteral("sellAmount"),
            QStringLiteral("115792089237316195423570985008687907853269984665640564039457584007913129639936"),
            false, "uint256 overflow rejected");
    replace(options, QStringLiteral("buyToken"), options.value(QStringLiteral("sellToken")),
            false, "self-swaps rejected");
    replace(options, QStringLiteral("buyToken"),
            QStringLiteral("eip155:1/native"), false, "unapproved mainnet rejected");
    replace(options, QStringLiteral("buyToken"),
            QStringLiteral("eip155:8453/erc20:0xinvalid"), false, "bad ERC20 address");
    replace(options, QStringLiteral("buyToken"),
            QStringLiteral("eip155:8453/erc20:0x0000000000000000000000000000000000000000"),
            false, "zero ERC20 address rejected");
    replace(options, QStringLiteral("buyToken"),
            QStringLiteral("https://example.com/token"), false, "URLs are not asset IDs");
    replace(options, QStringLiteral("sellAmount"), QStringLiteral(" 1000000"), false,
            "whitespace in amount rejected");
    QJsonObject native = options;
    native.insert(QStringLiteral("buyToken"), options.value(QStringLiteral("sellToken")));
    native.insert(QStringLiteral("sellToken"), QStringLiteral("eip155:8453/native"));
    check(accepted(native), "native sell asset accepted");
    std::puts("swap_intent_contract: all 24 scenarios passed");
    return 0;
}
