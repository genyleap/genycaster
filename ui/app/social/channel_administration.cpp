#include "channel_administration.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace {
constexpr qsizetype maxReplyBytes = 1024 * 1024;
constexpr int requestTimeoutMs = 11000;

QString stringField(const QJsonObject& obj, std::initializer_list<const char*> names)
{
    for (const auto* name : names) {
        const QString candidate = obj.value(QLatin1StringView{name}).toString().trimmed();
        if (!candidate.isEmpty()) return candidate;
    }
    return {};
}
bool validUuid(const QString& value)
{
    static const QRegularExpression re{
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-8][0-9a-fA-F]{3}-[89a-fA-F][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$")};
    return re.match(value).hasMatch();
}
}

ChannelAdministrationController::ChannelAdministrationController(
    QString neynarApiKey, QString neynarSignerUuid, bool networkEnabled,
    bool directProxy, QObject* parent)
    : QObject(parent),
      m_neynarApiKey(std::move(neynarApiKey)),
      m_neynarSignerUuid(std::move(neynarSignerUuid)),
      m_networkEnabled(networkEnabled)
{
    if (directProxy) m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    if (m_neynarApiKey.size() > 256 || m_neynarApiKey.contains(QLatin1Char('\r'))
        || m_neynarApiKey.contains(QLatin1Char('\n')))
        m_neynarApiKey.clear();
    if (!validUuid(m_neynarSignerUuid)) m_neynarSignerUuid.clear();
}

bool ChannelAdministrationController::validChannelId(const QString& value) const
{
    static const QRegularExpression re{
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$")};
    return re.match(value).hasMatch();
}

bool ChannelAdministrationController::followsAvailable() const noexcept
{
    // HyperSnap's documented /user-active route means activity, not follows.
    // Reusing it as 'Following' would be factually wrong.
    return m_networkEnabled && m_contract == QLatin1String("neynar-v2")
        && !m_neynarApiKey.isEmpty();
}

bool ChannelAdministrationController::invitesAvailable() const noexcept
{
    // Hypersnap's compatibility endpoint returns an empty list even for users
    // with real invites in other networks. Never turn that into 'no invites'.
    return followsAvailable() && m_viewerFid > 0;
}

QString ChannelAdministrationController::providerNotice() const
{
    if (m_contract == QLatin1String("hypersnap-v2"))
        return QStringLiteral("HyperSnap: channel follow and invite REST writes return 501. Only supported, signed protocol writes can change network state.");
    if (m_contract == QLatin1String("neynar-v2")) {
        if (m_neynarApiKey.isEmpty())
            return QStringLiteral("Neynar requires an API credential for channel management.");
        if (m_neynarSignerUuid.isEmpty())
            return QStringLiteral("Neynar management requires a separately approved managed signer for this account.");
        if (!m_signerApproved)
            return QStringLiteral("Neynar signer is not approved for the active account. Writes are unavailable.");
        return QStringLiteral("Neynar managed signer verified for this account.");
    }
    return QStringLiteral("Channel management is unavailable for this read-provider contract.");
}

void ChannelAdministrationController::setMessage(QString value)
{
    if (m_message == value) return;
    m_message = std::move(value);
    emit changed();
}

void ChannelAdministrationController::setBusy(bool value)
{
    if (m_busy == value) return;
    m_busy = value;
    emit changed();
}

void ChannelAdministrationController::beginRead()
{
    ++m_activeReads;
    setBusy(true);
}

void ChannelAdministrationController::finishRead()
{
    if (m_activeReads > 0) --m_activeReads;
    setBusy(m_activeReads > 0);
}

void ChannelAdministrationController::configureSession(
    QString contract, QString endpoint, qulonglong viewerFid, QString appliedWriteProviderId)
{
    endpoint = endpoint.trimmed();
    while (endpoint.endsWith(QLatin1Char('/'))) endpoint.chop(1);
    const bool same = contract == m_contract && endpoint == m_endpoint
        && viewerFid == m_viewerFid && appliedWriteProviderId == m_writeProviderId;
    if (same) return;
    ++m_generation;
    ++m_channelGeneration;
    m_activeReads = 0;
    m_contract = std::move(contract);
    m_endpoint = std::move(endpoint);
    m_viewerFid = viewerFid;
    m_writeProviderId = std::move(appliedWriteProviderId);
    m_signerApproved = false;
    m_canModerate = false;
    m_roleVerified = false;
    m_busy = false;
    m_actionBusy = false;
    m_channelId.clear();
    m_channel.clear();
    m_following.clear();
    m_members.clear();
    m_invites.clear();
    m_message.clear();
    emit changed();
    if (followsAvailable() && !m_neynarSignerUuid.isEmpty()
        && m_writeProviderId == QLatin1String("neynar-managed"))
        verifyManagedSigner(m_generation);
}

void ChannelAdministrationController::request(
    QString method, QString path, QVariantMap query, QJsonObject body,
    quint64 generation, JsonCallback callback, bool managedWrite)
{
    if (!m_networkEnabled || generation != m_generation) {
        callback({}, QStringLiteral("Network unavailable"));
        return;
    }
    const bool neynar = m_contract == QLatin1String("neynar-v2");
    const QUrl origin{m_endpoint};
    // An API key is never forwarded to a node URL supplied by the account.
    if ((neynar && (origin.scheme() != QLatin1String("https")
                    || origin.host() != QLatin1String("api.neynar.com")
                    || m_neynarApiKey.isEmpty()))
        || (!neynar && m_contract != QLatin1String("hypersnap-v2"))
        || (!neynar && origin.scheme() != QLatin1String("https")
            && !(origin.scheme() == QLatin1String("http") && origin.isLocalFile() == false
                 && (origin.host() == QLatin1String("127.0.0.1")
                     || origin.host() == QLatin1String("localhost"))))) {
        callback({}, QStringLiteral("The selected provider has no safe channel API route"));
        return;
    }
    if (managedWrite && (!neynar || !m_signerApproved
                          || m_writeProviderId != QLatin1String("neynar-managed"))) {
        callback({}, QStringLiteral("No approved Neynar managed signer for this account"));
        return;
    }
    QUrl url{m_endpoint + QStringLiteral("/v2/farcaster/") + path
             + (neynar ? QStringLiteral("/") : QString{})};
    QUrlQuery parameters{};
    for (auto it = query.cbegin(); it != query.cend(); ++it)
        parameters.addQueryItem(it.key(), it.value().toString());
    url.setQuery(parameters);
    if (!url.isValid() || url.host() != origin.host()
        || url.scheme() != origin.scheme()) {
        callback({}, QStringLiteral("Invalid channel API URL"));
        return;
    }
    QNetworkRequest req{url};
    req.setTransferTimeout(requestTimeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Accept", "application/json");
    if (neynar) req.setRawHeader("x-api-key", m_neynarApiKey.toUtf8());
    QNetworkReply* reply = nullptr;
    if (method == QLatin1String("GET"))
        reply = m_network.get(req);
    else {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = m_network.sendCustomRequest(req, method.toUtf8(),
                                            QJsonDocument(body).toJson(QJsonDocument::Compact));
    }
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, generation, callback = std::move(callback)]() mutable {
            reply->deleteLater();
            if (generation != m_generation) return;
            const QByteArray payload = reply->readAll();
            if (payload.size() > maxReplyBytes) {
                callback({}, QStringLiteral("Channel response exceeds size limit"));
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                callback({}, QStringLiteral("Channel request failed (HTTP %1)")
                         .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
                return;
            }
            QJsonParseError parse{};
            const QJsonDocument document = QJsonDocument::fromJson(payload, &parse);
            if (parse.error != QJsonParseError::NoError || !document.isObject()) {
                callback({}, QStringLiteral("Invalid channel API response"));
                return;
            }
            callback(document.object(), {});
        });
}

void ChannelAdministrationController::get(
    QString path, QVariantMap query, quint64 generation, JsonCallback callback)
{
    request(QStringLiteral("GET"), std::move(path), std::move(query), {},
            generation, std::move(callback));
}

QVariantMap ChannelAdministrationController::projectChannel(const QJsonObject& raw)
{
    const QString id = stringField(raw, {"id"});
    if (id.isEmpty()) return {};
    const QJsonObject lead = raw.value(QStringLiteral("lead")).toObject();
    QVariantList moderators{};
    for (const auto& item : raw.value(QStringLiteral("moderator_fids")).toArray())
        if (item.toInteger(0) > 0) moderators.append(item.toInteger());
    for (const auto& item : raw.value(QStringLiteral("moderators")).toArray()) {
        const qint64 fid = item.isObject()
            ? item.toObject().value(QStringLiteral("fid")).toInteger(0) : item.toInteger(0);
        if (fid > 0 && !moderators.contains(fid)) moderators.append(fid);
    }
    const QString image = stringField(raw, {"image_url", "imageUrl"});
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), stringField(raw, {"name", "id"})},
        {QStringLiteral("description"), stringField(raw, {"description"})},
        {QStringLiteral("imageUrl"),
            image.startsWith(QStringLiteral("https://")) ? image : QString{}},
        {QStringLiteral("parentUrl"), stringField(raw, {"parent_url", "parentUrl"})},
        {QStringLiteral("followerCount"), raw.value(QStringLiteral("follower_count")).toInteger(-1)},
        {QStringLiteral("memberCount"), raw.value(QStringLiteral("member_count")).toInteger(-1)},
        {QStringLiteral("leadFid"), lead.value(QStringLiteral("fid")).toInteger(0)},
        {QStringLiteral("moderatorFids"), moderators},
        {QStringLiteral("isMember"), false},
    };
}

QVariantMap ChannelAdministrationController::projectUser(const QJsonObject& raw)
{
    const QJsonObject user = raw.value(QStringLiteral("user")).isObject()
        ? raw.value(QStringLiteral("user")).toObject() : raw;
    const qint64 fid = user.value(QStringLiteral("fid")).toInteger(0);
    if (fid <= 0) return {};
    const QString avatar = stringField(user, {"pfp_url", "avatar_url", "image_url"});
    return {
        {QStringLiteral("fid"), fid},
        {QStringLiteral("username"), stringField(user, {"username"})},
        {QStringLiteral("displayName"), stringField(user, {"display_name", "username"})},
        {QStringLiteral("avatarUrl"),
            avatar.startsWith(QStringLiteral("https://")) ? avatar : QString{}},
        {QStringLiteral("role"), stringField(raw, {"role"})},
    };
}

QVariantList ChannelAdministrationController::projectChannels(const QJsonArray& raw)
{
    QVariantList out{};
    out.reserve(std::min<qsizetype>(raw.size(), 100));
    for (const auto& item : raw) {
        if (!item.isObject()) continue;
        auto row = projectChannel(item.toObject());
        if (!row.isEmpty() && out.size() < 100) out.push_back(row);
    }
    return out;
}

QVariantList ChannelAdministrationController::projectMembers(const QJsonObject& raw)
{
    const auto items = raw.contains(QStringLiteral("members"))
        ? raw.value(QStringLiteral("members")).toArray()
        : raw.value(QStringLiteral("users")).toArray();
    QVariantList out{};
    out.reserve(std::min<qsizetype>(items.size(), 100));
    for (const auto& item : items) {
        if (!item.isObject()) continue;
        const auto row = projectUser(item.toObject());
        if (!row.isEmpty() && out.size() < 100) out.append(row);
    }
    return out;
}

QVariantList ChannelAdministrationController::projectInvites(const QJsonObject& raw)
{
    QVariantList out{};
    for (const auto& item : raw.value(QStringLiteral("invites")).toArray()) {
        if (!item.isObject() || out.size() >= 100) continue;
        const auto entry = item.toObject();
        const QString channelId = stringField(entry, {"channel_id", "channelId"}).isEmpty()
            ? stringField(entry.value(QStringLiteral("channel")).toObject(), {"id"})
            : stringField(entry, {"channel_id", "channelId"});
        if (channelId.isEmpty()) continue;
        QVariantMap invite{
            {QStringLiteral("channelId"), channelId},
            {QStringLiteral("role"), stringField(entry, {"role"})},
        };
        const auto invited = projectUser(entry.value(QStringLiteral("invited")).toObject());
        invite.insert(QStringLiteral("invited"), invited);
        out.append(invite);
    }
    return out;
}

void ChannelAdministrationController::updateRole(const QJsonObject& raw)
{
    m_roleVerified = raw.contains(QStringLiteral("lead"))
        || raw.contains(QStringLiteral("moderator_fids"))
        || raw.contains(QStringLiteral("moderators"));
    const auto lead = raw.value(QStringLiteral("lead")).toObject()
                         .value(QStringLiteral("fid")).toInteger(0);
    m_canModerate = m_viewerFid > 0 && m_roleVerified
        && (lead == static_cast<qint64>(m_viewerFid)
            || m_channel.value(QStringLiteral("moderatorFids")).toList()
                   .contains(static_cast<qint64>(m_viewerFid)));
}

void ChannelAdministrationController::verifyManagedSigner(quint64 generation)
{
    get(QStringLiteral("signer"),
        {{QStringLiteral("signer_uuid"), m_neynarSignerUuid}}, generation,
        [this, generation](const QJsonObject& body, const QString& error) {
            if (generation != m_generation) return;
            const auto signer = body.value(QStringLiteral("signer")).isObject()
                ? body.value(QStringLiteral("signer")).toObject() : body;
            m_signerApproved = error.isEmpty()
                && signer.value(QStringLiteral("status")).toString()
                    == QLatin1String("approved")
                && signer.value(QStringLiteral("fid")).toInteger(0)
                    == static_cast<qint64>(m_viewerFid)
                && signer.value(QStringLiteral("signer_uuid")).toString()
                    == m_neynarSignerUuid;
            emit changed();
        });
}

void ChannelAdministrationController::loadFollowing()
{
    m_following.clear();
    emit changed();
    if (!followsAvailable()) {
        setMessage(QStringLiteral("This provider does not expose a verified Following list."));
        return;
    }
    if (m_viewerFid == 0) {
        setMessage(QStringLiteral("Sign in to see your followed channels."));
        return;
    }
    const quint64 generation = m_generation;
    beginRead();
    get(QStringLiteral("user/channels"),
        {{QStringLiteral("fid"), QString::number(m_viewerFid)},
         {QStringLiteral("limit"), 100}}, generation,
        [this, generation](const QJsonObject& body, const QString& error) {
            if (generation != m_generation) return;
            finishRead();
            if (!error.isEmpty() || !body.contains(QStringLiteral("channels"))) {
                setMessage(error.isEmpty() ? QStringLiteral("Unable to load followed channels") : error);
                return;
            }
            m_following = projectChannels(body.value(QStringLiteral("channels")).toArray());
            setMessage({});
            emit changed();
        });
}

void ChannelAdministrationController::openChannel(QString channelId)
{
    if (!validChannelId(channelId)) {
        setMessage(QStringLiteral("Invalid channel identifier"));
        return;
    }
    ++m_channelGeneration;
    m_channelId = std::move(channelId);
    m_channel.clear();
    m_members.clear();
    m_invites.clear();
    m_roleVerified = false;
    m_canModerate = false;
    emit changed();
    const quint64 generation = m_generation;
    const quint64 channelGeneration = m_channelGeneration;
    beginRead();
    get(QStringLiteral("channel"),
        {{QStringLiteral("id"), m_channelId}}, generation,
        [this, generation, channelGeneration](const QJsonObject& body, const QString& error) {
            if (generation != m_generation) return;
            finishRead();
            if (channelGeneration != m_channelGeneration) return;
            const auto channel = body.value(QStringLiteral("channel")).toObject();
            if (!error.isEmpty() || channel.isEmpty()
                || stringField(channel, {"id"}) != m_channelId) {
                setMessage(error.isEmpty() ? QStringLiteral("Channel lookup unavailable") : error);
                return;
            }
            m_channel = projectChannel(channel);
            updateRole(channel);
            setMessage({});
            emit changed();
        });
}

void ChannelAdministrationController::loadMembers()
{
    m_members.clear();
    emit changed();
    if (!validChannelId(m_channelId)) return;
    const quint64 generation = m_generation;
    const quint64 channelGeneration = m_channelGeneration;
    const QString path = m_contract == QLatin1String("neynar-v2")
        ? QStringLiteral("channel/member/list") : QStringLiteral("channel/members");
    beginRead();
    get(path, {{QStringLiteral("channel_id"), m_channelId},
               {QStringLiteral("limit"), 100}}, generation,
        [this, generation, channelGeneration](const QJsonObject& body, const QString& error) {
            if (generation != m_generation) return;
            finishRead();
            if (channelGeneration != m_channelGeneration) return;
            if (!error.isEmpty() || (!body.contains(QStringLiteral("members"))
                                     && !body.contains(QStringLiteral("users")))) {
                setMessage(error.isEmpty() ? QStringLiteral("Channel members unavailable") : error);
                return;
            }
            m_members = projectMembers(body);
            setMessage({});
            emit changed();
        });
}

void ChannelAdministrationController::loadInvites()
{
    m_invites.clear();
    emit changed();
    if (!invitesAvailable()) {
        setMessage(QStringLiteral("Channel invitations are unavailable on this provider."));
        return;
    }
    const quint64 generation = m_generation;
    beginRead();
    QVariantMap params{
        {QStringLiteral("invited_fid"), QString::number(m_viewerFid)},
        {QStringLiteral("limit"), 100},
    };
    get(QStringLiteral("channel/member/invite/list"), params, generation,
        [this, generation](const QJsonObject& body, const QString& error) {
            if (generation != m_generation) return;
            finishRead();
            if (!error.isEmpty() || !body.contains(QStringLiteral("invites"))) {
                setMessage(error.isEmpty() ? QStringLiteral("Invitations unavailable") : error);
                return;
            }
            m_invites = projectInvites(body);
            setMessage({});
            emit changed();
        });
}

void ChannelAdministrationController::refresh()
{
    if (!m_channelId.isEmpty()) {
        const QString id = m_channelId;
        openChannel(id);
        loadMembers();
    } else {
        loadFollowing();
    }
}

bool ChannelAdministrationController::validateMutation(
    const QString& action, qulonglong targetFid) const
{
    if (!canWrite() || m_viewerFid == 0
        || m_contract != QLatin1String("neynar-v2")
        || m_writeProviderId != QLatin1String("neynar-managed")
        || !validChannelId(m_channelId))
        return false;
    if (action == QLatin1String("follow") || action == QLatin1String("unfollow"))
        return true;
    if (action == QLatin1String("invite") || action == QLatin1String("remove"))
        return m_roleVerified && m_canModerate && targetFid > 0
            && targetFid != m_viewerFid;
    return false;
}

void ChannelAdministrationController::submit(
    QString action, QString path, QJsonObject body)
{
    if (m_actionBusy || !m_signerApproved) {
        setMessage(QStringLiteral("No approved signer is available for this account"));
        return;
    }
    body.insert(QStringLiteral("signer_uuid"), m_neynarSignerUuid);
    const quint64 generation = m_generation;
    m_actionBusy = true;
    emit changed();
    QString method;
    if (action == QLatin1String("follow") || action == QLatin1String("invite"))
        method = QStringLiteral("POST");
    else if (action == QLatin1String("respond"))
        method = QStringLiteral("PUT");
    else
        method = QStringLiteral("DELETE");
    request(method, std::move(path), {}, std::move(body),
        generation,
        [this, action = std::move(action), generation](
            const QJsonObject& result, const QString& error) {
            if (generation != m_generation) return;
            m_actionBusy = false;
            const bool success = error.isEmpty()
                && result.value(QStringLiteral("success")).isBool()
                && result.value(QStringLiteral("success")).toBool();
            const QString message = success
                ? QStringLiteral("Channel action accepted by the selected provider")
                : error.isEmpty()
                    ? QStringLiteral("Channel action was not confirmed by the provider")
                    : error;
            setMessage(message);
            emit changed();
            emit actionCompleted(success, message);
            if (success) {
                // Never optimistically grant channel roles or follows.
                // Re-read authoritative data after a successful response.
                if (action == QLatin1String("respond")) loadInvites();
                else if (action == QLatin1String("follow")
                         || action == QLatin1String("unfollow")) loadFollowing();
                else loadMembers();
            }
        }, true);
}

void ChannelAdministrationController::followChannel(bool follow)
{
    const QString action = follow
        ? QStringLiteral("follow") : QStringLiteral("unfollow");
    if (!validateMutation(action, 0)) {
        setMessage(providerNotice());
        emit actionCompleted(false, m_message);
        return;
    }
    submit(action, QStringLiteral("channel/follow"),
        {{QStringLiteral("channel_id"), m_channelId}});
}

void ChannelAdministrationController::inviteMember(qulonglong targetFid)
{
    if (!validateMutation(QStringLiteral("invite"), targetFid)) {
        setMessage(QStringLiteral("A verified moderator role and Neynar signer are required to invite users"));
        emit actionCompleted(false, m_message);
        return;
    }
    submit(QStringLiteral("invite"), QStringLiteral("channel/member/invite"),
        {{QStringLiteral("channel_id"), m_channelId},
         {QStringLiteral("fid"), static_cast<qint64>(targetFid)}});
}

void ChannelAdministrationController::removeMember(qulonglong targetFid)
{
    if (!validateMutation(QStringLiteral("remove"), targetFid)) {
        setMessage(QStringLiteral("A verified moderator role and Neynar signer are required to remove users"));
        emit actionCompleted(false, m_message);
        return;
    }
    submit(QStringLiteral("remove"), QStringLiteral("channel/member"),
        {{QStringLiteral("channel_id"), m_channelId},
         {QStringLiteral("fid"), static_cast<qint64>(targetFid)}});
}

void ChannelAdministrationController::respondToInvite(QString channelId, bool accept)
{
    if (!canWrite() || !validChannelId(channelId)
        || m_contract != QLatin1String("neynar-v2")
        || m_writeProviderId != QLatin1String("neynar-managed")) {
        setMessage(providerNotice());
        emit actionCompleted(false, m_message);
        return;
    }
    // Only respond to invites returned for the active FID; a guessed channel
    // name cannot silently approve an unrelated membership.
    const auto match = std::find_if(m_invites.cbegin(), m_invites.cend(),
        [&channelId](const QVariant& entry) {
            return entry.toMap().value(QStringLiteral("channelId")).toString()
                == channelId;
        });
    if (match == m_invites.cend()) {
        setMessage(QStringLiteral("No verified pending invitation for this channel"));
        emit actionCompleted(false, m_message);
        return;
    }
    submit(QStringLiteral("respond"), QStringLiteral("channel/member/invite"),
        {{QStringLiteral("channel_id"), std::move(channelId)},
         {QStringLiteral("accept"), accept}});
}
