#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QVariantMap>

#include <cassert>

#include "presentation_bridge.hpp"

namespace {

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
    bridge.applyReferencedCastPreview(QStringLiteral("0xparent"), preview);

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
        {QStringLiteral("trustScoreSource"), QStringLiteral("neynar")},
    };
    bridge.applyIdentity(42, QStringLiteral("Geny"), QStringLiteral("geny"), profile,
        {}, {}, {}, false, false, false);

    assert(bridge.accountProfile().value(QStringLiteral("fid")).toULongLong() == 42);
    assert(bridge.accountProfile().value(QStringLiteral("trustScore")).toDouble() == 0.91);
    assert(bridge.accountProfile().value(QStringLiteral("trustScoreSource")).toString()
        == QStringLiteral("neynar"));
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

    PresentationBridge bridge{nullptr, &settings};
    assert(bridge.initialSetupRequired());
    assert(!bridge.initialSetupCompleted());
    assert(!bridge.legalConsentAccepted());
    // The preferred read/sign-in route is a validated preference, never a
    // substitute for a linked FID or signer authority.
    assert(bridge.preferredNetwork() == QStringLiteral("hypersnap"));
    assert(!bridge.farcasterReadConfigured());
    bridge.setFarcasterReadConfigured(true);
    assert(bridge.farcasterReadConfigured());
    bridge.setFarcasterReadEndpoint(QStringLiteral("https://api.neynar.com"));
    assert(bridge.farcasterReadEndpoint() == QStringLiteral("https://api.neynar.com"));
    bridge.setFarcasterReadConfigured(false);
    assert(!bridge.farcasterReadConfigured());
    assert(!bridge.setPreferredNetwork(QStringLiteral("arbitrary-provider")));
    assert(bridge.preferredNetwork() == QStringLiteral("hypersnap"));
    assert(bridge.setPreferredNetwork(QStringLiteral("farcaster")));
    assert(bridge.preferredNetwork() == QStringLiteral("farcaster"));
    assert(settings.value(QStringLiteral("connection/preferredNetwork")).toString()
           == QStringLiteral("farcaster"));

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

    // A completed setup survives an ordinary restart while the account can be
    // restored, but removing the protocol signer deliberately re-opens the
    // complete setup flow rather than a sign-in-only checkpoint.
    PresentationBridge restored{nullptr, &settings};
    assert(restored.initialSetupCompleted());
    assert(restored.legalConsentAccepted());
    assert(restored.preferredNetwork() == QStringLiteral("farcaster"));
    assert(restored.setPreferredNetwork(QStringLiteral("hypersnap")));
    assert(restored.preferredNetwork() == QStringLiteral("hypersnap"));
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
    assert(!restored.initialSetupCompleted());
    assert(restored.initialSetupRequired());
    assert(restored.legalConsentAccepted());
    settings.sync();
    assert(!settings.contains(QStringLiteral("setup/onboardingVersion")));

    // Finishing the wizard again is allowed without discarding the durable
    // legal consent record.
    restored.completeInitialSetup();
    assert(restored.initialSetupCompleted());
    assert(!restored.initialSetupRequired());
    assert(settings.value(QStringLiteral("setup/onboardingVersion")).toInt() == 4);

    // Signing out is also a setup reset boundary. The next run must start from
    // Welcome and expose all six setup steps again.
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
    assert(!restored.initialSetupCompleted());
    assert(restored.initialSetupRequired());
    assert(restored.legalConsentAccepted());
    settings.sync();
    assert(!settings.contains(QStringLiteral("setup/onboardingVersion")));

    PresentationBridge afterSignOut{nullptr, &settings};
    assert(!afterSignOut.initialSetupCompleted());
    assert(afterSignOut.legalConsentAccepted());
    assert(afterSignOut.initialSetupRequired());
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    testContextualReactionPriorityAndBoundaries();
    testPaginationPreservesHydratedReferencedContext();
    testPaginationPreservesResolvedMediaOnlyReferencedContext();
    testReadPreferencesAreBoundedAndPersisted();
    testBufferedRefreshPreservesTimelineUntilReveal();
    testReferencedPreviewHydratesReplyAndQuoteMetrics();
    testAuthenticatedProfilePresentationIsCached();
    testInitialSetupRequiresAuthenticationAndFirstRunCompletion();
    return 0;
}
