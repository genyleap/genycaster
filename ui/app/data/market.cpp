#include "market.hpp"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <utility>

namespace {
constexpr qint64 localCacheMs = 5 * 60 * 1000;
constexpr qint64 retryMs = 60 * 1000;
constexpr qsizetype maxPayload = 16 * 1024;
// No API credential belongs in this URL or in QML/WASM.
constexpr auto snapshotUrl = "https://caster.genyleap.com/api/v1/cloud/market";
}

MarketController::MarketController(const bool directProxy, const bool networkEnabled,
                                   QObject* parent)
    : QObject(parent), m_networkEnabled(networkEnabled) {
    if(directProxy)m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
}
MarketController::~MarketController(){
    ++m_epoch;
    if(m_reply){
        auto* reply=m_reply.data();
        m_reply.clear();
        disconnect(reply,nullptr,this,nullptr);
        reply->abort();
        reply->deleteLater();
    }
}
void MarketController::refresh(const bool force){
    if(!m_networkEnabled)return;
    const qint64 now=QDateTime::currentMSecsSinceEpoch();
    if(m_reply || now<m_retryAtMs ||
       (!force && m_lastRefreshMs>0 && now-m_lastRefreshMs<localCacheMs))return;
    QNetworkRequest request{QUrl{QString::fromLatin1(snapshotUrl)}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(12'000);
    request.setRawHeader("Accept","application/json");
    auto* const reply=m_network.get(request);
    m_reply=reply;
    m_state=QStringLiteral("loading");
    emit changed();
    const quint64 generation=++m_epoch;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation]{
        if(m_reply!=reply || generation!=m_epoch){
            reply->deleteLater();return;
        }
        m_reply.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool readable=reply->error()==QNetworkReply::NoError &&
            status==200 && reply->bytesAvailable()<=maxPayload;
        const QByteArray bytes=readable?reply->readAll():QByteArray{};
        reply->deleteLater();
        const auto document=QJsonDocument::fromJson(bytes);
        const auto payload=document.object();
        const auto raw=payload.value(QStringLiteral("metrics")).toObject();
        if(readable && document.isObject() && !raw.isEmpty()){
            // Only structured per-indicator records are exposed to QML.
            QVariantMap result;
            for(auto it=raw.begin();it!=raw.end();++it)
                if(it.value().isObject())result.insert(it.key(),it.value().toObject().toVariantMap());
            if(!result.isEmpty()){
                m_metrics=std::move(result);
                m_partial=payload.value(QStringLiteral("partial")).toBool(true);
                m_planConfigured=payload.value(QStringLiteral("plan_configured")).toBool();
                m_updatedAt=QDateTime::currentDateTime().toString(
                    QStringLiteral("yyyy-MM-dd HH:mm"));
                m_lastRefreshMs=QDateTime::currentMSecsSinceEpoch();
                m_retryAtMs=0;
                m_state=m_partial?QStringLiteral("partial"):QStringLiteral("ready");
                emit changed();
                return;
            }
        }
        m_retryAtMs=QDateTime::currentMSecsSinceEpoch()+retryMs;
        m_state=m_metrics.isEmpty()?QStringLiteral("error"):QStringLiteral("partial");
        m_partial=true;
        emit changed();
    });
}
