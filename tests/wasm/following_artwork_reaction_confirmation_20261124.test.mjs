import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
const read = path => readFileSync(path,"utf8");
function method(qml,name,root) {
    const start=qml.indexOf("    function "+name+"(");
    const end=qml.indexOf("\n    }",start);
    assert.ok(start>=0 && end>start,name);
    return new Function("root",qml.slice(start,end+6)+"\nreturn "+name+";")(root);
}
test("following artists can share captionless artwork without marketplace links",()=>{
    const qml=read("ui/components/feed/FeedSurface.qml");
    const root={activeFeed:"following"};
    for (const name of ["embedUrl","isCollectibleMarketplaceUrl","isNftAssetLocator",
        "isFungibleTokenEvidence","looksLikeArtworkUrl","channelSlugFor","isArtChannelCast",
        "hasArtistIdentity","hasDirectArtworkMedia","hasCollectible"])
        root[name]=method(qml,name,root);
    root.collectiblePreview=()=>({});
    root.classifiedFamily=e=>e.kind||"web";
    root.rowHasImage=row=>root.hasDirectArtworkMedia(row);
    root.containsCollectibleLink=()=>false;
    const image={kind:"image",url:"https://imagedelivery.net/account/piece/original"};
    const artwork={authorBio:"Generative artist | creative coder",text:"GM!",embeds:[image]};
    assert.equal(root.hasCollectible(artwork),true);
    assert.equal(root.hasCollectible({...artwork,text:"",authorBio:"Painter"}),true);
    assert.equal(root.hasCollectible({...artwork,authorBio:"Software developer"}),false);
    assert.equal(root.hasCollectible({...artwork,text:"My son was diagnosed with diabetes"}),false);
    assert.equal(root.hasCollectible({...artwork,isReply:true}),false);
    assert.equal(root.hasCollectible({...artwork,embeds:[{kind:"web",url:"https://example.com/news",previewImage:image.url}]}),false);
    assert.equal(root.hasCollectible({...artwork,embeds:[{kind:"token",url:image.url}]}),false);
    root.activeFeed="nfts";
    assert.equal(root.hasCollectible(artwork),false);
    const bindings=read("ui/app/runtime/bindings.cpp");
    assert.match(bindings,/\{QStringLiteral\("authorBio"\), profile \? QString::fromStdString\(profile->bio\)/);
    assert.match(bindings,/feed == QStringLiteral\("following"\) && bridge\.followingArtworkScan\(\)\s*\? std::max\(40, bridge\.feedPageSize\(\)\)/);
    assert.match(qml,/visible: stableRows\.count > 0 && footer\.moreFiltered/);
    assert.match(qml,/root\.loadMoreRequested\(\)/);
});
test("like/recast click dispatches first and only protocol confirmation changes truth",()=>{
    const bar=read("ui/components/feed/EngagementBar.qml");
    const bridge=read("src/presentation/qt/presentation_bridge.cpp");
    const browser=read("ui/app/browser/signer_bridge.hpp");
    const js=read("ui/wasm/protocol-signer-pre.js");
    assert.match(bar,/root\.likeRequested\(next\)\s+likeButton\.playReactionMotion/);
    assert.match(bar,/root\.recastRequested\(next\)\s+recastButton\.playReactionMotion/);
    assert.doesNotMatch(bar,/CastReactions\.setLocal\(/);
    assert.match(bridge,/void PresentationBridge::setReaction\([\s\S]*?canonicalProtocolCastId\(targetCastId\)/);
    assert.match(bridge,/!targetCastId\.isEmpty\(\) && m_writePhase == QStringLiteral\("confirmed"\)/);
    assert.match(bridge,/updateReactionPresentation\(m_pendingFeedRows, targetCastId, reaction, active\)/);
    assert.match(browser,/operation == QStringLiteral\("reaction"\)[\s\S]*?makeCastReference\(targetFid, target\)[\s\S]*?QStringLiteral\("undo-recast"\)/);
    assert.match(js,/reaction: action==='reaction' \? String\(payload\.reaction/);
    assert.match(js,/targetCastId: action==='remove'\|\|action==='reaction'/);
    assert.match(bar,/root\.viewDescriptor\.likedByMe !== undefined[\s\S]*?Boolean\(root\.viewDescriptor\.likedByMe\)/);
});
