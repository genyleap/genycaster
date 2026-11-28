#include "presentation_bridge.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

#include <memory>
#include <source_location>

namespace {
void require(const bool condition,
             const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Profile bridge snapshot failed at %s:%u", location.file_name(), location.line());
}

QVariantMap row(const QString& id, const int time, const int likes)
{
    return {{"castId", id}, {"text", id}, {"createdAtMs", time}, {"likeCount", likes}};
}
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    PresentationBridge bridge{};
    bridge.applyAuthenticationState(true, "authenticated", 42, "viewer", "Viewer", {}, {}, {}, {});
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("bridge", &bridge);
    QQmlComponent component{&engine};
    component.setData(R"QML(
import QtQml
QtObject {
    property var snapshot: bridge.profileRowSnapshot
    property int snapshotChanges: 0
    onSnapshotChanged: ++snapshotChanges
    property var rows: snapshot && snapshot.rows ? snapshot.rows : []
    property string token: snapshot && snapshot.token ? snapshot.token : ""
    property string mode: "newest"
    property string ranked: {
        if (bridge.activeView !== "profile" || !snapshot || !snapshot.rows) return ""
        const order = bridge.profileRowOrder(snapshot.token, bridge.profileFeed, mode, "likes")
        if (order.length !== rows.length) return "stale"
        const ids = []
        for (let i=0; i<order.length; ++i) ids.push(rows[order[i]].castId)
        return ids.join(",")
    }
}
)QML", QUrl{});
    if (!component.isReady()) qWarning().noquote()<<component.errorString();
    require(component.isReady());
    const std::unique_ptr<QObject> binding{component.create()};
    require(static_cast<bool>(binding));
    require(binding->property("ranked").toString().isEmpty());

    bridge.openProfileFid(77);
    bridge.applyList("profile","user-posts",
        {row("0xa",20,1),row("0xb",30,2),row("0xc",30,3)},
        "next", {}, {}, {}, false, false, false, false);
    require(binding->property("ranked").toString()=="0xb,0xc,0xa");
    const QString firstToken=binding->property("token").toString();
    require(!firstToken.isEmpty());
    require(bridge.profileRowSnapshot().value("token").toString()==firstToken);
    require(bridge.profileRowOrder(firstToken,"user-posts","newest","likes")==QVariantList{1LL,2LL,0LL});
    require(binding->setProperty("mode","oldest"));
    require(binding->property("ranked").toString()=="0xa,0xb,0xc");

    const int priorChanges=binding->property("snapshotChanges").toInt();
    bridge.applyLookupState("refreshing", {}, {}, true, false, false);
    require(binding->property("token").toString()==firstToken);
    require(binding->property("snapshotChanges").toInt()==priorChanges);
    bridge.applyList("profile","user-posts",{row("0xd",10,4)}, {}, {}, {}, {},
        false, false, false, true);
    require(binding->property("ranked").toString()=="0xd,0xa,0xb,0xc");
    const QString pageToken=binding->property("token").toString();
    require(pageToken!=firstToken);
    require(bridge.profileRowOrder(firstToken,"user-posts","oldest","likes").isEmpty());

    bridge.selectProfileFeed("user-popular");
    require(bridge.profileRowOrder(pageToken,"user-popular","newest","likes").isEmpty());
    bridge.applyList("profile","user-popular",
        {row("0xe",10,4),row("0xf",30,2),row("0xg",30,4)}, {}, {}, {}, {},
        false, false, false, false);
    require(binding->property("ranked").toString()=="0xg,0xe,0xf");
    const QString popularToken=binding->property("token").toString();
    require(bridge.profileRowOrder(popularToken,"user-posts","newest","likes").isEmpty());

    bridge.openProfileFid(88);
    require(binding->property("ranked").toString().isEmpty());
    require(bridge.profileRowOrder(popularToken,"user-popular","newest","likes").isEmpty());
    bridge.selectFeed("recent");
    require(binding->property("ranked").toString().isEmpty());
    require(bridge.profileRowSnapshot().isEmpty());
    qInfo()<<"Production bridge QML snapshot, pagination, sorting, stale tokens and route isolation passed";
    return 0;
}
