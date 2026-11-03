.pragma library

// UI policy is measured in Unicode code points; publication is always
// segmented by actual UTF-8 bytes to satisfy Farcaster CAST_ADD validation.
// The source is never silently truncated or posted before user confirmation.
var basicCharacters = 1024
var proCharacters = 4096
var singleCastBytes = 1024
var singleCastEmbeds = 2
var basicMedia = 1
var proMedia = 4
var maximumThreadCasts = 24

function scalars(value) {
    return Array.from(String(value || ""))
}
function graphemes(value) {
    // Qt QML's JS runtime does not guarantee Intl.Segmenter. Group common
    // combining marks, variation selectors, skin tones, flag pairs and ZWJ
    // emoji without splitting a visible symbol into separate cast messages.
    var points = scalars(value)
    var groups = []
    var previous = -1
    var consecutiveRegional = 0
    for (var i = 0; i < points.length; ++i) {
        var point = points[i]
        var cp = point.codePointAt(0)
        var regional = cp >= 0x1F1E6 && cp <= 0x1F1FF
        var combining = cp >= 0x0300 && cp <= 0x036F
            || cp >= 0x0591 && cp <= 0x05BD
            || cp >= 0x064B && cp <= 0x065F
            || cp >= 0x0670 && cp <= 0x0670
            || cp >= 0x08D3 && cp <= 0x08FF
            || cp >= 0x1AB0 && cp <= 0x1AFF
            || cp >= 0x1DC0 && cp <= 0x1DFF
            || cp >= 0x20D0 && cp <= 0x20FF
            || cp >= 0xFE20 && cp <= 0xFE2F
            || cp >= 0xFE00 && cp <= 0xFE0F
            || cp >= 0x1F3FB && cp <= 0x1F3FF
        var join = groups.length > 0 && (combining || cp === 0x200D
            || previous === 0x200D
            || regional && consecutiveRegional % 2 === 1)
        if (join) groups[groups.length-1] += point
        else groups.push(point)
        if (regional) ++consecutiveRegional
        else consecutiveRegional = 0
        previous = cp
    }
    return groups
}
function characterCount(value) {
    return graphemes(value).length
}
function utf8Length(value) {
    var bytes = 0
    var characters = scalars(value)
    for (var i = 0; i < characters.length; ++i) {
        var cp = characters[i].codePointAt(0)
        bytes += cp < 128 ? 1 : cp < 2048 ? 2 : cp < 65536 ? 3 : 4
    }
    return bytes
}
function limits(pro) {
    return {characters: pro ? proCharacters : basicCharacters,
            media: pro ? proMedia : basicMedia}
}
function partitionText(text, mentionFids, mentionPositions) {
    var points = graphemes(text)
    var parts = []
    var cursor = 0
    var offsets = [0]
    for (var i = 0; i < points.length; ++i)
        offsets.push(offsets[i] + utf8Length(points[i]))
    // Preserve mention byte positions instead of attaching all mentions to
    // the root cast when the text becomes a thread.
    var references = []
    for (var j = 0; j < mentionFids.length; ++j) {
        var pos = Number(mentionPositions[j])
        if (!Number.isInteger(pos) || pos < 0 || pos > offsets[offsets.length - 1])
            throw Error("Invalid mention byte position")
        references.push({fid:mentionFids[j],position:pos})
    }
    if (references.length > 10) throw Error("Too many mentions")
    while (cursor < points.length) {
        var start = cursor
        var end = cursor
        while (end < points.length && offsets[end + 1] - offsets[start] <= singleCastBytes)
            ++end
        if (end === start) throw Error("Invalid Unicode character")
        // When a boundary cuts a word, prefer the last whitespace nearby.
        if (end < points.length && end - start > 24) {
            var candidate = end
            while (candidate > start + 24 && offsets[end] - offsets[candidate] < 100) {
                if (/\s/.test(points[candidate - 1])) { end = candidate; break }
                --candidate
            }
        }
        var localFids = []
        var localPositions = []
        for (var k = 0; k < references.length; ++k) {
            var ref = references[k]
            var lower = offsets[start]
            var upper = offsets[end]
            if (ref.position >= lower && (ref.position < upper
                || (end === points.length && ref.position === upper))) {
                localFids.push(ref.fid)
                localPositions.push(ref.position - lower)
            }
        }
        parts.push({text:points.slice(start,end).join(""),
                    mentionFids:localFids,mentionPositions:localPositions,embeds:[]})
        cursor = end
    }
    return parts
}
function publicationPlan(text, mentionFids, mentionPositions, embeds, extras, pro, mediaCount) {
    text = String(text || "")
    mentionFids = mentionFids || []
    mentionPositions = mentionPositions || []
    embeds = embeds || []
    extras = extras || []
    var policy = limits(pro)
    if (characterCount(text) > policy.characters)
        return {error:"character_limit",characters:characterCount(text),limit:policy.characters}
    var actualMedia = Number.isInteger(mediaCount) ? mediaCount : embeds.length
    if (actualMedia > policy.media || embeds.length > policy.media + 2)
        return {error:"media_limit",media:actualMedia,limit:policy.media}
    var parts = partitionText(text,mentionFids,mentionPositions)
    if (parts.length === 0 && embeds.length > 0)
        parts.push({text:"",mentionFids:[],mentionPositions:[],embeds:[]})
    for (var i = 0; i < embeds.length; ++i) {
        var target = Math.floor(i / singleCastEmbeds)
        if (!parts[target]) parts.push({text:"",mentionFids:[],mentionPositions:[],embeds:[]})
        parts[target].embeds.push(embeds[i])
    }
    for (var j = 0; j < extras.length; ++j) {
        var tail = String(extras[j] || "")
        if (!tail.length) return {error:"empty_thread_cast"}
        var nextParts = partitionText(tail,[],[])
        for (var n = 0; n < nextParts.length; ++n) parts.push(nextParts[n])
    }
    if (parts.length === 0 || parts.length > maximumThreadCasts)
        return {error:"thread_limit",limit:maximumThreadCasts}
    return {parts:parts,characters:characterCount(text),
            media:embeds.length,requiresThread:parts.length > 1}
}
