.pragma library

// A fixed, auditable first-party catalog. Casts carry ordinary HTTPS image
// embeds so other clients render the same PNG even without a sticker picker.
// Never accept arbitrary publisher-controlled URLs from picker UI.
var origin = "https://genycaster.xyz/stickers/v1.0.0/"
var resource = "qrc:/qt/qml/Farcaster/Client/assets/stickers/v1/"

var stickers = [
    {id:"bloom",pack:"originals",title:"Bloom",keywords:"flower happy smile blossom pink"},
    {id:"orbit",pack:"originals",title:"Orbit",keywords:"planet ring space orbit purple"},
    {id:"pulse",pack:"originals",title:"Pulse",keywords:"fire flame energy streak"},
    {id:"spark",pack:"originals",title:"Spark",keywords:"star sparkle magic blue"},
    {id:"love",pack:"reactions",title:"Love",keywords:"heart kiss affection pink"},
    {id:"laugh",pack:"reactions",title:"Laugh",keywords:"haha lol funny laugh"},
    {id:"wow",pack:"reactions",title:"Wow",keywords:"surprise shocked amazed"},
    {id:"cheer",pack:"reactions",title:"Cheer",keywords:"congratulations celebrate clap thank you"},
    {id:"gm",pack:"web3",title:"GM",keywords:"good morning sun gm"},
    {id:"rocket",pack:"web3",title:"Rocket",keywords:"launch moon wagmi rocket"},
    {id:"onchain",pack:"web3",title:"Onchain",keywords:"link blockchain connected chain"},
    {id:"verified",pack:"web3",title:"Verified",keywords:"verified check success security"}
]

function itemForId(id) {
    for (var i=0;i<stickers.length;++i)
        if (stickers[i].id===id) return stickers[i]
    return null
}

function embedUrl(id) {
    return itemForId(id) ? origin+id+".png" : ""
}

function previewUrl(id) {
    return itemForId(id) ? resource+id+".png" : ""
}

function decodeIds(json,maxLength) {
    var raw=[]
    try { raw=JSON.parse(String(json||"[]")) } catch (_) { return [] }
    if (!Array.isArray(raw)) return []
    var result=[]
    for (var i=0;i<raw.length&&result.length<maxLength;++i) {
        var id=raw[i]
        if (typeof id==="string" && itemForId(id) && result.indexOf(id)<0)
            result.push(id)
    }
    return result
}

function toggleFavorite(ids,id) {
    if (!itemForId(id)) return ids.slice()
    var result=ids.slice()
    var index=result.indexOf(id)
    if (index<0) result.unshift(id)
    else result.splice(index,1)
    return result.slice(0,stickers.length)
}

function recordRecent(ids,id) {
    if (!itemForId(id)) return ids.slice()
    var result=[id]
    for(var i=0;i<ids.length&&result.length<12;++i)
        if(ids[i]!==id && itemForId(ids[i])) result.push(ids[i])
    return result
}

function matches(sticker,query) {
    if(!query) return true
    var searchable=(sticker.id+" "+sticker.title+" "+sticker.keywords).toLowerCase()
    return searchable.indexOf(query)>=0
}

function filtered(pack,query,favorites,recent) {
    var filter=String(pack||"all")
    var q=String(query||"").trim().toLowerCase().slice(0,70)
    var ordered=stickers
    if(filter==="favorites") ordered=favorites.map(itemForId).filter(Boolean)
    else if(filter==="recent") ordered=recent.map(itemForId).filter(Boolean)
    return ordered.filter(function(item) {
        return (filter==="all"||filter==="recent"||filter==="favorites"
                ||item.pack===filter) && matches(item,q)
    })
}
