// Browser custody adapter for the shared QML setup. The JavaScript wallet
// owns temporary mnemonic state and all cryptographic/network authority.
// Only metadata and a transient, explicitly revealed painted word buffer
// cross the WASM boundary; no QML property exposes generated words.
#include "native_wallet.hpp"
#include <QPromise>
#include <QChar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <string_view>
#include <QTimer>
#include <algorithm>
#include <array>
#include <emscripten.h>
import farcaster.capability.signer;
#include "native_wallet_signer.hpp"

namespace {
[[nodiscard]] QFuture<std::optional<QString>> unavailableSignature()
{
    QPromise<std::optional<QString>> promise;
    promise.start();
    promise.addResult(std::optional<QString>{});
    promise.finish();
    return promise.future();
}
QVariantMap readState()
{
    std::array<char,4096> buffer{};
    const auto received = EM_ASM_INT({
        const bridge=window.__genycasterSetupWallet;
        if(!bridge)return 0;
        const result=JSON.stringify(bridge.state());
        if(lengthBytesUTF8(result)+1>$1)return 0;
        stringToUTF8(result,$0,$1);
        return 1;
    },buffer.data(),buffer.size());
    if(!received)return {};
    const auto parsed=QJsonDocument::fromJson(buffer.data());
    return parsed.isObject()?parsed.object().toVariantMap():QVariantMap{};
}
// Bound each dispatch to the fixed setup API. No generic JS evaluation or
// arbitrary signing capability is available from QML.
bool dispatch(const QString& method, QString a={}, QString b={}, QString c={})
{
    auto name=method.toUtf8();
    auto x=a.toUtf8(),y=b.toUtf8(),z=c.toUtf8();
    a.fill(QChar{}); b.fill(QChar{}); c.fill(QChar{});
    const int result=EM_ASM_INT({
        const bridge=window.__genycasterSetupWallet;
        if(!bridge)return 0;
        const method=UTF8ToString($0,$1);
        const a=UTF8ToString($2,$3);
        const b=UTF8ToString($4,$5);
        const c=UTF8ToString($6,$7);
        switch(method){
        case 'begin':return bridge.begin(a,b)?1:0;
        case 'challenge':return bridge.challenge()?1:0;
        case 'verify':return bridge.verify(a,b)?1:0;
        case 'reveal':bridge.reveal();return 1;
        case 'copy':return bridge.copy()?1:0;
        case 'prepare':return bridge.prepare(a,b,c)?1:0;
        case 'recovery':return bridge.stageRecovery(a,b)?1:0;
        case 'discard':return bridge.discard()?1:0;
        case 'cancel':return bridge.cancel()?1:0;
        case 'voucher':return bridge.acceptVoucher(a)?1:0;
        case 'reserve':return bridge.reserveSponsored()?1:0;
        case 'leave':return bridge.leave()?1:0;
        default:return 0;
        }
    },name.constData(),name.size(),x.constData(),x.size(),
      y.constData(),y.size(),z.constData(),z.size());
    x.fill('\0'); y.fill('\0'); z.fill('\0');
    return result!=0;
}
}
struct NativeCustodyWalletController::Private {
    QVariantMap state{};
    QTimer timer{};
};
NativeCustodyWalletController::NativeCustodyWalletController(QObject* parent)
    : QObject{parent}, m_private{std::make_unique<Private>()}
{
    m_private->timer.setInterval(100);
    connect(&m_private->timer,&QTimer::timeout,this,[this]{
        auto state=readState();
        if(state==m_private->state)return;
        m_private->state=std::move(state);
        emit changed();
    });
    m_private->timer.start();
}
NativeCustodyWalletController::~NativeCustodyWalletController() = default;
bool NativeCustodyWalletController::hasWallet() const noexcept {return m_private->state.value(QStringLiteral("hasWallet")).toBool();}
bool NativeCustodyWalletController::recoveryVisible() const noexcept {return m_private->state.value(QStringLiteral("revealed")).toBool();}
bool NativeCustodyWalletController::backupVerified() const noexcept {return m_private->state.value(QStringLiteral("verified")).toBool();}
int NativeCustodyWalletController::firstChallengeIndex() const noexcept {return m_private->state.value(QStringLiteral("indexA"),-1).toInt();}
int NativeCustodyWalletController::secondChallengeIndex() const noexcept {return m_private->state.value(QStringLiteral("indexB"),-1).toInt();}
QString NativeCustodyWalletController::address() const {return m_private->state.value(QStringLiteral("address")).toString();}
QString NativeCustodyWalletController::error() const {return m_private->state.value(QStringLiteral("error")).toString();}
QVariantMap NativeCustodyWalletController::browserSetup() const {return m_private->state;}
bool NativeCustodyWalletController::generate(){return false;} // Explicit protocol is mandatory.
bool NativeCustodyWalletController::beginBrowserWallet(QString protocol,QString words)
{
    return dispatch(QStringLiteral("begin"),std::move(protocol),std::move(words));
}
bool NativeCustodyWalletController::restoreForRegistration(QString recoveryWords)
{
    recoveryWords.fill(QChar{});
    return false; // Use protocol-bound beginBrowserWallet.
}
bool NativeCustodyWalletController::startBackupChallenge(){return dispatch(QStringLiteral("challenge"));}
bool NativeCustodyWalletController::verifyBackup(QString first,QString second)
{
    return dispatch(QStringLiteral("verify"),std::move(first),std::move(second));
}
void NativeCustodyWalletController::revealForBackup(){dispatch(QStringLiteral("reveal"));}
bool NativeCustodyWalletController::copyRecoveryWords(){return dispatch(QStringLiteral("copy"));}
void NativeCustodyWalletController::clearCopiedRecoveryWords(){}
void NativeCustodyWalletController::discard(){dispatch(QStringLiteral("discard"));}
bool NativeCustodyWalletController::prepareBrowserRegistration(QString protocol,QString voucher,QString username)
{
    return dispatch(QStringLiteral("prepare"),std::move(protocol),std::move(voucher),std::move(username));
}
bool NativeCustodyWalletController::stageBrowserRecovery(QString protocol,QString words)
{
    return dispatch(QStringLiteral("recovery"),std::move(protocol),std::move(words));
}
bool NativeCustodyWalletController::cancelBrowserSetup(){return dispatch(QStringLiteral("cancel"));}
bool NativeCustodyWalletController::useBrowserVoucher(QString voucher){return dispatch(QStringLiteral("voucher"),std::move(voucher));}
bool NativeCustodyWalletController::reserveBrowserFid(){return dispatch(QStringLiteral("reserve"));}
bool NativeCustodyWalletController::leaveBrowserSetup(){return dispatch(QStringLiteral("leave"));}
// Native-only signature ports remain fail closed. WASM uses the independently
// validated JS protocol implementation, not a raw C++ signer fallback.
std::optional<QByteArray> NativeCustodyWalletController::sealNativeOnboard(
    std::uint64_t,const QString&,const QString&,std::uint32_t) const{return std::nullopt;}
std::optional<QString> NativeCustodyWalletController::signReservedFarcasterTransfer(
    std::uint64_t,std::uint64_t,std::uint64_t) const{return std::nullopt;}
QFuture<std::optional<QString>> NativeCustodyWalletController::signOpenProofFarcasterChallenge(
    std::uint64_t,const QString&) const{return unavailableSignature();}
QFuture<std::optional<QString>> NativeCustodyWalletController::signOpenProofWalletChallenge(
    std::uint64_t,const QString&) const{return unavailableSignature();}
std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>
NativeCustodySignerAccess::forConfirmedNativeFid(const NativeCustodyWalletController&,std::uint64_t,const QString&){return {};}
std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>
NativeCustodySignerAccess::forConfirmedFarcasterFid(const NativeCustodyWalletController&,std::uint64_t,const QString&){return {};}
NativeRecoveryWordsItem::NativeRecoveryWordsItem(QQuickItem* parent):QQuickPaintedItem{parent}{setOpaquePainting(false);}
void NativeRecoveryWordsItem::paint(QPainter* painter)
{
    if(!m_wallet||!m_wallet->recoveryVisible())return;
    std::array<char,512> buffer{};
    std::string_view phrase;
    if(!m_masked){
        const int received=EM_ASM_INT({
            const bridge=window.__genycasterSetupWallet;
            if(!bridge)return 0;
            return bridge.paint($0,$1)?1:0;
        },buffer.data(),buffer.size());
        if(received)phrase=std::string_view{buffer.data()};
        if(phrase.empty()||std::count(phrase.begin(),phrase.end(),' ')!=23){
            std::fill(buffer.begin(),buffer.end(),'\0');
            return;
        }
    }
    const int columns=width()<470?2:4,rows=24/columns;
    const qreal cellWidth=width()/columns,cellHeight=height()/rows;
    painter->setRenderHint(QPainter::TextAntialiasing,true);
    painter->setPen(m_inkColor);
    auto font=painter->font();font.setPixelSize(15);painter->setFont(font);
    std::size_t start{};
    for(int i=0;i<24;++i){
        const QRectF cell{(i%columns)*cellWidth+8,(i/columns)*cellHeight+2,cellWidth-16,cellHeight-4};
        painter->drawText(QRectF{cell.x(),cell.y(),32,cell.height()},Qt::AlignLeft|Qt::AlignVCenter,QString::number(i+1)+QStringLiteral("."));
        const auto end=phrase.find(' ',start);
        const auto token=phrase.substr(start,end==std::string_view::npos?end:end-start);
        auto word=m_masked?QStringLiteral("••••••"):QString::fromLatin1(token.data(),static_cast<qsizetype>(token.size()));
        painter->drawText(QRectF{cell.x()+32,cell.y(),std::max(0.0,cell.width()-32),cell.height()},Qt::AlignLeft|Qt::AlignVCenter,word);
        word.fill(QChar{});
        start=end==std::string_view::npos?0:end+1;
    }
    std::fill(buffer.begin(),buffer.end(),'\0');
}
NativeCustodyWalletController* NativeRecoveryWordsItem::wallet() const noexcept{return m_wallet.data();}
void NativeRecoveryWordsItem::setWallet(NativeCustodyWalletController* wallet)
{
    if(m_wallet==wallet)return;
    QObject::disconnect(m_connection);
    m_wallet=wallet;
    if(m_wallet)m_connection=QObject::connect(m_wallet,&NativeCustodyWalletController::changed,this,[this]{update();});
    update();emit walletChanged();
}
void NativeRecoveryWordsItem::setInkColor(QColor color)
{
    if(m_inkColor==color)return;
    m_inkColor=color;update();emit inkColorChanged();
}
