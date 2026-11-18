#include "../../ui/app/miniapp/swap_quote.hpp"
#include "../../ui/app/miniapp/swap_execution.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstdio>
#include <cstdlib>

using farcaster::platform::miniapp::SwapIntent;
using farcaster::platform::miniapp::validateSwapQuote;
using farcaster::platform::miniapp::decimalToHex256;
using farcaster::platform::miniapp::exactApprovalCalldata;

namespace {
void require(const bool pass, const char* description)
{
    if (pass) return;
    std::fprintf(stderr,"swap_quote_contract: %s\n",description);
    std::abort();
}
QJsonObject response(const qint64 now)
{
    return {
        {QStringLiteral("chainId"),8453},
        {QStringLiteral("taker"),QStringLiteral("0x1234567890123456789012345678901234567890")},
        {QStringLiteral("sellToken"),QStringLiteral("eip155:8453/erc20:0x833589fcd6edb6e08f4c7c32d4f71b54bda02913")},
        {QStringLiteral("buyToken"),QStringLiteral("eip155:8453/native")},
        {QStringLiteral("sellAmount"),QStringLiteral("1000000")},
        {QStringLiteral("buyAmount"),QStringLiteral("500000000000000")},
        {QStringLiteral("minBuyAmount"),QStringLiteral("495000000000000")},
        {QStringLiteral("totalNetworkFee"),QStringLiteral("1762500000000")},
        {QStringLiteral("spender"),QStringLiteral("0x0000000000001ff3684f28c67538d4d072c22734")},
        {QStringLiteral("requiresApproval"),true},
        {QStringLiteral("expiresAt"),static_cast<double>(now+20000)},
        {QStringLiteral("transaction"),QJsonObject{
            {QStringLiteral("to"),QStringLiteral("0x7F6cEE965959295cC64d0E6c00d99d6532d8e86b")},
            {QStringLiteral("data"),QStringLiteral("0x1234567800")},
            {QStringLiteral("value"),QStringLiteral("0")},
            {QStringLiteral("gas"),QStringLiteral("141000")}}},
    };
}
}

int main()
{
    const SwapIntent intent{
        .sellToken=QStringLiteral("eip155:8453/erc20:0x833589fcd6edb6e08f4c7c32d4f71b54bda02913"),
        .buyToken=QStringLiteral("eip155:8453/native"),
        .sellAmount=QStringLiteral("1000000"),
    };
    constexpr qint64 now=1795000000000;
    const QString taker=QStringLiteral("0x1234567890123456789012345678901234567890");
    const auto quote=validateSwapQuote(response(now),intent,taker,now);
    require(quote.has_value(),"valid first-party Base quote");
    require(quote->requiresApproval,"ERC20 needs exact approval");
    require(quote->approvalTransaction().value(QStringLiteral("to")).toString()
            ==QStringLiteral("0x833589fcd6edb6e08f4c7c32d4f71b54bda02913"),
            "approve sell token itself, never Settler");
    const QString calldata=quote->approvalTransaction().value(QStringLiteral("data")).toString();
    require(calldata.size()==138,"approve ABI calldata is exactly 4+64 bytes");
    require(calldata.startsWith(QStringLiteral("0x095ea7b3")),"ERC20 approve selector");
    require(calldata.endsWith(QStringLiteral("00000000000000000000000000000000000000000000000000000000000f4240")),
            "exact finite allowance for one million units");
    require(quote->swapTransaction().value(QStringLiteral("value")).toString()
            ==QStringLiteral("0x0"),"no unexpected native value");
    require(decimalToHex256(QStringLiteral("115792089237316195423570985008687907853269984665640564039457584007913129639935"))
            ==QString(64,QLatin1Char('f')),"full uint256 preserved");
    require(decimalToHex256(QStringLiteral("0"))==QStringLiteral("0"),"zero conversion");
    require(exactApprovalCalldata(QStringLiteral("0x1234567890123456789012345678901234567890"),
                                 QStringLiteral("1")).isEmpty(),
            "never approve a non-audited spender");
    QJsonObject tampered=response(now);
    auto rejects=[&](const char* message) {
        require(!validateSwapQuote(tampered,intent,taker,now),message);
        tampered=response(now);
    };
    require(quote->swapTransaction().value(QStringLiteral("to")).toString()
            ==QStringLiteral("0x7F6cEE965959295cC64d0E6c00d99d6532d8e86b"),
            "dynamic Settler destination preserved separate from allowance spender");
    tampered.insert(QStringLiteral("chainId"),1); rejects("chain isolation");
    tampered.insert(QStringLiteral("taker"),QStringLiteral("0x2234567890123456789012345678901234567890"));
    rejects("wallet address binding");
    tampered.insert(QStringLiteral("sellAmount"),QStringLiteral("2000000"));
    rejects("quote amount binding");
    tampered.insert(QStringLiteral("minBuyAmount"),QStringLiteral("500000000000001"));
    rejects("minBuy must not exceed quoted output");
    tampered.insert(QStringLiteral("spender"),QStringLiteral("0x2234567890123456789012345678901234567890"));
    rejects("deny hostile approval spender");
    tampered.insert(QStringLiteral("expiresAt"),static_cast<double>(now+100000));
    rejects("quote cannot outlive 25 second freshness");
    tampered.insert(QStringLiteral("expiresAt"),static_cast<double>(now-1));
    rejects("expired quote");
    QJsonObject tx=tampered.value(QStringLiteral("transaction")).toObject();
    tx.insert(QStringLiteral("to"),taker);
    tampered.insert(QStringLiteral("transaction"),tx); rejects("reject wallet self-send destination");
    tx=tampered.value(QStringLiteral("transaction")).toObject();
    tx.insert(QStringLiteral("value"),QStringLiteral("1"));
    tampered.insert(QStringLiteral("transaction"),tx); rejects("reject unsolicited native transfer");
    tx=tampered.value(QStringLiteral("transaction")).toObject();
    tx.insert(QStringLiteral("data"),QStringLiteral("0xHI"));
    tampered.insert(QStringLiteral("transaction"),tx); rejects("reject malformed calldata");
    const QString txHash=QStringLiteral("0x")+QString(64,QLatin1Char('a'));
    const QString blockHash=QStringLiteral("0x")+QString(64,QLatin1Char('b'));
    const QJsonObject receipt{
        {QStringLiteral("transactionHash"),txHash},
        {QStringLiteral("blockHash"),blockHash},
        {QStringLiteral("blockNumber"),QStringLiteral("0x1024")},
        {QStringLiteral("from"),taker},
        {QStringLiteral("to"),QStringLiteral("0x7F6cEE965959295cC64d0E6c00d99d6532d8e86b")},
        {QStringLiteral("status"),QStringLiteral("0x1")},
    };
    using farcaster::platform::miniapp::ReceiptStatus;
    using farcaster::platform::miniapp::validateReceipt;
    const QString to=receipt.value(QStringLiteral("to")).toString();
    require(validateReceipt(receipt,txHash,taker,to)==ReceiptStatus::Success,
            "confirmed successful swap receipt");
    require(validateReceipt(QJsonValue::Null,txHash,taker,to)==ReceiptStatus::Pending,
            "pending receipt cannot prove success");
    QJsonObject badReceipt=receipt;
    badReceipt.insert(QStringLiteral("status"),QStringLiteral("0x0"));
    require(validateReceipt(badReceipt,txHash,taker,to)==ReceiptStatus::Reverted,
            "reverted receipt cannot prove success");
    badReceipt=receipt;
    badReceipt.insert(QStringLiteral("transactionHash"),blockHash);
    require(validateReceipt(badReceipt,txHash,taker,to)==ReceiptStatus::Invalid,
            "mismatched tx hash invalid");
    badReceipt=receipt;
    badReceipt.insert(QStringLiteral("from"),QStringLiteral("0x2234567890123456789012345678901234567890"));
    require(validateReceipt(badReceipt,txHash,taker,to)==ReceiptStatus::Invalid,
            "mismatched wallet address invalid");
    badReceipt=receipt;
    badReceipt.insert(QStringLiteral("to"),taker);
    require(validateReceipt(badReceipt,txHash,taker,to)==ReceiptStatus::Invalid,
            "mismatched router invalid");
    badReceipt=receipt;
    badReceipt.remove(QStringLiteral("blockNumber"));
    require(validateReceipt(badReceipt,txHash,taker,to)==ReceiptStatus::Invalid,
            "unmined receipt invalid");
    std::puts("swap_quote_contract: quote, expiry, taker, spender, ABI approval and receipts passed");
}
