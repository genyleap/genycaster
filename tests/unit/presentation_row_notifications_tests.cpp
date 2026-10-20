#include <QCoreApplication>
#include <QMetaProperty>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <memory>
#include <QVariantMap>
#include <QVariantList>

#include <cassert>

#include "presentation_bridge.hpp"

namespace {
[[nodiscard]] QVariantMap castRow(const QString& id, const QString& quoteId = {})
{
    QVariantMap row{
        {QStringLiteral("castId"), id},
        {QStringLiteral("text"), QStringLiteral("Cached local cast")},
        {QStringLiteral("likeCount"), 0},
    };
    if (!quoteId.isEmpty()) {
        row.insert(QStringLiteral("embeds"), QVariantList{
            QVariantMap{{QStringLiteral("kind"), QStringLiteral("cast-reference")},
                        {QStringLiteral("castId"), quoteId}},
        });
    }
    return row;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    PresentationBridge bridge{};
    const QMetaObject* meta = bridge.metaObject();
    const int rowsProperty = meta->indexOfProperty("rows");
    assert(rowsProperty >= 0);
    assert(meta->property(rowsProperty).notifySignal().name() == "rowsChanged");
    int rowSignals{};
    int stateSignals{};
    QObject::connect(&bridge, &PresentationBridge::rowsChanged, &bridge, [&] { ++rowSignals; });
    QObject::connect(&bridge, &PresentationBridge::stateChanged, &bridge, [&] { ++stateSignals; });

    // This is a real QML property binding, not a source-pattern assertion:
    // unrelated bridge state must not re-evaluate a potentially huge rows list.
    QQmlEngine engine{};
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);
    QQmlComponent component{&engine};
    component.setData(R"QML(
import QtQml
QtObject {
    property var feedRows: bridge.rows
    property int rowBindingUpdates: 0
    onFeedRowsChanged: rowBindingUpdates += 1
}
)QML", QUrl{});
    assert(component.isReady());
    std::unique_ptr<QObject> binding{component.create()};
    assert(binding);
    const int initialBindings = binding->property("rowBindingUpdates").toInt();

    QVariantList initial{};
    constexpr int largeTimeline = 2048;
    initial.reserve(largeTimeline);
    for (int index = 0; index < largeTimeline; ++index) {
        initial.push_back(castRow(QStringLiteral("0xrow-%1").arg(index),
            index == 0 ? QStringLiteral("0xquoted") : QString{}));
    }
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
        initial, QStringLiteral("next-page"), {}, {}, {}, false, false, false, false);
    assert(bridge.rows().size() == largeTimeline);
    assert(rowSignals == 1);
    assert(binding->property("rowBindingUpdates").toInt() == initialBindings + 1);

    // Repeated status and detail events must not replay/re-diff 2048 rows.
    const int afterInitial = binding->property("rowBindingUpdates").toInt();
    for (int i = 0; i < 32; ++i) {
        bridge.applyLookupState(QStringLiteral("Still reading"), {}, {}, true, false, false);
        bridge.applyNetworkReachability(true, true);
        bridge.applyDetail(QStringLiteral("cast"),
            QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xother")}},
            {}, {}, {}, false, false, false);
    }
    assert(stateSignals >= 96);
    assert(rowSignals == 1);
    assert(binding->property("rowBindingUpdates").toInt() == afterInitial);

    // In-place busy cached snapshots do not invalidate stable delegates.
    // A detail surface has another identity: restore the original feed before
    // testing an in-place busy snapshot.
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
        initial, QStringLiteral("next-page"), {}, {}, {}, false, false, false, false);
    const int afterRestore = rowSignals;
    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
        {}, {}, QStringLiteral("Refreshing"), {}, {}, true, false, false, false);
    assert(rowSignals == afterRestore);
    assert(bridge.rows().size() == largeTimeline);

    // Exact cast hydration must update the feed binding only when a cast
    // references the completed preview. Repeated identical evidence is inert.
    bridge.applyReferencedCastPreview(QStringLiteral("0xunrelated"),
        QVariantMap{{QStringLiteral("text"), QStringLiteral("Ignore")}});
    assert(rowSignals == afterRestore);
    bridge.applyReferencedCastPreview(QStringLiteral("0xquoted"),
        QVariantMap{{QStringLiteral("text"), QStringLiteral("Quoted root")}});
    assert(rowSignals == afterRestore + 1);
    assert(bridge.rows().first().toMap().value(QStringLiteral("_referencedRevision"))
        .toString().contains(QStringLiteral("quoted")));

    bridge.applyViewerReactionDelta(QVariantList{
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xrow-0")},
                    {QStringLiteral("liked"), true}},
    });
    assert(rowSignals == afterRestore + 2);
    assert(bridge.rows().first().toMap().value(QStringLiteral("likedByMe")).toBool());
    bridge.applyViewerReactionDelta(QVariantList{
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xrow-0")},
                    {QStringLiteral("liked"), true}},
    });
    assert(rowSignals == afterRestore + 2);

    bridge.applyList(QStringLiteral("feed"), QStringLiteral("trending"),
        QVariantList{castRow(QStringLiteral("0xnew-page"))},
        {}, {}, {}, {}, false, false, false, true);
    assert(rowSignals == afterRestore + 3);
    assert(bridge.rows().size() == largeTimeline + 1);
    assert(bridge.rows().last().toMap().value(QStringLiteral("castId")).toString()
        == QStringLiteral("0xnew-page"));
    assert(bridge.rows().first().toMap().value(QStringLiteral("likedByMe")).toBool());

    const int afterPagination = rowSignals;
    bridge.applyWriteState(QStringLiteral("checking"), true, {}, {});
    assert(rowSignals == afterPagination); // Write progress is not a feed delta.

    bridge.applyAuthenticationState(true, QStringLiteral("authenticated"), 42,
        QStringLiteral("viewer"), QStringLiteral("Viewer"), {}, {}, {}, {});
    bridge.applyWriteState(QStringLiteral("queued"), false, {}, {},
        QVariantMap{{QStringLiteral("operationKind"), QStringLiteral("publish-cast")},
                    {QStringLiteral("castId"), QStringLiteral("0xoptimistic")},
                    {QStringLiteral("text"), QStringLiteral("Local outgoing cast")}});
    assert(rowSignals == afterPagination + 1);
    assert(bridge.rows().first().toMap().value(QStringLiteral("castId"))
        == QStringLiteral("0xoptimistic"));

    // Leaving the account removes only its optimistic cast, without losing
    // any of the 2049 public cached feed entries.
    bridge.applyAuthenticationState(true, QStringLiteral("authenticated"), 43,
        QStringLiteral("other"), QStringLiteral("Other"), {}, {}, {}, {});
    assert(rowSignals == afterPagination + 2);
    assert(bridge.rows().size() == largeTimeline + 1);
    assert(bridge.rows().first().toMap().value(QStringLiteral("castId"))
        == QStringLiteral("0xrow-0"));

    const int beforeProfile = rowSignals;
    bridge.openProfileFid(77);
    assert(rowSignals == beforeProfile + 1);
    assert(bridge.rows().isEmpty());
    return 0;
}
