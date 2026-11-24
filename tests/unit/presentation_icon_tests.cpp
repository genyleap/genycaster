#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QVariantMap>

#include <algorithm>
#include <cassert>

#include "presentation_bridge.hpp"

namespace {

void testFollowingStoriesKeepOwnFirstAndOlderFollowees()
{
    PresentationBridge bridge{};
    bridge.applyAuthenticationState(
        true, QStringLiteral("authenticated"), 42,
        QStringLiteral("viewer"), QStringLiteral("Viewer"), {}, {}, {}, {});
    bridge.refreshFollowingStories();

    bridge.applyFollowingStoryProfiles(42, {
        QVariantMap{{QStringLiteral("fid"), 12U},
                    {QStringLiteral("username"), QStringLiteral("first")}},
        QVariantMap{{QStringLiteral("fid"), 15U},
                    {QStringLiteral("username"), QStringLiteral("second")}},
        QVariantMap{{QStringLiteral("fid"), 77U},
                    {QStringLiteral("username"), QStringLiteral("inactive")}},
    }, true, true);

    const QString firstCast = QStringLiteral("a").repeated(40);
    const QString secondCast = QStringLiteral("b").repeated(40);
    bridge.applyFollowingStoryActivities(42, {
        QVariantMap{{QStringLiteral("fid"), 15U},
                    {QStringLiteral("latestCastId"), secondCast},
                    {QStringLiteral("createdAtMs"), 1000LL}},
        QVariantMap{{QStringLiteral("fid"), 12U},
                    {QStringLiteral("latestCastId"), firstCast},
                    {QStringLiteral("createdAtMs"), 2000LL},
                    {QStringLiteral("activityKind"), QStringLiteral("reply")}},
        // This author is not followed and must never become a public story.
        QVariantMap{{QStringLiteral("fid"), 123U},
                    {QStringLiteral("latestCastId"), firstCast},
                    {QStringLiteral("createdAtMs"), 4000LL}},
        QVariantMap{{QStringLiteral("fid"), 77U},
                    {QStringLiteral("latestCastId"), QStringLiteral("invalid")}},
    }, true, true);

    const auto first = bridge.followingStoryProfiles();
    assert(first.size() == 4);
    // Own profile is always first, including when the account has no cast.
    assert(first[0].toMap().value(QStringLiteral("fid")).toULongLong() == 42U);
    assert(first[0].toMap().value(QStringLiteral("isSelf")).toBool());
    assert(first[1].toMap().value(QStringLiteral("username")).toString()
        == QStringLiteral("first"));
    assert(first[1].toMap().value(QStringLiteral("latestCastId")).toString()
        == QStringLiteral("0x") + firstCast);
    assert(first[1].toMap().value(QStringLiteral("activityKind")).toString()
        == QStringLiteral("reply"));
    assert(first[2].toMap().value(QStringLiteral("username")).toString()
        == QStringLiteral("second"));
    // The inactive followed account remains visible rather than disappearing.
    assert(first[3].toMap().value(QStringLiteral("username")).toString()
        == QStringLiteral("inactive"));
    assert(first[3].toMap().value(QStringLiteral("createdAtMs")).toLongLong() == 0LL);
    assert(std::none_of(first.cbegin(), first.cend(), [](const QVariant& item) {
        return item.toMap().value(QStringLiteral("fid")).toULongLong() == 123U;
    }));

    // A newer observed cast moves a previously older follow to the front.
    bridge.applyFollowingStoryActivities(42, {
        QVariantMap{{QStringLiteral("fid"), 15U},
                    {QStringLiteral("latestCastId"), secondCast},
                    {QStringLiteral("createdAtMs"), 3000LL}},
    }, true, true);
    assert(bridge.followingStoryProfiles()[0].toMap().value(QStringLiteral("isSelf")).toBool());
    assert(bridge.followingStoryProfiles()[1].toMap()
        .value(QStringLiteral("username")).toString() == QStringLiteral("second"));

    // One failed refresh must not make a known clickable activity disappear.
    bridge.applyFollowingStoryActivities(42, {}, true, false);
    assert(bridge.followingStoryProfiles().size() == 4);

    // An account switch invalidates both the follow graph and its activity.
    bridge.applyAuthenticationState(
        true, QStringLiteral("authenticated"), 43,
        QStringLiteral("other"), QStringLiteral("Other"), {}, {}, {}, {});
    assert(bridge.followingStoryProfiles().isEmpty());
    bridge.refreshFollowingStories();
    assert(bridge.followingStoryProfiles().size() == 1);
    assert(bridge.followingStoryProfiles().front().toMap()
        .value(QStringLiteral("fid")).toULongLong() == 43U);
}

void testContextualReactionPriorityAndBoundaries()
{
    PresentationBridge bridge{};

    assert(bridge.contextualReactionIcon(QStringLiteral("Building Genyleap for Farcaster"))
        == QStringLiteral("brands/genyleap"));
    assert(bridge.contextualReactionIcon(QStringLiteral("Geny ships on FC"))
        == QStringLiteral("brands/geny"));
    assert(bridge.contextualReactionIcon(QStringLiteral("یک کلاینت تازه برای FC"))
        == QStringLiteral("brands/farcaster"));
    assert(bridge.contextualReactionIcon(QStringLiteral("effect sizes"))
        == QStringLiteral("actions/heart"));
    assert(bridge.contextualReactionIcon(QStringLiteral("C++ on Linux"))
        == QStringLiteral("languages/cplusplus"));
}


void testPaginationPreservesHydratedReferencedContext()
{
    PresentationBridge bridge{};
    const QVariantMap hydratedParent{
        {QStringLiteral("castId"), QStringLiteral("0xparent")},
        {QStringLiteral("displayName"), QStringLiteral("Parent")},
        {QStringLiteral("text"), QStringLiteral("Hydrated parent context")},
    };
    const QVariantMap initialRow{
        {QStringLiteral("castId"), QStringLiteral("0xchild")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xparent")},
        {QStringLiteral("parentPreview"), hydratedParent},
        {QStringLiteral("_referencedRevision"), QStringLiteral("parent-revision")},
    };
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {initialRow}, QStringLiteral("next-1"),
        {}, {}, {}, false, false, false, false);

    const QVariantMap cacheRow{
        {QStringLiteral("castId"), QStringLiteral("0xchild")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xparent")},
        {QStringLiteral("parentPreview"), QVariantMap{}},
    };
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {cacheRow}, QStringLiteral("next-1"),
        QStringLiteral("Loading more"), {}, {}, true, true, false, true);
    assert(bridge.rows().size() == 1);
    assert(bridge.rows().front().toMap().value(QStringLiteral("parentPreview")).toMap()
        .value(QStringLiteral("text")).toString() == QStringLiteral("Hydrated parent context"));

    const QVariantMap nextRow{{QStringLiteral("castId"), QStringLiteral("0xnext")}};
    // Even a degraded terminal continuation snapshot containing only the new
    // provider page must reconcile with the already-visible timeline.
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {nextRow}, QStringLiteral("next-2"),
        {}, {}, {}, false, false, false, true);
    assert(bridge.rows().size() == 2);
    assert(bridge.rows().at(1).toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xnext"));
    const QVariantMap retained = bridge.rows().front().toMap();
    assert(retained.value(QStringLiteral("parentPreview")).toMap()
        .value(QStringLiteral("text")).toString() == QStringLiteral("Hydrated parent context"));
    assert(retained.value(QStringLiteral("_referencedRevision")).toString()
        == QStringLiteral("parent-revision"));
}


void testIncrementalPaginationDecoratesOnlyNewOrRevisedRows()
{
    PresentationBridge bridge{};
    QVariantList initial{};
    initial.reserve(180);
    for (int index = 0; index < 180; ++index) {
        initial.push_back(QVariantMap{
            {QStringLiteral("castId"), QStringLiteral("0xfeed-%1").arg(index)},
            {QStringLiteral("text"), QStringLiteral("Original item %1").arg(index)},
            {QStringLiteral("embeds"), QVariantList{QVariantMap{
                {QStringLiteral("kind"), QStringLiteral("image")},
                {QStringLiteral("url"), QStringLiteral("https://example.invalid/%1").arg(index)},
            }}},
        });
    }
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("recent"), initial, QStringLiteral("p2"),
        {}, {}, {}, false, false, false, false);
    bridge.applyViewerReactionDelta({
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xfeed-0")},
                    {QStringLiteral("liked"), true},
                    {QStringLiteral("recasted"), false}},
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xnext")},
                    {QStringLiteral("liked"), true},
                    {QStringLiteral("recasted"), false}},
    });
    const QVariantMap original = bridge.rows().first().toMap();
    assert(original.value(QStringLiteral("likedByMe")).toBool());

    bridge.applyReferencedCastPreview(QStringLiteral("0xparent"),
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xparent")},
                    {QStringLiteral("text"), QStringLiteral("Loaded parent")}});
    const QVariantMap next{
        {QStringLiteral("castId"), QStringLiteral("0xnext")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xparent")},
        {QStringLiteral("parentPreview"), QVariantMap{}},
        {QStringLiteral("likeCount"), 0},
    };
    // An incremental page must keep the rich prefix intact and decorate only
    // the incoming row from existing reaction and reference caches.
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("recent"), {next}, QStringLiteral("p3"),
        {}, {}, {}, false, false, false, true);
    assert(bridge.rows().size() == 181);
    assert(bridge.rows().first().toMap() == original);
    const QVariantMap appended = bridge.rows().last().toMap();
    assert(appended.value(QStringLiteral("likedByMe")).toBool());
    assert(appended.value(QStringLiteral("likeCount")).toULongLong() == 1);
    assert(appended.value(QStringLiteral("parentPreview")).toMap()
        .value(QStringLiteral("text")).toString() == QStringLiteral("Loaded parent"));

    // A duplicate arriving in a later slice must update only its old index,
    // preserving the revision of a previously hydrated item and the ordering.
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("recent"),
        {QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xfeed-1")},
                     {QStringLiteral("text"), QStringLiteral("Revised second item")}},
         QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xnext")},
                     {QStringLiteral("parentCastId"), QStringLiteral("0xparent")}}},
        {}, {}, {}, {}, false, false, false, true);
    assert(bridge.rows().size() == 181);
    assert(bridge.rows().first().toMap() == original);
    assert(bridge.rows().at(1).toMap().value(QStringLiteral("text")).toString()
        == QStringLiteral("Revised second item"));
    assert(bridge.rows().last().toMap().value(QStringLiteral("likedByMe")).toBool());
    assert(bridge.rows().last().toMap().value(QStringLiteral("parentPreview")).toMap()
        .value(QStringLiteral("text")).toString() == QStringLiteral("Loaded parent"));
}

void testPaginationPreservesResolvedMediaOnlyReferencedContext()
{
    PresentationBridge bridge{};
    const QVariantMap mediaOnlyParent{
        {QStringLiteral("castId"), QStringLiteral("0xmedia-parent")},
        {QStringLiteral("resolved"), true},
    };
    const QVariantMap initialRow{
        {QStringLiteral("castId"), QStringLiteral("0xmedia-child")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xmedia-parent")},
        {QStringLiteral("parentPreview"), mediaOnlyParent},
        {QStringLiteral("_referencedRevision"), QStringLiteral("media-parent-revision")},
    };
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {initialRow}, QStringLiteral("next-1"),
        {}, {}, {}, false, false, false, false);

    const QVariantMap cacheRow{
        {QStringLiteral("castId"), QStringLiteral("0xmedia-child")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xmedia-parent")},
        {QStringLiteral("parentPreview"), QVariantMap{
            {QStringLiteral("castId"), QStringLiteral("0xmedia-parent")},
        }},
    };
    const QVariantMap nextRow{{QStringLiteral("castId"), QStringLiteral("0xnext")}};
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {cacheRow, nextRow}, QStringLiteral("next-2"),
        {}, {}, {}, false, false, false, true);

    const QVariantMap retained = bridge.rows().front().toMap();
    assert(retained.value(QStringLiteral("parentPreview")).toMap()
        .value(QStringLiteral("resolved")).toBool());
    assert(retained.value(QStringLiteral("_referencedRevision")).toString()
        == QStringLiteral("media-parent-revision"));
}


void testReadPreferencesAreBoundedAndPersisted()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("presentation.ini"));
    QSettings settings{settingsPath, QSettings::IniFormat};
    settings.setValue(QStringLiteral("read/feedPageSize"), 500);
    settings.setValue(QStringLiteral("read/contextHydrationLimit"), -3);
    settings.setValue(QStringLiteral("read/linkPreviewPrefetchEnabled"), false);
    settings.setValue(QStringLiteral("read/inlineYouTubeEnabled"), false);
    settings.setValue(QStringLiteral("read/bufferNewPosts"), true);

    PresentationBridge bridge{nullptr, &settings};
    assert(bridge.feedPageSize() == 100);
    assert(bridge.contextHydrationLimit() == 1);
    assert(!bridge.linkPreviewPrefetchEnabled());
    assert(!bridge.inlineYouTubeEnabled());
    assert(bridge.bufferNewPosts());

    bridge.setFeedPageSize(37);
    bridge.setContextHydrationLimit(52);
    bridge.setLinkPreviewPrefetchEnabled(true);
    bridge.setInlineYouTubeEnabled(true);
    bridge.setBufferNewPosts(false);
    settings.sync();

    assert(settings.value(QStringLiteral("read/feedPageSize")).toInt() == 37);
    assert(settings.value(QStringLiteral("read/contextHydrationLimit")).toInt() == 52);
    assert(settings.value(QStringLiteral("read/linkPreviewPrefetchEnabled")).toBool());
    assert(settings.value(QStringLiteral("read/inlineYouTubeEnabled")).toBool());
    assert(!settings.value(QStringLiteral("read/bufferNewPosts")).toBool());
}

void testBufferedRefreshPreservesTimelineUntilReveal()
{
    PresentationBridge bridge{};
    const QVariantMap existing{
        {QStringLiteral("castId"), QStringLiteral("0xexisting")},
        {QStringLiteral("text"), QStringLiteral("Visible cast")},
        {QStringLiteral("likeCount"), 2},
    };
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {existing}, {},
        {}, {}, {}, false, false, false, false);

    bridge.retry();
    const QVariantMap refreshedExisting{
        {QStringLiteral("castId"), QStringLiteral("0xexisting")},
        {QStringLiteral("text"), QStringLiteral("Visible cast")},
        {QStringLiteral("likeCount"), 5},
    };
    const QVariantMap newCast{
        {QStringLiteral("castId"), QStringLiteral("0xnew")},
        {QStringLiteral("text"), QStringLiteral("New cast")},
        {QStringLiteral("likeCount"), 1},
    };
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {newCast, refreshedExisting}, {},
        {}, {}, {}, false, false, false, false);

    assert(bridge.pendingNewPostCount() == 1);
    assert(bridge.rows().size() == 1);
    assert(bridge.rows().front().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xexisting"));
    assert(bridge.rows().front().toMap().value(QStringLiteral("likeCount")).toInt() == 5);

    bridge.applyViewerReactionDelta({QVariantMap{
        {QStringLiteral("castId"), QStringLiteral("0xnew")},
        {QStringLiteral("liked"), true},
        {QStringLiteral("recasted"), false},
    }});
    bridge.revealPendingPosts();

    assert(bridge.pendingNewPostCount() == 0);
    assert(bridge.rows().size() == 2);
    const QVariantMap revealed = bridge.rows().front().toMap();
    assert(revealed.value(QStringLiteral("castId")).toString() == QStringLiteral("0xnew"));
    assert(revealed.value(QStringLiteral("likedByMe")).toBool());
}

void testReferencedPreviewHydratesReplyAndQuoteMetrics()
{
    PresentationBridge bridge{};
    const QVariantMap replyRow{
        {QStringLiteral("castId"), QStringLiteral("0xreply")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xparent")},
        {QStringLiteral("parentPreview"), QVariantMap{}},
    };
    const QVariantMap quoteRow{
        {QStringLiteral("castId"), QStringLiteral("0xquote")},
        {QStringLiteral("embeds"), QVariantList{QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("cast-reference")},
            {QStringLiteral("castId"), QStringLiteral("0xparent")},
        }}},
    };
    bridge.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"), {replyRow, quoteRow}, {},
        {}, {}, {}, false, false, false, false);

    const QVariantMap preview{
        {QStringLiteral("castId"), QStringLiteral("0xparent")},
        {QStringLiteral("displayName"), QStringLiteral("Original author")},
        {QStringLiteral("text"), QStringLiteral("Original cast body")},
        {QStringLiteral("replyCount"), 7},
        {QStringLiteral("recastCount"), 3},
        {QStringLiteral("likeCount"), 19},
    };
    int changes = 0;
    QObject::connect(&bridge, &PresentationBridge::stateChanged, &bridge,
        [&changes] { ++changes; });
    bridge.applyReferencedCastPreview(QStringLiteral("0xparent"), preview);
    assert(changes == 1);

    const QVariantMap hydratedReply = bridge.rows().at(0).toMap()
        .value(QStringLiteral("parentPreview")).toMap();
    assert(hydratedReply.value(QStringLiteral("text")).toString()
        == QStringLiteral("Original cast body"));
    assert(hydratedReply.value(QStringLiteral("resolved")).toBool());
    assert(hydratedReply.value(QStringLiteral("likeCount")).toInt() == 19);

    const QVariantMap hydratedQuote = bridge.rows().at(1).toMap()
        .value(QStringLiteral("embeds")).toList().front().toMap();
    assert(hydratedQuote.value(QStringLiteral("text")).toString()
        == QStringLiteral("Original cast body"));
    assert(hydratedQuote.value(QStringLiteral("resolved")).toBool());
    assert(hydratedQuote.value(QStringLiteral("replyCount")).toInt() == 7);
    assert(hydratedQuote.value(QStringLiteral("recastCount")).toInt() == 3);

    const QVariantList hydrated = bridge.rows();
    // The same network preview and unrelated offscreen completions must not
    // detach all visible QVariant maps or invalidate QML bindings again.
    bridge.applyReferencedCastPreview(QStringLiteral("0xparent"), preview);
    bridge.applyReferencedCastPreview(QStringLiteral("0xunrelated"), QVariantMap{
        {QStringLiteral("castId"), QStringLiteral("0xunrelated")},
        {QStringLiteral("text"), QStringLiteral("Offscreen content")},
    });
    assert(changes == 1);
    assert(bridge.rows() == hydrated);

    auto revised = preview;
    revised.insert(QStringLiteral("likeCount"), 23);
    bridge.applyReferencedCastPreview(QStringLiteral("0xparent"), revised);
    assert(changes == 2);
    assert(bridge.rows().at(1).toMap().value(QStringLiteral("embeds"))
        .toList().front().toMap().value(QStringLiteral("likeCount")).toInt() == 23);
}

void testNestedQuoteHydrationNotifiesDetailExactlyOnce()
{
    PresentationBridge bridge{};
    const QVariantMap nestedReply{
        {QStringLiteral("castId"), QStringLiteral("0xchild")},
        {QStringLiteral("parentCastId"), QStringLiteral("0xroot")},
        {QStringLiteral("embeds"), QVariantList{QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("cast-reference")},
            {QStringLiteral("castId"), QStringLiteral("0xquote")},
        }}},
    };
    bridge.applyDetail(
        QStringLiteral("cast"), QVariantMap{
            {QStringLiteral("castId"), QStringLiteral("0xroot")},
            {QStringLiteral("threadReplies"), QVariantList{nestedReply}},
        }, {}, {}, {}, false, false, false);

    int notifications = 0;
    QObject::connect(&bridge, &PresentationBridge::stateChanged, &bridge,
        [&notifications] { ++notifications; });
    const QVariantMap quoted{
        {QStringLiteral("castId"), QStringLiteral("0xquote")},
        {QStringLiteral("text"), QStringLiteral("Hydrated quote in nested reply")},
        {QStringLiteral("likeCount"), 12},
    };
    bridge.applyReferencedCastPreview(QStringLiteral("0xquote"), quoted);
    assert(notifications == 1);
    const auto replies = bridge.detail().value(QStringLiteral("threadReplies")).toList();
    assert(replies.size() == 1);
    const auto embeds = replies.front().toMap().value(QStringLiteral("embeds")).toList();
    assert(embeds.size() == 1);
    assert(embeds.front().toMap().value(QStringLiteral("text")).toString()
        == QStringLiteral("Hydrated quote in nested reply"));
    assert(embeds.front().toMap().value(QStringLiteral("likeCount")).toInt() == 12);

    const QVariantMap hydratedRoot = bridge.detail();
    bridge.applyReferencedCastPreview(QStringLiteral("0xquote"), quoted);
    assert(notifications == 1);
    assert(bridge.detail() == hydratedRoot);

    auto revised = quoted;
    revised.insert(QStringLiteral("likeCount"), 15);
    bridge.applyReferencedCastPreview(QStringLiteral("0xquote"), revised);
    assert(notifications == 2);
    const auto updatedReplies = bridge.detail().value(QStringLiteral("threadReplies")).toList();
    const auto updatedEmbeds = updatedReplies.front().toMap()
        .value(QStringLiteral("embeds")).toList();
    assert(updatedEmbeds.front().toMap().value(QStringLiteral("likeCount")).toInt() == 15);
}

void testProfileNavigationKeepsIndependentMetadataAndCachedCounts()
{
    PresentationBridge bridge{};
    QStringList emissions{};
    QObject::connect(&bridge, &PresentationBridge::profileRequested, &bridge,
        [&emissions](qulonglong fid, bool) {
            emissions.push_back(QStringLiteral("profile:%1").arg(fid));
        });
    QObject::connect(&bridge, &PresentationBridge::feedRequested, &bridge,
        [&emissions](const QString& kind, qulonglong fid,
                     const QString&, const QString&, bool) {
            emissions.push_back(QStringLiteral("feed:%1:%2").arg(kind).arg(fid));
        });

    bridge.openProfileFid(101);
    assert((emissions == QStringList{
        QStringLiteral("profile:101"), QStringLiteral("feed:user-posts:101")}));
    bridge.applyDetail(QStringLiteral("profile"), QVariantMap{
        {QStringLiteral("fid"), 101U},
        {QStringLiteral("username"), QStringLiteral("alice")},
        {QStringLiteral("displayName"), QStringLiteral("Alice")},
        {QStringLiteral("avatarUrl"), QStringLiteral("https://example.test/alice.png")},
        {QStringLiteral("followerCount"), 9384U},
        {QStringLiteral("followingCount"), 238U},
    }, {}, {}, {}, false, false, false);

    emissions.clear();
    bridge.selectProfileFeed(QStringLiteral("user-likes"));
    bridge.selectProfileFeed(QStringLiteral("user-quotes"));
    bridge.selectProfileFeed(QStringLiteral("user-replies"));
    assert((emissions == QStringList{
        QStringLiteral("feed:user-likes:101"),
        QStringLiteral("feed:user-quotes:101"),
        QStringLiteral("feed:user-replies:101")}));
    assert(bridge.detail().value(QStringLiteral("displayName")).toString()
        == QStringLiteral("Alice"));
    assert(bridge.detail().value(QStringLiteral("followerCount")).toUInt() == 9384U);

    // A pending or failed metadata refresh may report no profile, but an
    // existing matching identity must not be replaced by synthetic zeros.
    bridge.applyDetail(QStringLiteral("profile"), {}, {}, {}, {}, true, true, true);
    assert(bridge.detail().value(QStringLiteral("followerCount")).toUInt() == 9384U);
    bridge.applyDetail(QStringLiteral("profile"), {}, {},
        QStringLiteral("provider unavailable"), {}, false, true, true);
    assert(bridge.detail().value(QStringLiteral("followingCount")).toUInt() == 238U);
    assert(bridge.detail().value(QStringLiteral("avatarUrl")).toString()
        == QStringLiteral("https://example.test/alice.png"));

    // A different profile must never inherit the old identity or counters.
    emissions.clear();
    bridge.openProfileFid(202);
    assert((emissions == QStringList{
        QStringLiteral("profile:202"), QStringLiteral("feed:user-replies:202")}));
    assert(bridge.detail().isEmpty());
    bridge.applyDetail(QStringLiteral("profile"), {}, {}, {}, {}, true, false, false);
    assert(bridge.detail().isEmpty());
    // A legitimate verified zero for another FID remains a known zero.
    bridge.applyDetail(QStringLiteral("profile"), QVariantMap{
        {QStringLiteral("fid"), 202U}, {QStringLiteral("displayName"), QStringLiteral("Bob")},
        {QStringLiteral("followerCount"), 0U}, {QStringLiteral("followingCount"), 0U},
    }, {}, {}, {}, false, false, false);
    assert(bridge.detail().value(QStringLiteral("fid")).toUInt() == 202U);
    assert(bridge.detail().value(QStringLiteral("followerCount")).toUInt() == 0U);

    // Username resolution is not a second history entry.
    bridge.openCast(QStringLiteral("0xusername-mention"));
    bridge.openProfileUsername(QStringLiteral("alice"));
    assert(bridge.activeProfileFidText() == QStringLiteral("0"));
    bridge.resolveProfileUsername(QStringLiteral("alice"), 101);
    assert(bridge.activeProfileFidText() == QStringLiteral("101"));
    bridge.returnFromDetail();
    assert(bridge.activeView() == QStringLiteral("cast"));
    bridge.resolveProfileUsername(QStringLiteral("alice"), 101);
    assert(bridge.activeView() == QStringLiteral("cast"));
    bridge.openProfileUsername(QStringLiteral("bob"));
    bridge.resolveProfileUsername(QStringLiteral("alice"), 101);
    assert(bridge.activeProfileFidText() == QStringLiteral("0"));
    bridge.resolveProfileUsername(QStringLiteral("bob"), 202);
    bridge.returnFromDetail();
    assert(bridge.activeView() == QStringLiteral("cast"));

    // Notifications -> Cast -> mentioned profile -> Back must remain on
    // the Cast even if a previously requested profile arrives afterward.
    bridge.openCast(QStringLiteral("0xmentioned-cast"));
    bridge.openProfileFid(303);
    assert(bridge.activeView() == QStringLiteral("profile"));
    bridge.returnFromDetail();
    assert(bridge.activeView() == QStringLiteral("cast"));
    bridge.applyDetail(QStringLiteral("profile"), QVariantMap{
        {QStringLiteral("fid"), 303U},
        {QStringLiteral("displayName"), QStringLiteral("Delayed profile")}
    }, {}, {}, {}, false, false, false);
    assert(bridge.activeView() == QStringLiteral("cast"));
    assert(bridge.detail().value(QStringLiteral("fid")).toUInt() != 303U);
    bridge.applyList(QStringLiteral("profile"), QStringLiteral("user-replies"),
        QVariantList{}, {}, {}, {}, {}, false, false, false, false);
    assert(bridge.activeView() == QStringLiteral("cast"));

    bridge.openProfileFid(404);
    assert(bridge.activeView() == QStringLiteral("profile"));
    bridge.applyDetail(QStringLiteral("cast"), QVariantMap{
        {QStringLiteral("castId"), QStringLiteral("0xmentioned-cast")}
    }, {}, {}, {}, false, false, false);
    assert(bridge.activeView() == QStringLiteral("profile"));
    assert(bridge.activeProfileFidText() == QStringLiteral("404"));
}

void testAuthenticatedProfilePresentationIsCached()
{
    PresentationBridge bridge{};
    bridge.applyAuthenticationState(
        true,
        QStringLiteral("authenticated"),
        42,
        QStringLiteral("geny"),
        QStringLiteral("Geny"),
        QStringLiteral("https://example.test/avatar.png"),
        QStringLiteral("https://openproof.genyleap.com"),
        {},
        {});

    QVariantMap profile{
        {QStringLiteral("fid"), 42},
        {QStringLiteral("displayName"), QStringLiteral("Geny")},
        {QStringLiteral("trustScore"), 0.91},
        {QStringLiteral("trustScoreSource"), QStringLiteral("external-source")},
    };
    bridge.applyIdentity(42, QStringLiteral("Geny"), QStringLiteral("geny"), profile,
        {}, {}, {}, false, false, false);

    assert(bridge.accountProfile().value(QStringLiteral("fid")).toULongLong() == 42);
    assert(bridge.accountProfile().value(QStringLiteral("trustScore")).toDouble() == 0.91);
    assert(bridge.accountProfile().value(QStringLiteral("trustScoreSource")).toString()
        == QStringLiteral("external-source"));
}

void testInitialSetupRequiresAuthenticationAndFirstRunCompletion()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("setup-auth.ini"));
    QSettings settings{settingsPath, QSettings::IniFormat};
    // Legacy setup completion could be written by the old guest-dismissal flow.
    // It must not bypass the new authenticated onboarding wizard.
    settings.setValue(QStringLiteral("setup/initialSetupCompleted"), true);
    settings.setValue(QStringLiteral("setup/onboardingVersion"), 2);
    settings.sync();

    PresentationBridge bridge{nullptr, &settings,
        QStringLiteral("fixture-independent"), QStringLiteral("fixture-hosted")};
    assert(bridge.initialSetupRequired());
    assert(!bridge.initialSetupCompleted());
    assert(!bridge.legalConsentAccepted());
    // The preferred read/sign-in route is a validated preference, never a
    // substitute for a linked FID or signer authority.
    assert(bridge.preferredNetwork() == QStringLiteral("fixture-independent"));
    assert(!bridge.farcasterReadConfigured());
    bridge.setFarcasterReadConfigured(true);
    assert(bridge.farcasterReadConfigured());
    bridge.setFarcasterReadEndpoint(QStringLiteral("https://read.example.invalid"));
    assert(bridge.farcasterReadEndpoint() == QStringLiteral("https://read.example.invalid"));
    bridge.setFarcasterReadConfigured(false);
    assert(!bridge.farcasterReadConfigured());
    assert(!bridge.setPreferredNetwork(QStringLiteral("arbitrary-provider")));
    assert(bridge.preferredNetwork() == QStringLiteral("fixture-independent"));
    assert(bridge.setPreferredNetwork(QStringLiteral("fixture-hosted")));
    assert(bridge.preferredNetwork() == QStringLiteral("fixture-hosted"));
    assert(settings.value(QStringLiteral("connection/preferredNetwork")).toString()
           == QStringLiteral("fixture-hosted"));

    bridge.completeInitialSetup();
    assert(bridge.initialSetupRequired());
    assert(!bridge.initialSetupCompleted());

    bridge.applyAuthenticationState(
        true,
        QStringLiteral("authenticated"),
        77,
        {},
        {},
        {},
        QStringLiteral("https://openproof.genyleap.com"),
        {},
        {});
    assert(bridge.initialSetupRequired());

    bridge.completeInitialSetup();
    assert(!bridge.initialSetupCompleted());

    assert(!bridge.acceptLegalConsent(true, true, false));
    assert(!bridge.legalConsentAccepted());
    assert(!bridge.acceptLegalConsent(true, false, true));
    assert(!bridge.legalConsentAccepted());
    assert(bridge.acceptLegalConsent(true, true, true));
    assert(bridge.legalConsentAccepted());
    assert(settings.value(QStringLiteral("setup/legalConsentVersion")).toInt() == 2);
    assert(!settings.value(QStringLiteral("setup/legalConsentAcceptedAtUtc")).toString().isEmpty());

    bridge.completeInitialSetup();
    assert(bridge.initialSetupCompleted());
    assert(!bridge.initialSetupRequired());

    settings.sync();
    assert(settings.value(QStringLiteral("setup/onboardingVersion")).toInt() == 4);
    assert(!settings.contains(QStringLiteral("setup/initialSetupCompleted")));

    // Completed onboarding persists, but access is still gated on a verified
    // session. Restarts and sign-out return to Setup without anonymous feed.
    PresentationBridge restored{nullptr, &settings,
        QStringLiteral("fixture-independent"), QStringLiteral("fixture-hosted")};
    assert(restored.initialSetupCompleted());
    assert(restored.legalConsentAccepted());
    assert(restored.preferredNetwork() == QStringLiteral("fixture-hosted"));
    assert(restored.setPreferredNetwork(QStringLiteral("fixture-independent")));
    assert(restored.preferredNetwork() == QStringLiteral("fixture-independent"));
    // On restart a saved onboarding record alone does not grant guest access.
    assert(restored.initialSetupRequired());
    restored.applyAuthenticationState(
        true,
        QStringLiteral("authenticated"),
        77,
        {},
        {},
        {},
        QStringLiteral("https://openproof.genyleap.com"),
        {},
        {});
    assert(!restored.initialSetupRequired());

    restored.applySignerState(
        QStringLiteral("ready"), false, true, QStringLiteral("ready"), {});
    assert(restored.initialSetupCompleted());
    restored.applySignerState(
        QStringLiteral("revoked"), false, false, QStringLiteral("revoked"), {});
    assert(restored.initialSetupCompleted());
    assert(!restored.initialSetupRequired());
    assert(!restored.writeReady());
    assert(restored.legalConsentAccepted());
    settings.sync();
    assert(settings.value(QStringLiteral("setup/onboardingVersion")).toInt() == 4);

    // Finishing the wizard again is allowed without discarding the durable
    // legal consent record.
    restored.completeInitialSetup();
    assert(restored.initialSetupCompleted());
    assert(!restored.initialSetupRequired());
    assert(settings.value(QStringLiteral("setup/onboardingVersion")).toInt() == 4);

    // Signing out retains consent, but returns to the Setup overlay.
    restored.applyAuthenticationState(
        true,
        QStringLiteral("signed-out"),
        0,
        {},
        {},
        {},
        {},
        {},
        {});
    assert(restored.initialSetupCompleted());
    assert(restored.initialSetupRequired());
    assert(!restored.writeReady());
    assert(restored.legalConsentAccepted());
    settings.sync();
    assert(settings.value(QStringLiteral("setup/onboardingVersion")).toInt() == 4);

    PresentationBridge afterSignOut{nullptr, &settings};
    assert(afterSignOut.initialSetupCompleted());
    assert(afterSignOut.legalConsentAccepted());
    assert(afterSignOut.initialSetupRequired());

    // A returning verified account clears the temporary post-logout Setup
    // gate; it must not be forced through onboarding after recovery sign-in.
    afterSignOut.applyAuthenticationState(
        true,
        QStringLiteral("authenticated"),
        77,
        {},
        {},
        {},
        QStringLiteral("https://openproof.genyleap.com"),
        {},
        {});
    assert(!afterSignOut.initialSetupRequired());
    assert(afterSignOut.initialSetupCompleted());
    assert(!settings.contains(QStringLiteral("setup/showAfterSignOut")));
}


void testReplyCanonicalizationForPublicFeedHashes()
{
    const QString bare = QString(40, QLatin1Char('a'));
    const QString canonical = QStringLiteral("0x") + bare;
    PresentationBridge bridge{};
    bridge.applyRuntimeState(true);
    bridge.applyAuthenticationState(
        true, QStringLiteral("authenticated"), 77, {}, {}, {}, {}, {}, {});
    bridge.applySignerState(QStringLiteral("ready"), false, true, {}, {});

    QString publishedParent{};
    qulonglong publishedFid{};
    QObject::connect(&bridge, &PresentationBridge::publishReplyRequested, &bridge,
        [&publishedParent, &publishedFid](QString, QVariantList, QVariantList, QVariantList,
                                           qulonglong authorFid, QString castId) {
            publishedParent = castId;
            publishedFid = authorFid;
        });
    assert(bridge.prepareReplyToCast(bare.toUpper(), 539854));
    bridge.publishPreparedReply(QStringLiteral("Canonical reply"), {}, {}, {});
    assert(publishedParent == canonical);
    assert(publishedFid == 539854);

    PresentationBridge cached{};
    cached.applyList(
        QStringLiteral("feed"), QStringLiteral("trending"),
        {QVariantMap{{QStringLiteral("castId"), bare},
                     {QStringLiteral("authorFid"), 539854}}},
        {}, {}, {}, {}, false, false, false, false);
    assert(cached.prepareReply(canonical.toUpper()));
    assert(cached.prepareReplyToCast(canonical.toUpper(), 539854));
    assert(!cached.prepareReplyToCast(QString(39, QLatin1Char('a')), 539854));
    assert(!cached.prepareReplyToCast(QString(40, QLatin1Char('z')), 539854));
    assert(!cached.prepareReplyToCast(bare, 0));
    assert(!cached.prepareReplyToCast(bare, 539854.5));
}

void testExplicitExplorerRefreshRevealsLatestPage()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("refresh.ini")), QSettings::IniFormat};
    settings.setValue(QStringLiteral("read/bufferNewPosts"), true);
    PresentationBridge bridge{nullptr, &settings};
    bridge.applyRuntimeState(true);
    const QVariantMap oldRow{{QStringLiteral("castId"), QStringLiteral("0xold")}};
    const QVariantMap newRow{{QStringLiteral("castId"), QStringLiteral("0xnew")}};
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"), {oldRow},
                     {}, {}, {}, {}, false, false, false, false);
    bridge.retry();
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"), {newRow, oldRow},
                     {}, {}, {}, {}, false, false, false, false);
    assert(bridge.pendingNewPostCount() == 1);

    QString requestedFeed{};
    QObject::connect(&bridge, &PresentationBridge::feedRequested, &bridge,
        [&requestedFeed](const QString& feed, qulonglong, const QString&, const QString&, bool) {
            requestedFeed = feed;
        });
    bridge.refreshLatestFeed();
    assert(requestedFeed == QStringLiteral("trending"));
    assert(bridge.pendingNewPostCount() == 0);
    assert(bridge.rows().front().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xnew"));
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"), {newRow, oldRow},
                     {}, {}, {}, {}, false, false, false, false);
    assert(bridge.pendingNewPostCount() == 0);
    assert(bridge.rows().size() == 2);
    assert(bridge.rows().front().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xnew"));
}


void testFirstPageCacheDoesNotRemountLiveTimeline()
{
    PresentationBridge bridge{};
    const QVariantMap visible{
        {QStringLiteral("castId"), QStringLiteral("0xvisible")},
        {QStringLiteral("_referencedRevision"), QStringLiteral("enriched")},
        {QStringLiteral("text"), QStringLiteral("Live hydrated cast")},
    };
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"), {visible},
                     {}, {}, {}, {}, false, false, false, false);
    // Starting a first-page refresh republishes a cached snapshot; it must
    // not tear down already-hydrated delegates before the provider responds.
    const QVariantMap stale{
        {QStringLiteral("castId"), QStringLiteral("0xvisible")},
        {QStringLiteral("text"), QStringLiteral("Old cached cast")},
    };
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"), {stale},
                     {}, {}, {}, {}, true, true, false, false);
    const auto retained = bridge.rows().front().toMap();
    assert(retained.value(QStringLiteral("text")).toString()
        == QStringLiteral("Live hydrated cast"));
    assert(retained.value(QStringLiteral("_referencedRevision")).toString()
        == QStringLiteral("enriched"));
    const QVariantMap fresh{
        {QStringLiteral("castId"), QStringLiteral("0xlatest")},
        {QStringLiteral("text"), QStringLiteral("Newest live cast")},
    };
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"), {fresh},
                     {}, {}, {}, {}, false, false, false, false);
    assert(bridge.rows().front().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xlatest"));
}


void testViewerReactionFirstNegativeAndDuplicateEvidence()
{
    PresentationBridge bridge{};
    const QString id{QStringLiteral("0x") + QString(40, QLatin1Char('a'))};
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
        {QVariantMap{
            {QStringLiteral("castId"), id},
            {QStringLiteral("likedByMe"), true},
            {QStringLiteral("recastedByMe"), true},
            {QStringLiteral("likeCount"), 7U},
            {QStringLiteral("recastCount"), 2U},
        }},
        {}, {}, {}, {}, false, false, false, false);
    int invalidations{0};
    QObject::connect(&bridge, &PresentationBridge::stateChanged,
                     &bridge, [&invalidations] { ++invalidations; });
    const QVariantMap negative{
        {QStringLiteral("castId"), id},
        {QStringLiteral("liked"), false},
        {QStringLiteral("recasted"), false},
    };
    bridge.applyViewerReactionDelta({negative});
    assert(invalidations == 1);
    assert(!bridge.rows().front().toMap().value(QStringLiteral("likedByMe")).toBool());
    assert(!bridge.rows().front().toMap().value(QStringLiteral("recastedByMe")).toBool());
    // No repeated whole-feed update when an identical complete result arrives.
    bridge.applyViewerReactionDelta({negative});
    assert(invalidations == 1);
    bridge.applyViewerReactionDelta({
        QVariantMap{
            {QStringLiteral("castId"), QStringLiteral("0x") + QString(40, QLatin1Char('f'))},
            {QStringLiteral("liked"), true},
            {QStringLiteral("recasted"), false},
        }});
    assert(invalidations == 1); // An offscreen target is not a UI update.
    const QVariantMap positive{
        {QStringLiteral("castId"), id},
        {QStringLiteral("liked"), true},
        {QStringLiteral("recasted"), false},
    };
    bridge.applyViewerReactionDelta({positive});
    assert(invalidations == 2);
    assert(bridge.rows().front().toMap().value(QStringLiteral("likedByMe")).toBool());

    bridge.applyReactionProjections({
        QVariantMap{
            {QStringLiteral("castId"), id},
            {QStringLiteral("kind"), QStringLiteral("like")},
            {QStringLiteral("active"), true},
        }});
    bridge.applyViewerReactionDelta({negative});
    assert(bridge.rows().front().toMap().value(QStringLiteral("likedByMe")).toBool());
}

void testViewerReactionPartialTypeCoverage()
{
    PresentationBridge bridge{};
    const QString id{QStringLiteral("0x") + QString(40, QLatin1Char('b'))};
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("recent"),
        {QVariantMap{{QStringLiteral("castId"), id},
                     {QStringLiteral("likedByMe"), true},
                     {QStringLiteral("recastedByMe"), true}}},
        {}, {}, {}, {}, false, false, false, false);
    int notifications{};
    QObject::connect(&bridge, &PresentationBridge::stateChanged,
        &bridge, [&notifications] { ++notifications; });

    // A complete negative Like read must never pretend to know Recast.
    bridge.applyViewerReactionDelta({
        QVariantMap{{QStringLiteral("castId"), id},
                    {QStringLiteral("liked"), false}},
    });
    assert(notifications == 1);
    assert(!bridge.rows().front().toMap().value(QStringLiteral("likedByMe")).toBool());
    assert(bridge.rows().front().toMap().value(QStringLiteral("recastedByMe")).toBool());

    // Incomplete Like coverage has no value key and cannot undo the latest
    // verified Like evidence; Recast may still be known independently.
    bridge.applyViewerReactionDelta({
        QVariantMap{{QStringLiteral("castId"), id},
                    {QStringLiteral("recasted"), false}},
    });
    assert(notifications == 2);
    assert(!bridge.rows().front().toMap().value(QStringLiteral("likedByMe")).toBool());
    assert(!bridge.rows().front().toMap().value(QStringLiteral("recastedByMe")).toBool());
    bridge.applyViewerReactionDelta({
        QVariantMap{{QStringLiteral("castId"), id}},
    });
    assert(notifications == 2);
    bridge.applyViewerReactionDelta({
        QVariantMap{{QStringLiteral("castId"), id},
                    {QStringLiteral("liked"), true}},
    });
    assert(notifications == 3);
    assert(bridge.rows().front().toMap().value(QStringLiteral("likedByMe")).toBool());
    assert(!bridge.rows().front().toMap().value(QStringLiteral("recastedByMe")).toBool());
}

void testBoundedWarmRefreshRetainsAnchorAndOlderCachedRows()
{
    PresentationBridge bridge{};
    const auto id = [](const int value) {
        return QStringLiteral("0x") + QString::number(value, 16)
            .rightJustified(40, QLatin1Char('0'));
    };
    QVariantList history{};
    for (int i = 0; i < 130; ++i) {
        history.push_back(QVariantMap{
            {QStringLiteral("castId"), id(i)},
            {QStringLiteral("text"), QStringLiteral("old-content")},
        });
    }
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("recent"),
        history, {}, {}, {}, {}, false, false, false, false);
    QVariantList fresh{};
    for (int i = 0; i < 2; ++i) {
        fresh.push_back(QVariantMap{
            {QStringLiteral("castId"), id(5000 + i)},
            {QStringLiteral("text"), QStringLiteral("new-content")},
        });
    }
    for (int i = 0; i < 62; ++i) {
        fresh.push_back(QVariantMap{
            {QStringLiteral("castId"), id(i)},
            {QStringLiteral("text"), QStringLiteral("fresh-content")},
        });
    }
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("recent"), fresh,
        {}, {}, {}, {}, false, false, false, false, true);
    const auto merged = bridge.rows();
    assert(merged.size() == 132);
    assert(merged[0].toMap().value(QStringLiteral("castId")).toString()
        == id(5000));
    assert(merged[1].toMap().value(QStringLiteral("castId")).toString()
        == id(5001));
    int anchors{};
    int changed{};
    for (const auto& value : merged) {
        const auto row = value.toMap();
        if (row.value(QStringLiteral("castId")).toString() == id(129)) {
            ++anchors;
        }
        if (row.value(QStringLiteral("castId")).toString() == id(5)
            && row.value(QStringLiteral("text")).toString()
                == QStringLiteral("fresh-content")) {
            ++changed;
        }
    }
    assert(anchors == 1 && changed == 1);
}

void testSourceSwitchAdmitsCachedTimeline()
{
    PresentationBridge bridge{};
    bridge.applyRuntimeState(true);
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("following"),
                     {QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xfollowing")}}},
                     {}, {}, {}, {}, false, false, false, false);
    bridge.selectFeed(QStringLiteral("trending"));
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
                     {QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xcached-trending")}}},
                     {}, {}, {}, {}, true, true, false, false);
    assert(bridge.rows().front().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xcached-trending"));
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
                     {QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xlive-trending")}}},
                     {}, {}, {}, {}, false, false, false, false);
    assert(bridge.rows().front().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xlive-trending"));
}


void testPortableCastProfileLinksWithoutLocalModeration()
{
    QTemporaryDir directory{};
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("share.ini")), QSettings::IniFormat};
    settings.setValue(QStringLiteral("sharing/publicBaseUrl"),
                      QStringLiteral("https://social.example.test"));
    PresentationBridge bridge{nullptr, &settings};
    const QString castHash = QStringLiteral("0x") + QString(40, QLatin1Char('a'));
    const QString castUrl = QStringLiteral("https://social.example.test/?cast=") + castHash;
    const QString profileUrl = QStringLiteral("https://social.example.test/?profile=77");
    assert(bridge.publicShareBaseUrl() == QStringLiteral("https://social.example.test/"));
    assert(bridge.castShareUrl(castHash) == castUrl);
    // Providers may return 40 hex characters without the optional 0x prefix.
    assert(bridge.castShareUrl(castHash.mid(2)) == castUrl);
    assert(bridge.quorumCastUri(castHash.mid(2))
           == QStringLiteral("quorummobile://feed?castHashPrefix=") + castHash);
    assert(bridge.castShareUrl(QStringLiteral("0x1234")).isEmpty());
    assert(bridge.profileShareUrl(0).isEmpty());
    assert(bridge.profileShareUrl(77) == profileUrl);
    assert(bridge.farcasterCastUri(castHash, 77)
           == QStringLiteral("farcaster://77/") + castHash);
    assert(bridge.farcasterProfileUri(77) == QStringLiteral("farcaster://77"));
    assert(bridge.farcasterWebCastUrl(castHash, QStringLiteral("alice"))
           == QStringLiteral("https://farcaster.xyz/alice/") + castHash);
    assert(bridge.farcasterWebCastUrl(castHash, QStringLiteral("../evil"))
           == QStringLiteral("https://farcaster.xyz/~/conversations/") + castHash);
    assert(bridge.farcasterWebProfileUrl(QStringLiteral("alice"))
           == QStringLiteral("https://farcaster.xyz/alice"));
    assert(bridge.quorumCastUri(castHash)
           == QStringLiteral("quorummobile://feed?castHashPrefix=") + castHash);
    assert(bridge.quorumCastUri(QStringLiteral("not-a-hash")).isEmpty());
    QString receivedCast;
    qulonglong receivedProfile = 0;
    QObject::connect(&bridge, &PresentationBridge::castRequested, &bridge,
                     [&](const QString& hash, bool) { receivedCast = hash; });
    QObject::connect(&bridge, &PresentationBridge::profileRequested, &bridge,
                     [&](qulonglong fid, bool) { receivedProfile = fid; });
    assert(bridge.openSharedLink(castUrl));
    assert(receivedCast == castHash);
    assert(bridge.openSharedLink(profileUrl));
    assert(receivedProfile == 77);
    assert(bridge.openSharedLink(bridge.farcasterCastUri(castHash, 77)));
    assert(receivedCast == castHash);
    assert(bridge.openSharedLink(bridge.farcasterProfileUri(77)));
    assert(receivedProfile == 77);
    assert(!bridge.openSharedLink(QStringLiteral("https://social.example.test.attacker.test/?profile=77")));
    assert(!bridge.openSharedLink(QStringLiteral("https://social.example.test/?cast=javascript:alert(1)")));
    assert(!bridge.openSharedLink(QStringLiteral("https://social.example.test/?profile=0")));
    assert(!bridge.openSharedLink(QStringLiteral("farcaster://77/not-a-hash")));

    // A migration changes the share and incoming link authority immediately.
    settings.setValue(QStringLiteral("sharing/publicBaseUrl"),
                      QStringLiteral("https://caster.future.test/app"));
    assert(bridge.publicShareBaseUrl() == QStringLiteral("https://caster.future.test/app/"));
    const QString migratedCastUrl = QStringLiteral("https://caster.future.test/app/?cast=") + castHash;
    assert(bridge.castShareUrl(castHash) == migratedCastUrl);
    assert(bridge.profileShareUrl(77)
           == QStringLiteral("https://caster.future.test/app/?profile=77"));
    assert(bridge.openSharedLink(migratedCastUrl));
    assert(receivedCast == castHash);
    assert(!bridge.openSharedLink(castUrl));
    assert(!bridge.openSharedLink(QStringLiteral("https://caster.future.test.attacker.test/app/?profile=77")));
    assert(!bridge.openSharedLink(QStringLiteral("https://caster.future.test/other/?profile=77")));
    assert(!bridge.openSharedLink(QStringLiteral("https://caster.future.test/app/?profile=0")));

    // Untrusted/invalid native configuration must not emit a malformed link.
    for (const QString& unsafeBase : {
             QStringLiteral("http://caster.future.test"),
             QStringLiteral("https://user:password@caster.future.test"),
             QStringLiteral("https://caster.future.test/?token=secret"),
             QStringLiteral("https://caster.future.test/#fragment"),
         }) {
        settings.setValue(QStringLiteral("sharing/publicBaseUrl"), unsafeBase);
        assert(bridge.publicShareBaseUrl().isEmpty());
        assert(bridge.castShareUrl(castHash).isEmpty());
        assert(!bridge.openSharedLink(migratedCastUrl));
    }
    settings.setValue(QStringLiteral("sharing/publicBaseUrl"),
                      QStringLiteral("https://caster.future.test/app"));

    bridge.applyAuthenticationState(true, QStringLiteral("authenticated"), 42,
        QStringLiteral("viewer"), {}, {}, {}, {}, {});
    // Historic local-only moderation settings must no longer hide feed authors.
    settings.setValue(QStringLiteral("moderation/42/mute"), QStringList{QStringLiteral("77")});
    settings.setValue(QStringLiteral("moderation/42/block"), QStringList{QStringLiteral("88")});
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
        {QVariantMap{{QStringLiteral("castId"), castHash},
                     {QStringLiteral("authorFid"), 77ULL}},
         QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xother")},
                     {QStringLiteral("authorFid"), 88ULL}}},
        {}, {}, {}, {}, false, false, false, false);
    assert(bridge.rows().size() == 2);
}

void testRecoveredProfileKeepsOwnAccountAffiliation()
{
    PresentationBridge bridge{};
    bridge.applyRuntimeState(true);
    assert(bridge.acceptLegalConsent(true, true, true));
    bridge.openProfileFid(42);
    assert(!bridge.ownProfileNavigation());

    // The 24-word custody flow is explicitly selected before the session
    // observer commits the registry-verified FID. No secret is retained.
    const QString phrase = QStringLiteral("abandon ").repeated(23)
        + QStringLiteral("art");
    bool requested = false;
    QObject::connect(&bridge, &PresentationBridge::nativeFarcasterRecoveryRequested,
        &bridge, [&requested](QString, bool allowed) {
            requested = allowed;
        });
    bridge.beginNativeFarcasterRecoverySetup(phrase, true);
    assert(requested);
    bridge.applyAuthenticationState(
        true, QStringLiteral("authenticated"), 42,
        QStringLiteral("owner"), QStringLiteral("Owner"), {},
        QStringLiteral("farcaster:optimism-id-registry"), {}, {});

    assert(bridge.ownProfileNavigation());
    assert(bridge.accountFid() == 42U);
    // Receiving the same profile ID again (from a delayed public read or an
    // activity card) must not turn the signed-in owner into a Follow target.
    bridge.openProfileFid(42);
    assert(bridge.ownProfileNavigation());
    bridge.openProfileFid(77);
    assert(!bridge.ownProfileNavigation());
    bridge.applyAuthenticationState(
        true, QStringLiteral("signed-out"), 0, {}, {}, {}, {}, {}, {});
    assert(!bridge.ownProfileNavigation());
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    testPortableCastProfileLinksWithoutLocalModeration();
    testFollowingStoriesKeepOwnFirstAndOlderFollowees();
    testContextualReactionPriorityAndBoundaries();
    testReplyCanonicalizationForPublicFeedHashes();
    testExplicitExplorerRefreshRevealsLatestPage();
    testFirstPageCacheDoesNotRemountLiveTimeline();
    testBoundedWarmRefreshRetainsAnchorAndOlderCachedRows();
    testSourceSwitchAdmitsCachedTimeline();
    testViewerReactionFirstNegativeAndDuplicateEvidence();
    testViewerReactionPartialTypeCoverage();
    testPaginationPreservesHydratedReferencedContext();
    testPaginationPreservesResolvedMediaOnlyReferencedContext();
    testIncrementalPaginationDecoratesOnlyNewOrRevisedRows();
    testReadPreferencesAreBoundedAndPersisted();
    testBufferedRefreshPreservesTimelineUntilReveal();
    testReferencedPreviewHydratesReplyAndQuoteMetrics();
    testNestedQuoteHydrationNotifiesDetailExactlyOnce();
    testAuthenticatedProfilePresentationIsCached();
    testRecoveredProfileKeepsOwnAccountAffiliation();
    testProfileNavigationKeepsIndependentMetadataAndCachedCounts();
    testInitialSetupRequiresAuthenticationAndFirstRunCompletion();
    return 0;
}
