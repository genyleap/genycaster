import QtQuick

QtObject {
    readonly property string profileBio: "Building native social software for open protocols."
    readonly property string profileRole: "Builder · Genycaster"
    readonly property int followerCount: 12840
    readonly property int followingCount: 864
    readonly property int castCount: 3214

    readonly property var stories: [
        { displayName: "commpez.eth", username: "compez.eth", state: "partial", avatarUrl: "" },
        { displayName: "pichi", username: "pichi", state: "unread", avatarUrl: "" },
        { displayName: "dwr.eth", username: "dwr", state: "live", avatarUrl: "" },
        { displayName: "linda", username: "linda", state: "special", avatarUrl: "" },
        { displayName: "alice", username: "alice", state: "viewed", avatarUrl: "" }
    ]

    readonly property var tokenPulse: [
        { name: "Bitcoin", symbol: "BTC", price: "$108,420", change: "+1.8%", positive: true, samples: [0.30, 0.38, 0.34, 0.52, 0.48, 0.68, 0.72] },
        { name: "Ethereum", symbol: "ETH", price: "$4,620", change: "+0.9%", positive: true, samples: [0.42, 0.39, 0.46, 0.44, 0.55, 0.57, 0.63] },
        { name: "Genyleap", symbol: "GENY", price: "$0.00060", change: "-0.4%", positive: false, samples: [0.70, 0.66, 0.69, 0.58, 0.61, 0.55, 0.52] }
    ]

    readonly property var calendarEventDays: [3, 6, 10, 14, 22, 28]

    readonly property var activity: [
        { name: "pichi", action: "replied to your cast", avatarUrl: "" },
        { name: "moxie", action: "followed you", avatarUrl: "" },
        { name: "alice", action: "recast your post", avatarUrl: "" }
    ]

    readonly property var suggestions: [
        { displayName: "Yuki", username: "yuki", reason: "Followed by 8 people you know", avatarUrl: "" },
        { displayName: "Nadia", username: "nadia", reason: "Popular in Open Source", avatarUrl: "" },
        { displayName: "Theo", username: "theo", reason: "Builder you may know", avatarUrl: "" }
    ]
}
