import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
const read = path => readFileSync(new URL("../../" + path, import.meta.url), "utf8");
function method(qml, name, root = {}) {
    const start = qml.indexOf("    function " + name + "(");
    assert.ok(start >= 0, name);
    const end = qml.indexOf("\n    }", start);
    assert.ok(end > start, name);
    return new Function("root", qml.slice(start, end + 6) + "\nreturn " + name + ";")(root);
}
test("NFT channel parent fallback accepts canonical Farcaster URLs, never arbitrary hosts", () => {
    const qml = read("ui/components/feed/FeedSurface.qml");
    const root = {};
    root.channelSlugFor = method(qml, "channelSlugFor", root);
    root.isArtChannelCast = method(qml, "isArtChannelCast", root);
    assert.equal(root.isArtChannelCast({parentUrl: "https://warpcast.com/~/channel/nft"}), true);
    assert.equal(root.isArtChannelCast({parentUrl: "https://farcaster.xyz/~/channel/art"}), true);
    assert.equal(root.isArtChannelCast({parentUrl: "https://warpcast.com.evil.test/~/channel/nft"}), false);
    assert.equal(root.isArtChannelCast({channelId: "dev", parentUrl: "https://warpcast.com/~/channel/nft"}), false);
    root.containsCollectibleLink = () => false;
    root.rowHasImage = row => !!row.image;
    root.hasArtChannelMediaCandidate = () => false;
    const hasCollectible = method(qml, "hasCollectible", root);
    assert.equal(hasCollectible({parentUrl: "https://warpcast.com/~/channel/nft", image: true, text: "New piece"}), true);
    assert.equal(hasCollectible({parentUrl: "https://warpcast.com/~/channel/nft", text: "Discussion"}), false);
});
test("cast previews preserve short text, limit long text and Unicode, and open full conversation", () => {
    const qml = read("ui/components/feed/CastItem.qml");
    const preview = method(qml, "makeTextPreview");
    assert.deepEqual(preview("short", 10, 5), {text: "short", truncated: false});
    assert.deepEqual(preview("𐐀𐐁𐐂", 2, 5), {text: "𐐀𐐁…", truncated: true});
    assert.deepEqual(preview("one\ntwo\nthree", 100, 2), {text: "one\ntwo…", truncated: true});
    assert.equal(preview("word ".repeat(300), 320, 7).truncated, true);
    assert.ok(Array.from(preview("word ".repeat(300), 320, 7).text).length <= 321);
    assert.match(qml, /objectName: "castReadMore"[\s\S]*?root\.openRequested\(String\(root\.modelData\.castId\)\)/);
    assert.match(qml, /useRichSidecar: !root\.textPreview\.truncated/);
});
test("username resolution is completion of one navigation, not a self breadcrumb", () => {
    const bridge = read("src/presentation/qt/presentation_bridge.cpp");
    const bindings = read("ui/app/runtime/bindings.cpp");
    assert.match(bridge, /void PresentationBridge::resolveProfileUsername[\s\S]*?m_profileFid != 0[\s\S]*?QScopedValueRollback<bool> suppressHistory\{m_restoringDetailHistory, true\};[\s\S]*?openProfileFid\(fid\)/);
    assert.match(bindings, /resolveProfileUsername\(/);
});
test("profile metrics use verified custody flags rather than read provider and streak stays with the ring", () => {
    const shell = read("ui/components/shell/AppShell.qml");
    const card = read("ui/components/profile/ProfileTrustCard.qml");
    assert.match(shell, /accountProtocol: presentationBridge\.nativeCustodyAccount \? "hypersnap"\s*: presentationBridge\.farcasterCustodyAccount \? "farcaster"/);
    assert.match(card, /objectName: "sidebarStreakRing"[\s\S]*?objectName: "sidebarStreakMetric"/);
    assert.equal((card.match(/objectName: "sidebarStreakMetric"/g) || []).length, 1);
    assert.match(card, /quilibrium\/hypersnap\.svg/);
    assert.match(card, /brands\/farcaster\.svg/);
});
