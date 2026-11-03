#include "genypulse_activity_controller.hpp"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrlQuery>

namespace {
constexpr qint64 kDayMs = 86'400'000;
constexpr qint64 kFarcasterWindowDays = 365;
constexpr qint64 kGithubWindowDays = 30;
constexpr qint64 kCacheMs = 10 * 60 * 1000;
constexpr int kCastPages = 40;
constexpr int kGithubPages = 3;
constexpr int kMaxResponseBytes = 1024 * 1024;

QVariantMap statusMap(const QString& status, const QString& source) {
    return {{QStringLiteral("status"), status}, {QStringLiteral("source"), source}};
}
QNetworkRequest publicRead(const QUrl& url) {
    QNetworkRequest request{url};
    request.setTransferTimeout(9000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "GenyCaster/1.0 (activity read)");
    return request;
}
qint64 isoTimestamp(const QString& stamp) {
    auto parsed=QDateTime::fromString(stamp, Qt::ISODateWithMs);
    if(!parsed.isValid())parsed=QDateTime::fromString(stamp,Qt::ISODate);
    return parsed.isValid() ? parsed.toMSecsSinceEpoch() : 0;
}
QString dayString(qint64 timestamp) {
    return QDateTime::fromMSecsSinceEpoch(timestamp, QTimeZone::UTC)
        .toString(QStringLiteral("yyyy-MM-dd"));
}
QVariantList toDays(const QVariantMap& buckets) {
    QVariantList output;
    for(auto it=buckets.cbegin();it!=buckets.cend();++it)
        output.append(QVariantMap{{QStringLiteral("date"),it.key()},
                                   {QStringLiteral("count"),it.value().toInt()}});
    return output;
}
} // namespace

GenyPulseActivityController::GenyPulseActivityController(QObject* parent)
    : QObject(parent) {}

void GenyPulseActivityController::requestFarcaster(const qulonglong fid) {
    // The profile preview and the hidden Explore drawer use one controller.
    // A transient empty profile must not abort another mounted panel's
    // verified public Cast history request or overwrite it with "idle".
    if (fid == 0) return;
    if(fid==m_fid && (m_farcasterReply ||
                      (m_castFetchedAt>0 &&
                       QDateTime::currentMSecsSinceEpoch()-m_castFetchedAt<kCacheMs)))return;
    ++m_castGeneration;
    if(m_farcasterReply){m_farcasterReply->abort();m_farcasterReply->deleteLater();m_farcasterReply=nullptr;}
    // Rebuild each window from fresh pages; previously seen hashes cannot
    // survive a refresh, including casts that were subsequently removed.
    m_castFetchedAt=0;
    m_castBuckets.clear();
    m_castCount=0;
    m_replyCount=0;
    m_castOldestObservedAt=0;
    m_castHashes.clear();
    m_fid=fid;
    if(!fid){m_farcaster=statusMap(QStringLiteral("idle"),QStringLiteral("hypersnap"));emit farcasterChanged();return;}
    m_farcaster=statusMap(QStringLiteral("loading"),QStringLiteral("hypersnap"));
    emit farcasterChanged();
    fetchFarcasterPage(fid,{},0);
}

void GenyPulseActivityController::publishFarcaster(const bool complete) {
    m_farcaster=statusMap(complete?QStringLiteral("ok"):QStringLiteral("partial"),
                          QStringLiteral("hypersnap"));
    m_farcaster.insert(QStringLiteral("fid"),QString::number(m_fid));
    m_farcaster.insert(QStringLiteral("days"),toDays(m_castBuckets));
    m_farcaster.insert(QStringLiteral("count"),m_castCount);
    m_farcaster.insert(QStringLiteral("replies"),m_replyCount);
    const qint64 windowStart=QDateTime::currentMSecsSinceEpoch()-kFarcasterWindowDays*kDayMs;
    const qint64 earliest=complete || m_castOldestObservedAt==0
        ?windowStart:std::max(windowStart,m_castOldestObservedAt);
    m_farcaster.insert(QStringLiteral("knownFromDay"),earliest/kDayMs);
    m_farcaster.insert(QStringLiteral("coverage"),complete
        ?QStringLiteral("window"):QStringLiteral("limited"));
    m_castFetchedAt=QDateTime::currentMSecsSinceEpoch();
    emit farcasterChanged();
}

void GenyPulseActivityController::fetchFarcasterPage(
    const qulonglong fid, const QString cursor, const int page) {
    QUrl url{QStringLiteral("https://haatz.quilibrium.com/v2/farcaster/feed/user/casts")};
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fid"),QString::number(fid));
    query.addQueryItem(QStringLiteral("limit"),QStringLiteral("100"));
    if(!cursor.isEmpty())query.addQueryItem(QStringLiteral("cursor"),cursor);
    url.setQuery(query);
    auto* reply=m_http.get(publicRead(url));
    m_farcasterReply=reply;
    const auto generation=m_castGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,fid,page] {
        if(m_farcasterReply==reply)m_farcasterReply=nullptr;
        const auto http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data=reply->bytesAvailable()<=kMaxResponseBytes?reply->readAll():QByteArray{};
        const bool success=http==200 && reply->error()==QNetworkReply::NoError;
        reply->deleteLater();
        if(generation!=m_castGeneration || m_fid!=fid)return;
        const auto json=QJsonDocument::fromJson(data);
        if(!success || !json.isObject() || !json.object().value(QStringLiteral("casts")).isArray()){
            if(m_castCount>0)publishFarcaster(false);
            else {m_farcaster=statusMap(QStringLiteral("error"),QStringLiteral("hypersnap"));emit farcasterChanged();}
            return;
        }
        const auto casts=json.object().value(QStringLiteral("casts")).toArray();
        const qint64 cutoff=QDateTime::currentMSecsSinceEpoch()-kFarcasterWindowDays*kDayMs;
        bool reachedOld=false;
        for(const auto& item:casts) {
            if(!item.isObject())continue;
            const auto cast=item.toObject();
            const qint64 when=isoTimestamp(cast.value(QStringLiteral("timestamp")).toString());
            if(!when)continue;
            if(when<cutoff){reachedOld=true;continue;}
            if(when>QDateTime::currentMSecsSinceEpoch()+60'000)continue;
            const auto author=cast.value(QStringLiteral("author")).toObject();
            if(author.value(QStringLiteral("fid")).toVariant().toULongLong()!=fid)continue;
            const QString hash=cast.value(QStringLiteral("hash")).toString();
            if(hash.size()!=40 || m_castHashes.contains(hash))continue;
            m_castHashes.insert(hash);
            m_castOldestObservedAt=m_castOldestObservedAt==0
                ?when:std::min(m_castOldestObservedAt,when);
            ++m_castCount;
            if(!cast.value(QStringLiteral("parent_hash")).isNull()
                && !cast.value(QStringLiteral("parent_hash")).toString().isEmpty())
                ++m_replyCount;
            const auto day=dayString(when);
            m_castBuckets.insert(day,m_castBuckets.value(day).toInt()+1);
        }
        const auto next=json.object().value(QStringLiteral("next")).toObject()
                        .value(QStringLiteral("cursor")).toString();
        if(next.isEmpty()||casts.isEmpty()||reachedOld){publishFarcaster(true);return;}
        if(page+1>=kCastPages){publishFarcaster(false);return;}
        // Share partial, clearly marked progress while fetching a whole year.
        if((page+1)%4==0)publishFarcaster(false);
        fetchFarcasterPage(fid,next,page+1);
    });
}

void GenyPulseActivityController::requestGitHub(const QString& username) {
    const static QRegularExpression pattern{
        QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")};
    const QString login=username.trimmed();
    if(login.compare(m_githubLogin,Qt::CaseInsensitive)==0 &&
       (m_githubReply||(m_githubFetchedAt>0&&
          QDateTime::currentMSecsSinceEpoch()-m_githubFetchedAt<kCacheMs)))return;
    ++m_githubGeneration;
    if(m_githubReply){m_githubReply->abort();m_githubReply->deleteLater();m_githubReply=nullptr;}
    if(m_githubResolveReply){m_githubResolveReply->abort();m_githubResolveReply->deleteLater();m_githubResolveReply=nullptr;}
    m_githubAccountId.clear();
    // Recompute rather than merging stale public events after cache expiry.
    m_githubFetchedAt=0;
    m_githubBuckets.clear();
    m_githubEvents.clear();
    m_eventCount=0;
    m_githubLogin=login;
    if(login.isEmpty()||!pattern.match(login).hasMatch()){
        m_github=statusMap(QStringLiteral("unlinked"),QStringLiteral("github-events"));
        emit githubChanged();return;
    }
    m_github=statusMap(QStringLiteral("loading"),QStringLiteral("github-events"));
    emit githubChanged();
    fetchGitHubPage(login,1);
}

void GenyPulseActivityController::requestGitHubId(const QString& accountId) {
    const static QRegularExpression numeric{
        QStringLiteral("^[1-9][0-9]{0,14}$")};
    const QString id=accountId.trimmed();
    if(!numeric.match(id).hasMatch()) {
        requestGitHub({});
        return;
    }
    if(id==m_githubAccountId &&
       (m_githubResolveReply || m_githubReply ||
        (m_githubFetchedAt>0 &&
         QDateTime::currentMSecsSinceEpoch()-m_githubFetchedAt<kCacheMs)))return;

    ++m_githubGeneration;
    if(m_githubReply){m_githubReply->abort();m_githubReply->deleteLater();m_githubReply=nullptr;}
    if(m_githubResolveReply){m_githubResolveReply->abort();m_githubResolveReply->deleteLater();m_githubResolveReply=nullptr;}
    m_githubAccountId=id;
    m_githubLogin.clear();
    m_githubFetchedAt=0;
    m_githubBuckets.clear();
    m_githubEvents.clear();
    m_eventCount=0;
    m_github=statusMap(QStringLiteral("loading"),QStringLiteral("github-events"));
    m_github.insert(QStringLiteral("accountId"),id);
    emit githubChanged();

    // /user/{id} is GitHub's official public lookup by immutable numeric ID,
    // not an inference from a profile bio or mutable display name.
    const QUrl url{QStringLiteral("https://api.github.com/user/%1").arg(id)};
    auto req=publicRead(url);
    req.setRawHeader("X-GitHub-Api-Version","2022-11-28");
    auto* reply=m_http.get(req);
    m_githubResolveReply=reply;
    const auto generation=m_githubGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,id] {
        if(m_githubResolveReply==reply)m_githubResolveReply=nullptr;
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data=reply->bytesAvailable()<=32'768?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(generation!=m_githubGeneration || m_githubAccountId!=id)return;
        const auto doc=QJsonDocument::fromJson(data);
        const auto obj=doc.object();
        const QString confirmedId=QString::number(
            obj.value(QStringLiteral("id")).toVariant().toLongLong());
        const QString login=obj.value(QStringLiteral("login")).toString();
        static const QRegularExpression validLogin{
            QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")};
        if(http!=200 || !doc.isObject() || confirmedId!=id ||
           !validLogin.match(login).hasMatch()) {
            m_github=statusMap(http==403 || http==429
                ?QStringLiteral("rate-limited"):QStringLiteral("error"),
                QStringLiteral("github-events"));
            m_github.insert(QStringLiteral("accountId"),id);
            emit githubChanged();
            return;
        }
        requestGitHub(login);
        // requestGitHub starts the event read synchronously. Preserve the
        // matching provider ID for the eventually published snapshot.
        m_githubAccountId=id;
    });
}

void GenyPulseActivityController::publishGitHub(const bool complete) {
    m_github=statusMap(complete?QStringLiteral("ok"):QStringLiteral("partial"),
                        QStringLiteral("github-events"));
    m_github.insert(QStringLiteral("username"),m_githubLogin);
    if(!m_githubAccountId.isEmpty())
        m_github.insert(QStringLiteral("accountId"),m_githubAccountId);
    m_github.insert(QStringLiteral("days"),toDays(m_githubBuckets));
    m_github.insert(QStringLiteral("count"),m_eventCount);
    m_github.insert(QStringLiteral("coverage"),complete
        ?QStringLiteral("last30d"):QStringLiteral("last30d-limited"));
    m_githubFetchedAt=QDateTime::currentMSecsSinceEpoch();
    emit githubChanged();
}

void GenyPulseActivityController::fetchGitHubPage(const QString& username,const int page) {
    QUrl url{QStringLiteral("https://api.github.com/users/%1/events/public").arg(username)};
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("per_page"),QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("page"),QString::number(page));
    url.setQuery(query);
    auto request=publicRead(url);
    request.setRawHeader("X-GitHub-Api-Version","2022-11-28");
    auto* reply=m_http.get(request);
    m_githubReply=reply;
    const auto generation=m_githubGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,username,page] {
        if(m_githubReply==reply)m_githubReply=nullptr;
        const auto http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data=reply->bytesAvailable()<=kMaxResponseBytes?reply->readAll():QByteArray{};
        const bool success=http==200&&reply->error()==QNetworkReply::NoError;
        reply->deleteLater();
        if(generation!=m_githubGeneration || username!=m_githubLogin)return;
        const auto json=QJsonDocument::fromJson(data);
        if(!success||!json.isArray()){
            if(m_eventCount>0)publishGitHub(false);
            else {
                m_github=statusMap(http==403 || http==429 ? QStringLiteral("rate-limited")
                    : http==404 ? QStringLiteral("not-found") : QStringLiteral("error"),
                    QStringLiteral("github-events"));
                m_github.insert(QStringLiteral("username"),username);
                if(!m_githubAccountId.isEmpty())
                    m_github.insert(QStringLiteral("accountId"),m_githubAccountId);
                emit githubChanged();
            }
            return;
        }
        const auto items=json.array();
        const qint64 cutoff=QDateTime::currentMSecsSinceEpoch()-kGithubWindowDays*kDayMs;
        bool reachedOld=false;
        for(const auto& item:items){
            if(!item.isObject())continue;
            const auto event=item.toObject();
            const auto actor=event.value(QStringLiteral("actor")).toObject()
                                  .value(QStringLiteral("login")).toString();
            if(actor.compare(username,Qt::CaseInsensitive)!=0)continue;
            const qint64 when=isoTimestamp(event.value(QStringLiteral("created_at")).toString());
            if(!when)continue;
            if(when<cutoff){reachedOld=true;continue;}
            const auto id=event.value(QStringLiteral("id")).toString();
            if(id.isEmpty()||m_githubEvents.contains(id))continue;
            m_githubEvents.insert(id);
            ++m_eventCount;
            const auto day=dayString(when);
            m_githubBuckets.insert(day,m_githubBuckets.value(day).toInt()+1);
        }
        if(items.size()<100||reachedOld){publishGitHub(true);return;}
        if(page>=kGithubPages){publishGitHub(false);return;}
        fetchGitHubPage(username,page+1);
    });
}

void GenyPulseActivityController::requestGitHubContributions(const QString& username,
                                                              const int year) {
    static const QRegularExpression validLogin{
        QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")};
    const auto login=username.trimmed();
    const int currentYear=QDate::currentDate().year();
    const bool valid=validLogin.match(login).hasMatch() &&
        year>=2014 && year<=currentYear;
    if(valid && login.compare(m_githubCalendarLogin,Qt::CaseInsensitive)==0
       && year==m_githubCalendarYear &&
       (m_githubContributionsReply ||
        (m_githubCalendarFetchedAt>0 &&
         QDateTime::currentMSecsSinceEpoch()-m_githubCalendarFetchedAt<kCacheMs)))
        return;

    ++m_githubCalendarGeneration;
    if(m_githubContributionsReply) {
        m_githubContributionsReply->abort();
        m_githubContributionsReply->deleteLater();
        m_githubContributionsReply=nullptr;
    }
    m_githubCalendarLogin=login;
    m_githubCalendarYear=year;
    m_githubCalendarFetchedAt=0;
    if(!valid) {
        m_githubContributions=statusMap(QStringLiteral("idle"),
                                         QStringLiteral("github-public-contributions"));
        emit githubContributionsChanged();
        return;
    }
    m_githubContributions=statusMap(QStringLiteral("loading"),
                                    QStringLiteral("github-public-contributions"));
    m_githubContributions.insert(QStringLiteral("username"),login);
    m_githubContributions.insert(QStringLiteral("year"),year);
    emit githubContributionsChanged();

    // Same-origin, account-bound GenyCaster service parses GitHub's public
    // calendar. Neither GitHub credentials nor page HTML go into QML.
    QUrl url{QStringLiteral("https://caster.genyleap.com/api/v1/cloud/github_contributions")};
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("username"),login);
    query.addQueryItem(QStringLiteral("year"),QString::number(year));
    url.setQuery(query);
    auto req=publicRead(url);
    auto* reply=m_http.get(req);
    m_githubContributionsReply=reply;
    const quint64 generation=m_githubCalendarGeneration;
    connect(reply,&QNetworkReply::finished,this,
            [this,reply,generation,login,year] {
        if(m_githubContributionsReply==reply)m_githubContributionsReply=nullptr;
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data=reply->bytesAvailable()<=96*1024
            ?reply->readAll():QByteArray{};
        const bool ok=http==200 && reply->error()==QNetworkReply::NoError;
        reply->deleteLater();
        if(generation!=m_githubCalendarGeneration || year!=m_githubCalendarYear ||
           login.compare(m_githubCalendarLogin,Qt::CaseInsensitive)!=0)return;
        const auto parsed=QJsonDocument::fromJson(data);
        const auto object=parsed.object();
        const auto list=object.value(QStringLiteral("days")).toArray();
        if(!ok || !parsed.isObject() ||
           object.value(QStringLiteral("source")).toString()!=
               QStringLiteral("github-public-contributions") ||
           object.value(QStringLiteral("username")).toString()
               .compare(login,Qt::CaseInsensitive)!=0 ||
           object.value(QStringLiteral("year")).toInt()!=year ||
           list.size()<20 || list.size()>366) {
            m_githubContributions=statusMap(http==401 || http==403
                ?QStringLiteral("authorization-required")
                :QStringLiteral("error"),QStringLiteral("github-public-contributions"));
            m_githubContributions.insert(QStringLiteral("username"),login);
            m_githubContributions.insert(QStringLiteral("year"),year);
            emit githubContributionsChanged();
            return;
        }
        QVariantList days;
        const auto prefix=QString::number(year)+QStringLiteral("-");
        for(const auto& raw:list) {
            if(!raw.isObject())continue;
            const auto item=raw.toObject();
            const auto date=item.value(QStringLiteral("date")).toString();
            const int count=item.value(QStringLiteral("count")).toInt(-1);
            const int level=item.value(QStringLiteral("level")).toInt(-1);
            if(!date.startsWith(prefix) || date.size()!=10 ||
               count<0 || level<0 || level>4)continue;
            days.push_back(QVariantMap{{QStringLiteral("date"),date},
                {QStringLiteral("count"),count},{QStringLiteral("level"),level}});
        }
        if(days.size()!=list.size()){
            m_githubContributions=statusMap(QStringLiteral("error"),
                QStringLiteral("github-public-contributions"));
        }else{
            m_githubContributions=statusMap(QStringLiteral("ok"),
                QStringLiteral("github-public-contributions"));
            m_githubContributions.insert(QStringLiteral("days"),days);
            m_githubContributions.insert(QStringLiteral("count"),
                object.value(QStringLiteral("count")).toInt());
            m_githubContributions.insert(QStringLiteral("observedDays"),
                object.value(QStringLiteral("observedDays")).toInt());
            m_githubCalendarFetchedAt=QDateTime::currentMSecsSinceEpoch();
        }
        m_githubContributions.insert(QStringLiteral("username"),login);
        m_githubContributions.insert(QStringLiteral("year"),year);
        emit githubContributionsChanged();
    });
}

void GenyPulseActivityController::setVerifiedWallet(const QString& address) {
    const static QRegularExpression pattern{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    const QString valid=pattern.match(address).hasMatch()?address.toLower():QString{};
    if(valid==m_wallet)return;
    m_wallet=valid;
    // A verified wallet is an identity proof, not a complete multichain
    // transaction feed. Chain-specific provider access must be configured
    // before a transaction counter can be truthfully published.
    m_onchain=statusMap(m_wallet.isEmpty()?QStringLiteral("unlinked")
              :QStringLiteral("not-indexed"),QStringLiteral("onchain"));
    emit onchainChanged();
}
