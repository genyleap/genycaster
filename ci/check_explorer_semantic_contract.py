#!/usr/bin/env python3
"""Guard Explorer source isolation, semantic partitioning, and request-pressure rules."""

from __future__ import annotations

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
errors: list[str] = []


def read(rel: str) -> str:
    path = root / rel
    if not path.exists():
        errors.append(f"missing required file: {rel}")
        return ""
    return path.read_text(encoding="utf-8")


shell = read("ui/components/shell/AppShell.qml")
feed = read("ui/components/feed/FeedSurface.qml")
runtime = (read("ui/app/main.cpp") + read("ui/app/runtime/composition.cppm") +
           read("ui/app/runtime/composition.cpp") +
           read("ui/app/runtime/bindings.cpp"))
link_preview = read("ui/components/media/LinkPreview.qml")
preview_resolver = read("ui/app/network/link_preview.cpp")
profile = read("ui/components/profile/ProfileSurface.qml")
detail = read("ui/components/feed/DetailSurface.qml")
superellipse_image = read("ui/components/visual/SuperellipseImage.qml")
bridge_header = read("src/presentation/qt/presentation_bridge.hpp")
bridge = read("src/presentation/qt/presentation_bridge.cpp")
model = read("src/application/read_model.cppm")
service = read("src/application/read_async_service.cpp")
routed = read("src/routing/read_async_gateway.cpp")
hypersnap_request = read("src/providers/hypersnap/async_request.cpp")

for token in (
    'const source = presentationBridge.isAuthenticated && root.identityAvailable',
    '? "following" : "trending"',
    'const modeChanged = root.explorerPresentationMode !== nextMode',
    'const emptySettledSource = presentationBridge.feedStarted',
    '&& presentationBridge.rows.length === 0',
    'const sameSource = presentationBridge.activeView === "feed"',
    'const sameHealthySource = sameSource && presentationBridge.feedStarted',
    '&& presentationBridge.activeFeed === source',
    'const refreshRequested = Boolean(forceRefresh)',
    'else if (!sameHealthySource && !(sameSource && presentationBridge.isBusy))',
    'presentationBridge.isStale',
    'presentationBridge.retry()',
    'root.selectExplorerPresentationMode(root.explorerPresentationMode, true)',
):
    if token not in shell:
        errors.append(f"Explorer source selection missing {token!r}")

if 'nextMode === "for-you" && presentationBridge.isAuthenticated' in shell:
    errors.append("Explorer tab selection must not switch live feed sources")

for token in (
    "property int forYouPrefetchPages: 0",
    "property int visualPrefetchPages: 0",
    "property int conversationPrefetchPages: 0",
    "property int developerPrefetchPages: 0",
    "property int automaticPrefetchPages: 0",
    'readonly property int filterPrefetchLimit: root.presentationMode === "visuals" && root.activeFeed !== "following" ? 12 : 8',
    "readonly property int automaticPrefetchLimit: 16",
    'readonly property int automaticNoProgressLimit: root.presentationMode === "visuals"',
    '? (root.activeFeed === "following" ? 8 : 5) : 2',
    "property int automaticLastRowCount: -1",
    "function resetAllFilterPrefetchBudgets()",
    "root.isRootPost(row)",
    "function semanticKind(data)",
    'if (root.looksDeveloper(data))',
    'if (root.hasCollectible(data))',
    'if (root.isConversation(data))',
    'return "other"',
    "readList.moving && stableRows.count > 0",
    'if (root.presentationMode === "visuals")',
):
    if token not in feed:
        errors.append(f"Explorer semantic presentation missing {token!r}")

semantic_match = re.search(
    r"function semanticKind\(data\)\s*\{(?P<body>.*?)\n\s*\}", feed, re.DOTALL
)
if semantic_match:
    body = semantic_match.group("body")
    expected_order = [
        "root.hasCollectible(data)",
        "root.looksDeveloper(data)",
        "root.isConversation(data)",
        'return "other"',
    ]
    positions = [body.find(token) for token in expected_order]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        errors.append("Explorer semantic precedence must remain NFTs > Devs > Conversations > Other")
else:
    errors.append("Explorer semanticKind() body is not structurally readable")

if 'presentationBridge.requestExplorerArt()' in shell:
    errors.append("Explorer Visuals must classify the live primary feed instead of injecting cached art-channel rows")
if 'supplementalRows: []' not in shell or 'supplementalBusy: false' not in shell:
    errors.append("Explorer must not bind the legacy art supplement into the semantic feed")
if 'github.com/' not in feed or 'raw.githubusercontent.com/' not in feed:
    errors.append("Explorer Devs must require explicit GitHub-link evidence")

for token in (
    'kind === "token" || kind === "token-address" || kind === "fungible-token"',
    'provider === "geckoterminal"',
    'eip155:\\d+\\/erc20:',
    'erc(?:721|1155)',
    'assetKind === "artwork" || assetKind === "collectible"',
    'root.isCollectibleMarketplaceUrl(value)',
    'function rowHasImage(data)',
    'function containsCollectibleLink(data)',
    'if (root.containsCollectibleLink(row))',
    'if (artContext && root.rowHasImage(row))',
):
    if token not in feed:
        errors.append(f"NFT-art filtering contract missing {token!r}")

if re.search(r'kind === "collectible" \|\| kind === "nft" \|\| kind === "token"', feed):
    errors.append("Fungible token embeds must not classify as NFT art")

presentation_mode_block = re.search(
    r"onPresentationModeChanged\s*:\s*\{(?P<body>.*?)\n\s*\}", feed, re.DOTALL
)
if not presentation_mode_block or "root.resetCurrentFilterPrefetchBudget()" not in presentation_mode_block.group("body"):
    errors.append("Each Explorer semantic-tab visit must receive a fresh bounded pagination budget")

if ('(root.presentationMode === "for-you" || root.presentationMode === "following")' not in feed
        or "if (root.isRootPost(row))" not in feed):
    errors.append("For You must preserve the full provider-ordered root-post stream")

for token in (
    "enum class ReadIntent",
    "UserVisible",
    "Prefetch",
):
    if token not in model:
        errors.append(f"Read scheduling model missing {token}")

for token in (
    "feedRequestGeneration",
    "beginFeedRequestGeneration()",
    "isCurrentFeedRequestGeneration(requestGeneration)",
    "maximumContextHydrationsPerFeedState{24U}",
    "keepReferencedPreviewRequest",
    ".intent = application::read::model::ReadIntent::Prefetch",
    "allowRecoveryProbe && !continuationRequest",
    "referencedPreviewInFlight.insert(castId.value)",
    "phase == application::read::model::ReadPhase::Ready",
    "phase == application::read::model::ReadPhase::Failed",
):
    if token not in runtime:
        errors.append(f"Explorer runtime isolation missing {token!r}")

if runtime.count("isCurrentFeedRequestGeneration(requestGeneration)") < 3:
    errors.append("Feed generation must gate worker completion and queued GUI/context completions")

missing_refs_match = re.search(
    r"missingReferencedCastIds\(.*?\n\}\n\n\[\[nodiscard\]\] QVariantMap castPreviewPresentation",
    runtime,
    re.DOTALL,
)
if not missing_refs_match:
    errors.append("Referenced-cast hydration selector is not structurally readable")
else:
    missing_refs_body = missing_refs_match.group(0)
    parent_position = missing_refs_body.find("if (cast.parent)")
    quote_position = missing_refs_body.find("if (embed.kind == domain::cast::EmbedKind::CastReference)")
    if parent_position < 0 or quote_position < 0 or parent_position > quote_position:
        errors.append("Parent casts must be queued before optional quote previews")

for token in (
    "root.openRequested(root.targetUrl)",
    'root.providerKind === "youtube" ? 58 : 42',
    "durationText",
):
    if token not in link_preview:
        errors.append(f"Direct YouTube preview contract missing {token!r}")

if "openEmbeddedMedia" in re.search(
    r"function openPreview\(\)\s*\{(?P<body>.*?)\n\s*\}", link_preview, re.DOTALL
).group("body"):
    errors.append("YouTube preview must not depend on embedded WebView playback")

for token in (
    'QStringLiteral("https://www.youtube.com/oembed")',
    'QStringLiteral("author_name")',
    'QStringLiteral("thumbnail_url")',
    'QStringLiteral("providerKind"), QStringLiteral("youtube")',
):
    if token not in preview_resolver:
        errors.append(f"YouTube metadata preview missing {token!r}")

for token in (
    "explorerArtRows",
    "explorerArtBusy",
    "requestExplorerArt",
    "explorerArtRequested",
):
    if token not in bridge_header or token not in bridge:
        errors.append(f"Explorer art-channel discovery bridge missing {token!r}")
art_request_match = re.search(
    r"void PresentationBridge::requestExplorerArt\(\)\s*\{(?P<body>.*?)\n\}",
    bridge, re.DOTALL
)
if not art_request_match or "!m_explorerArtRows.isEmpty()" in art_request_match.group("body"):
    errors.append("Visuals/NFT Explorer must refresh its art source even when cached art rows already exist")

for token in (
    '"art", "photography", "basepaint", "design", "superrare"',
    "feedPresentationRows(state, 0U, channelId)",
    'row.insert(QStringLiteral("_artChannelSource"), true)',
    ".intent = application::read::model::ReadIntent::Prefetch",
):
    if token not in runtime:
        errors.append(f"Explorer art-channel prefetch missing {token!r}")

for token in (
    "property var supplementalRows: []",
    "property bool supplementalBusy: false",
    "function rowHasImage(data)",
    "function containsCollectibleLink(data)",
    "if (root.containsCollectibleLink(row))",
    "if (artContext && root.rowHasImage(row))",
):
    if token not in feed:
        errors.append(f"NFT live-feed filtering missing {token!r}")

for surface_name, surface in (("FeedSurface", feed), ("DetailSurface", detail)):
    if "ScrollBar.vertical: GenyScrollBar" in surface:
        errors.append(f"{surface_name} must not expose a visible primary reading scrollbar")
# The profile editor is a long form and intentionally has a scrollbar; the
# profile reading surface itself must remain free of a visible primary bar.
if "ScrollBar.vertical: GenyScrollBar" in profile.split("id: streakDrawer", 1)[0]:
    errors.append("Profile reading surface must not expose a visible primary scrollbar")

for token in (
    "primaryRequestGeneration",
    "beginPrimaryRequestGeneration",
    "isCurrentPrimaryRequestGeneration",
):
    if token not in runtime:
        errors.append(f"Primary navigation generation guard missing {token!r}")

if runtime.count("isCurrentPrimaryRequestGeneration(requestGeneration)") < 6:
    errors.append("Primary profile/cast callbacks must be generation-gated before and after GUI queuing")

for token in (
    'm_activeView = QStringLiteral("cast")',
    "m_detail.clear()",
    'm_activeView = QStringLiteral("profile")',
    "m_rows.clear()",
):
    if token not in bridge:
        errors.append(f"Navigation stale-state reset missing {token!r}")

for token in (
    "root.activeSource = """,
    "sourceStartTimer.restart()",
):
    if token not in superellipse_image:
        errors.append(f"Image stale-frame reset missing {token!r}")

if "retainPreviousWhileLoading: true" in profile:
    errors.append("Profile banner must not retain a previous profile frame while loading")


for token in (
    "options.intent == model::ReadIntent::Prefetch",
    "state.next.reset()",
    ".phase = model::ReadPhase::Ready",
):
    if token not in service:
        errors.append(f"Prefetch continuation degradation missing {token!r}")

prefetch_branch = re.search(
    r"if \(options\.intent == application::read::model::ReadIntent::Prefetch\)\s*\{(?P<body>.*?)\n\s*\}\n\s*const auto mode",
    routed,
    re.DOTALL,
)
if not prefetch_branch:
    errors.append("Routed async gateway is missing a dedicated Prefetch branch")
else:
    body = prefetch_branch.group("body")
    for forbidden in ("beginAttempt(", "recordFailure(", "recordSuccess("):
        if forbidden in body:
            errors.append(f"Prefetch reads must not mutate source health; found {forbidden}")

for token in (
    "isBackgroundPriority",
    "retryPolicyFor",
    "maximumAttemptsFor",
    "return isBackgroundPriority(priority) ? 1U : 3U",
):
    if token not in hypersnap_request:
        errors.append(f"Hypersnap prefetch retry budget missing {token!r}")

if errors:
    print("Explorer semantic/source contract guard: FAILED")
    for error in errors:
        print(" -", error)
    raise SystemExit(1)

print("Explorer semantic/source contract guard: PASS")
