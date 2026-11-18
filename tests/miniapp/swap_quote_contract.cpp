#include "../../ui/app/miniapp/swap_quote.hpp"
#include "../../ui/app/miniapp/swap_execution.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

using namespace farcaster::platform::miniapp;

namespace {
void require(const bool pass,const char* reason)
{
    if (pass) return;
    std::fprintf(stderr,"swap_quote_contract failed: %s\n",reason);
    std::abort();
}

constexpr qint64 when=1795000000000;
const QString taker=QStringLiteral("0x1234567890123456789012345678901234567890");
const QString erc20=QStringLiteral("eip155:8453/erc20:0x833589fcd6edb6e08f4c7c32d4f71b54bda02913");
const QString native=QStringLiteral("eip155:8453/native");
const QString buyErc20=QStringLiteral("eip155:8453/erc20:0x4567890123456789012345678901234567890123");

QJsonObject response(const SwapIntent& intent,const QStringList& path,
                     const qint64 now=when,const bool needsApproval=true)
{
    const QString buyAmount=QStringLiteral("500000000000000");
    const QString minBuyAmount=QStringLiteral("495000000000000");
    const qint64 deadline=now/1000+180;
    return {
        {QStringLiteral("provider"),QStringLiteral("uniswap-v2-onchain")},
        {QStringLiteral("chainId"),8453},
        {QStringLiteral("taker"),taker},
        {QStringLiteral("sellToken"),intent.sellToken},
        {QStringLiteral("buyToken"),intent.buyToken},
        {QStringLiteral("sellAmount"),intent.sellAmount},
        {QStringLiteral("buyAmount"),buyAmount},
        {QStringLiteral("minBuyAmount"),minBuyAmount},
        {QStringLiteral("totalNetworkFee"),QStringLiteral("1762500000000")},
        {QStringLiteral("spender"),QString::fromLatin1(baseSwapRouter)},
        {QStringLiteral("requiresApproval"),needsApproval},
        {QStringLiteral("route"),QJsonArray::fromStringList(path)},
        {QStringLiteral("deadline"),static_cast<double>(deadline)},
        {QStringLiteral("expiresAt"),static_cast<double>(now+20'000)},
        {QStringLiteral("transaction"),QJsonObject{
            {QStringLiteral("to"),QString::fromLatin1(baseSwapRouter)},
            {QStringLiteral("data"),encodeSwapCalldata(intent,taker,minBuyAmount,path,deadline)},
            {QStringLiteral("value"),isNativeSwapToken(intent.sellToken)
                ?intent.sellAmount:QStringLiteral("0")},
            {QStringLiteral("gas"),QStringLiteral("141000")}}},
    };
}
}

int main()
{
    const SwapIntent intent{
        .sellToken=erc20,.buyToken=native,.sellAmount=QStringLiteral("1000000"),
    };
    const QStringList path{contractForSwapToken(erc20),QString::fromLatin1(baseWrappedEther)};
    const auto quote=validateSwapQuote(response(intent,path),intent,taker,when);
    require(quote.has_value(),"valid direct Base Uniswap V2 token to ETH quote");
    require(quote->requiresApproval,"only missing ERC20 allowance prompts approval");
    require(quote->swapTransaction().value(QStringLiteral("to")).toString()
            ==QString::fromLatin1(baseSwapRouter),"only router as swap recipient");
    require(quote->swapTransaction().value(QStringLiteral("data")).toString()
            .startsWith(QStringLiteral("0x18cbafe5")),"token to native selector");
    require(!quote->swapTransaction().contains(QStringLiteral("gas")),
            "wallet estimates execution gas, no backend-imposed gas cap");
    require(quote->approvalTransaction().value(QStringLiteral("to")).toString()
            ==contractForSwapToken(erc20),"approve ERC20 token itself");
    const QString approval=quote->approvalTransaction().value(QStringLiteral("data")).toString();
    require(approval.size()==138,"exact ERC20 approve ABI");
    require(approval.startsWith(QStringLiteral("0x095ea7b3")),"ERC20 approve selector");
    require(approval.endsWith(QStringLiteral("00000000000000000000000000000000000000000000000000000000000f4240")),
            "finite allowance of one million raw units");
    require(approval.contains(QString::fromLatin1(baseSwapRouter).mid(2)),
            "router receives allowance rather than aggregator or Settler");

    const SwapIntent fromEth{
        .sellToken=native,.buyToken=erc20,.sellAmount=QStringLiteral("1000000000000000")
    };
    const QStringList ethPath{QString::fromLatin1(baseWrappedEther),contractForSwapToken(erc20)};
    const auto nativeQuote=validateSwapQuote(response(fromEth,ethPath,when,false),fromEth,taker,when);
    require(nativeQuote.has_value(),"native ETH to token quote");
    require(nativeQuote->swapTransaction().value(QStringLiteral("data")).toString()
            .startsWith(QStringLiteral("0x7ff36ab5")),"native ETH input selector");
    require(nativeQuote->swapTransaction().value(QStringLiteral("value")).toString()
            ==QStringLiteral("0x38d7ea4c68000"),"exact native ETH msg.value");
    const SwapIntent between{
        .sellToken=erc20,.buyToken=buyErc20,.sellAmount=QStringLiteral("1000000")
    };
    const QStringList bridged{contractForSwapToken(erc20),
        QString::fromLatin1(baseWrappedEther),contractForSwapToken(buyErc20)};
    const auto three=validateSwapQuote(response(between,bridged),between,taker,when);
    require(three.has_value(),"three-token route bridged via WETH");
    require(three->swapTransaction().value(QStringLiteral("data")).toString()
            .startsWith(QStringLiteral("0x38ed1739")),"token-to-token selector");

    require(decimalToHex256(
        QStringLiteral("115792089237316195423570985008687907853269984665640564039457584007913129639935"))
        ==QString(64,QLatin1Char('f')),"full 256-bit math");
    require(decimalToHex256(QStringLiteral("0"))==QStringLiteral("0"),"zero conversion");
    require(exactApprovalCalldata(taker,QStringLiteral("1")).isEmpty(),
            "arbitrary spender rejected");

    QJsonObject tampered=response(intent,path);
    const auto rejects=[&](const char* reason) {
        require(!validateSwapQuote(tampered,intent,taker,when),reason);
        tampered=response(intent,path);
    };
    tampered.insert(QStringLiteral("provider"),QStringLiteral("0x"));rejects("no aggregator payloads");
    tampered.insert(QStringLiteral("chainId"),1);rejects("wrong chain");
    tampered.insert(QStringLiteral("taker"),QStringLiteral("0x2234567890123456789012345678901234567890"));
    rejects("different wallet");
    tampered.insert(QStringLiteral("sellAmount"),QStringLiteral("2000000"));rejects("wrong amount");
    tampered.insert(QStringLiteral("minBuyAmount"),QStringLiteral("600000000000000"));
    rejects("minimum greater than quote");
    tampered.insert(QStringLiteral("spender"),taker);rejects("wrong allowance spender");
    tampered.insert(QStringLiteral("expiresAt"),static_cast<double>(when+100000));
    rejects("quote too old");
    tampered.insert(QStringLiteral("deadline"),static_cast<double>(when/1000+2000));
    rejects("unsafe expiration deadline");
    tampered.insert(QStringLiteral("route"),QJsonArray{taker,QString::fromLatin1(baseWrappedEther)});
    rejects("arbitrary path tokens");
    QJsonObject tx=tampered.value(QStringLiteral("transaction")).toObject();
    tx.insert(QStringLiteral("to"),taker);
    tampered.insert(QStringLiteral("transaction"),tx);rejects("malicious target");
    tx=tampered.value(QStringLiteral("transaction")).toObject();
    tx.insert(QStringLiteral("data"),QStringLiteral("0x12345678"));
    tampered.insert(QStringLiteral("transaction"),tx);rejects("unrecognized calldata");
    tx=tampered.value(QStringLiteral("transaction")).toObject();
    tx.insert(QStringLiteral("value"),QStringLiteral("1"));
    tampered.insert(QStringLiteral("transaction"),tx);rejects("unexpected ETH transfer");

    const QString txHash=QStringLiteral("0x")+QString(64,QLatin1Char('a'));
    const QString blockHash=QStringLiteral("0x")+QString(64,QLatin1Char('b'));
    const QJsonObject receipt{
        {QStringLiteral("transactionHash"),txHash},
        {QStringLiteral("blockHash"),blockHash},
        {QStringLiteral("blockNumber"),QStringLiteral("0x1024")},
        {QStringLiteral("from"),taker},
        {QStringLiteral("to"),QString::fromLatin1(baseSwapRouter)},
        {QStringLiteral("status"),QStringLiteral("0x1")},
    };
    const QString destination=QString::fromLatin1(baseSwapRouter);
    require(validateReceipt(receipt,txHash,taker,destination)==ReceiptStatus::Success,
            "receipt confirms the correct swap");
    require(validateReceipt(QJsonValue::Null,txHash,taker,destination)==ReceiptStatus::Pending,
            "unmined transaction remains pending");
    QJsonObject badReceipt=receipt;
    badReceipt.insert(QStringLiteral("status"),QStringLiteral("0x0"));
    require(validateReceipt(badReceipt,txHash,taker,destination)==ReceiptStatus::Reverted,
            "reverted swap never succeeds");
    badReceipt=receipt;
    badReceipt.insert(QStringLiteral("to"),taker);
    require(validateReceipt(badReceipt,txHash,taker,destination)==ReceiptStatus::Invalid,
            "receipt of different router rejected");
    std::puts("swap_quote_contract: direct on-chain Uniswap V2 validation and receipts passed");
}
