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
    assert.equal(hasCollectible({parentUrl: "https://warpcast.com/~/channel/nft", text: "Discussion"}), true);
    assert.equal(hasCollectible({text: "NFT royalties and ERC-721"}), true);
    assert.equal(hasCollectible({text: "My new painting", image: true}), true);
    assert.equal(hasCollectible({text: "Lunch photo", image: true}), false);
    assert.equal(hasCollectible({text: "Discussing JavaScript"}), false);
    assert.equal(hasCollectible({text: "Doctors took a collection of samples during treatment", image: true}), false);
    assert.equal(hasCollectible({text: "A few stills from a video, here's the new collection", image: true}), false);
    assert.equal(hasCollectible({text: "Daily tasks, check-ins & onchain claims. Get NFT rewards", image: true}), false);
    assert.equal(hasCollectible({text: "A new generative art piece", image: true}), true);
    assert.equal(hasCollectible({_artChannelSource: true, text: "Unrelated feed row", image: true}), false);
    const marketplace = method(qml, "isCollectibleMarketplaceUrl");
    for (const link of ["https://opensea.io/assets/base/0xabc/1", "https://magiceden.io/collections/xyz", "https://zora.co/collect/base:0xabc", "https://objkt.com/tokens/abc/1", "https://artblocks.io/project/1"])
        assert.equal(marketplace("Look: " + link), true, link);
    for (const link of ["https://opensea.io.evil.test/assets/1", "https://example.com/?url=opensea.io/assets/1", "https://zora.co/coin/xyz", "https://zora.co/@user", "https://example.com/coins/mint"])
        assert.equal(marketplace(link), false, link);
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
    // QV4 does not provide trimEnd; emulate that runtime in Node as well.
    const originalTrimEnd = String.prototype.trimEnd;
    try {
        String.prototype.trimEnd = undefined;
        assert.deepEqual(preview("one  \ntwo", 100, 1), {text: "one…", truncated: true});
        assert.equal(preview("The wait is over. ".repeat(100), 360, 7).truncated, true);
    } finally {
        String.prototype.trimEnd = originalTrimEnd;
    }
    assert.match(qml, /objectName: "castTextExcerpt"[\s\S]*?sourceText: visible \? root\.textPreviewBody/);
    assert.match(qml, /qsTrId\("genycaster\.cast\.read_more"\)/);
    const readMore = qml.slice(qml.indexOf('id: readMoreLink'), qml.indexOf('// Protocol cast order:'));
    assert.match(readMore, /background: null/);
    assert.match(readMore, /Accessible\.role: Accessible\.Link/);
    assert.doesNotMatch(readMore, /NeonSurface|ActionButton|customForegroundColor/);
    assert.match(qml, /Boolean\(root\.textPreview && root\.textPreview\.truncated\)/);
    assert.doesNotMatch(qml, /preview\.trimEnd\(/);
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
    assert.match(card, /objectName: "sidebarStreakMetric"[\s\S]*?objectName: "sidebarStreakRing"/);
    assert.match(card, /objectName: "sidebarProfileAction"[\s\S]*?objectName: "sidebarFidMetric"/);
    assert.match(card, /sourceSize: Qt\.size\(Math\.ceil\(Theme\.iconSizeSmall \* Screen\.devicePixelRatio\)/);
    assert.equal((card.match(/objectName: "sidebarStreakMetric"/g) || []).length, 1);
    assert.match(card, /quilibrium\/hypersnap\.svg/);
    assert.match(card, /brands\/farcaster\.svg/);
});
