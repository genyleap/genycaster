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


shell = read("ui/components/AppShell.qml")
feed = read("ui/components/FeedSurface.qml")
runtime = read("ui/mobile/main.cpp")
model = read("src/application/read_model.cppm")
service = read("src/application/read_async_service.cpp")
routed = read("src/routing/read_async_gateway.cpp")
hypersnap_request = read("src/providers/hypersnap/async_request.cpp")

for token in (
    'const source = nextMode === "for-you"',
    '? "following" : "trending"',
    'presentationBridge.activeFeed !== source',
):
    if token not in shell:
        errors.append(f"Explorer source selection missing {token!r}")

for token in (
    "property int visualPrefetchPages: 0",
    "property int conversationPrefetchPages: 0",
    "property int developerPrefetchPages: 0",
    "readonly property int filterPrefetchLimit: 3",
    "function semanticKind(data)",
    'if (root.looksDeveloper(data))',
    'if (root.hasVisual(data))',
    'if (root.isConversation(data))',
    'return "other"',
):
    if token not in feed:
        errors.append(f"Explorer semantic presentation missing {token!r}")

semantic_match = re.search(
    r"function semanticKind\(data\)\s*\{(?P<body>.*?)\n\s*\}", feed, re.DOTALL
)
if semantic_match:
    body = semantic_match.group("body")
    expected_order = [
        "root.looksDeveloper(data)",
        "root.hasVisual(data)",
        "root.isConversation(data)",
        'return "other"',
    ]
    positions = [body.find(token) for token in expected_order]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        errors.append("Explorer semantic precedence must remain Devs > Visuals > Conversations > Other")
else:
    errors.append("Explorer semanticKind() body is not structurally readable")

if re.search(r"onPresentationModeChanged\s*:\s*\{[^}]*PrefetchPages\s*=\s*0", feed, re.DOTALL):
    errors.append("Explorer tab switching must not reset semantic pagination budgets")

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
    "maximumContextHydrationsPerFeedState{8U}",
    "maximumVisibleReactionHydrations{12U}",
    "keepReferencedPreviewRequest",
    ".intent = application::read::model::ReadIntent::Prefetch",
    "allowRecoveryProbe && !continuationRequest",
):
    if token not in runtime:
        errors.append(f"Explorer runtime isolation missing {token!r}")

if runtime.count("isCurrentFeedRequestGeneration(requestGeneration)") < 3:
    errors.append("Feed generation must gate worker completion and queued GUI/context completions")

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
