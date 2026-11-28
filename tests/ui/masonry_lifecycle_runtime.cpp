#include "../../ui/app/social/feed_rows.hpp"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <source_location>
#include <vector>

namespace {
std::atomic_int lifecycleWarnings{0};
QtMessageHandler previousHandler{nullptr};
void observeWarnings(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        if (message.contains("Binding loop") || message.contains("ReferenceError:")
            || message.contains("TypeError:") || message.contains("during incubation")
            || message.contains("items in the process"))
            lifecycleWarnings.fetch_add(1, std::memory_order_relaxed);
    }
    if (previousHandler) previousHandler(type, context, message);
}
void require(const bool condition,
             const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Magazine QML lifecycle failed at %s:%u", location.file_name(), location.line());
}
QUrl writeQml(const QTemporaryDir& directory, const QString& name, const QByteArray& content)
{
    QFile file{directory.filePath(name)};
    require(file.open(QIODevice::WriteOnly));
    require(file.write(content)==content.size());
    file.close();
    return QUrl::fromLocalFile(file.fileName());
}
QList<QQuickItem*> namedItems(QQuickItem* root, const QString& name)
{
    QList<QQuickItem*> result;
    if (root->objectName()==name) result.append(root);
    for (auto* child:root->childItems()) result.append(namedItems(child,name));
    return result;
}
QVariantList sourceRows(const int count, const QString& scope)
{
    QVariantList rows;
    rows.reserve(count);
    for (int i=0;i<count;++i)
        rows.append(QVariantMap{{"stableKey",scope+QString::number(i)},
            {"revisionKey","1"},{"presentationKind","conversation"},{"mediaLayoutKind","other"}});
    return rows;
}
}

int main(int argc, char** argv)
{
    QGuiApplication app{argc,argv};
    require(app.arguments().size()==2);
    const QString wallPath=QFileInfo(app.arguments().at(1)).absoluteFilePath();
    require(QFileInfo::exists(wallPath));
    previousHandler=qInstallMessageHandler(observeWarnings);
    {
        QTemporaryDir qml;
        require(qml.isValid());
        qmlRegisterType<FeedRowsModel>("Farcaster.Client",1,0,"FeedRowsModel");
        qmlRegisterType<MasonryWindowModel>("Farcaster.Client",1,0,"MasonryWindowModel");
        // Only visual chrome is substituted. The production wall, native
        // source/window models, actual Loaders, Repeater and timers are used.
        qmlRegisterSingletonType(writeQml(qml,"Theme.qml",R"QML(
pragma Singleton
import QtQuick
QtObject {
    property int space3: 12
    property int uiDirection: Qt.LeftToRight
    property bool neonEnabled: false
    property bool reduceMotion: true
    property int motionStandard: 0
    property color surfaceRaised: "gray"
    property real curveContainer: 0.6
    function scaleUi(value) { return value }
}
)QML"),"Farcaster.Client",1,0,"Theme");
        qmlRegisterType(writeQml(qml,"Superellipse.qml",R"QML(
import QtQuick
Rectangle {
    property color fillColor: "gray"
    property real borderWidth: 0
    property real curveDepth: 0
    color: fillColor
}
)QML"),"Farcaster.Client",1,0,"Superellipse");
        qmlRegisterType(writeQml(qml,"CastSkeleton.qml",R"QML(
import QtQuick
Item {
    property bool mediaPreview: false
    property bool animate: false
    implicitHeight: 160
    Rectangle { anchors.fill: parent; color: "lightgray" }
}
)QML"),"Farcaster.Client",1,0,"CastSkeleton");
        qmlRegisterType(writeQml(qml,"GenyScrollBar.qml",R"QML(
import QtQuick.Controls
ScrollBar {}
)QML"),"Farcaster.Client",1,0,"GenyScrollBar");
        qmlRegisterType(QUrl::fromLocalFile(wallPath),"Fixture",1,0,"FeedMasonryView");
        const std::unique_ptr<QQmlPropertyMap> controller{QQmlPropertyMap::create()};
        controller->insert("shuttingDown",false);
        controller->insert("stallDiagnosticsEnabled",false);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("renderingController",controller.get());
        QQmlComponent component{&engine};
        component.setData(R"QML(
import QtQuick
import Farcaster.Client 1.0
import Fixture 1.0
Window {
    id: window
    visible: true
    width: 1040
    height: 600
    FeedRowsModel { id: source; objectName: "sourceRows" }
    FeedMasonryView {
        id: wall
        objectName: "wall"
        anchors.fill: parent
        model: source
        targetCardWidth: 420
        maximumColumns: 3
        cacheBuffer: 400
        estimateHeight: function(key,kind,media,width) {
            return Number(key.split(":")[1]) % 2 === 0 ? 140 : 260
        }
        delegate: Component {
            Item {
                objectName: "fixtureCard"
                implicitHeight: parent && parent.rowIndex % 2 === 0 ? 140 : 260
                height: implicitHeight
                property real magazinePaintHeight: implicitHeight
                Loader {
                    asynchronous: true
                    sourceComponent: Component {
                        Item {
                            Repeater {
                                model: 20
                                Rectangle { width: 12; height: 12; color: "steelblue" }
                            }
                        }
                    }
                }
            }
        }
    }
}
)QML", QUrl::fromLocalFile(qml.filePath("Fixture.qml")));
        require(QTest::qWaitFor([&]{return component.isReady()||component.isError();},3000));
        if(component.isError()) qWarning().noquote()<<component.errorString();
        require(component.isReady());
        std::unique_ptr<QObject> object{component.create()};
        auto* window=qobject_cast<QQuickWindow*>(object.get());
        require(window!=nullptr);
        auto* wall=window->findChild<QQuickItem*>("wall");
        auto* source=window->findChild<FeedRowsModel*>("sourceRows");
        require(wall&&source);
        auto* layout=wall->findChild<MasonryWindowModel*>("masonryWindowModel");
        require(layout&&layout->sourceModel()==source);
        require(QTest::qWaitFor([&]{return window->isExposed();},3000));
        require(source->reconcile(sourceRows(500,"fixture:")));
        const auto slotItems=[&]{return namedItems(wall,"masonrySlot");};
        auto settled=[&](const int index) {
            const auto liveSlots=slotItems();
            if(layout->windowIndexForSource(index)<0 || liveSlots.size()!=layout->count()) return false;
            bool targetReady=false;
            for(auto* slot:liveSlots) {
                if(slot->property("retiring").toBool()) return false;
                if(slot->property("sourceIndex").toInt()==index)
                    targetReady=slot->property("cardReady").toBool();
            }
            return targetReady;
        };
        require(QTest::qWaitFor([&]{return settled(0);},5000));
        require(layout->count()<70);
        require(layout->geometryAt(499).value("stableKey").toString()=="fixture:499");
        int requestedJump{-1};
        int synchronousAdmissions{0};
        QObject::connect(layout,&QAbstractItemModel::rowsInserted,wall,
            [&](const QModelIndex&, int first, int last) {
                if (requestedJump < 0) return;
                const auto target = layout->geometryAt(requestedJump);
                require(!target.isEmpty());
                const auto maximum = std::max(0.0, wall->property("contentHeight").toDouble()
                    - wall->height());
                const auto expected = std::clamp(target.value("y").toDouble(), 0.0, maximum);
                require(std::abs(wall->property("contentY").toDouble() - expected) <= 1.0);
                synchronousAdmissions += last - first + 1;
            });
        auto jump=[&](const int index){
            requestedJump = index;
            require(QMetaObject::invokeMethod(wall,"positionViewAtIndex",
                Q_ARG(QVariant,QVariant{index}),Q_ARG(QVariant,QVariant{0})));
            requestedJump = -1;
        };
        std::vector<double> latencies;
        int maxSlots=layout->count();
        QObject::connect(layout,&MasonryWindowModel::countChanged,wall,[&]{
            maxSlots=std::max(maxSlots,layout->count());
        });
        for(const int index:{400,10,300,25,480,100,450,0,390,20,200,0}){
            QElapsedTimer timer;
            timer.start();
            jump(index);
            require(QTest::qWaitFor([&]{return settled(index);},5000));
            latencies.push_back(static_cast<double>(timer.nsecsElapsed())/1e6);
            require(layout->count()<70);
        }
        require(synchronousAdmissions > 0);
        // Real flicking must narrow speculative admissions without changing
        // the retention margin or canceling an existing visible card.
        const auto idleBuffer = wall->property("cacheBuffer").toReal();
        const auto retentionBuffer = wall->property("retentionBuffer").toReal();
        int motionAdmissions{0};
        const auto observeMotion = QObject::connect(layout, &QAbstractItemModel::rowsInserted,
            wall, [&](const QModelIndex&, int first, int last) {
                if (!wall->property("moving").toBool()) return;
                const auto top = wall->property("contentY").toReal();
                const auto buffer = wall->property("motionAdmissionBuffer").toReal();
                for (int row = first; row <= last; ++row) {
                    const auto index = layout->index(row, 0);
                    const auto y = layout->data(index, MasonryWindowModel::CardYRole).toReal();
                    const auto height = layout->data(index, MasonryWindowModel::CardHeightRole).toReal();
                    require(y + height >= top - buffer - 1.0);
                    require(y <= top + wall->height() + buffer + 1.0);
                    ++motionAdmissions;
                }
            });
        require(QMetaObject::invokeMethod(wall, "flick", Q_ARG(qreal, 0.0),
                                          Q_ARG(qreal, -1800.0)));
        require(QTest::qWaitFor([&] {
            return wall->property("moving").toBool()
                && wall->property("motionAdmissionBuffer").isValid()
                && wall->property("motionAdmissionBuffer").toReal() < idleBuffer;
        }, 1000));
        require(wall->property("retentionBuffer").toReal() == retentionBuffer);
        require(QTest::qWaitFor([&] { return !wall->property("moving").toBool(); }, 5000));
        require(wall->property("motionAdmissionBuffer").toReal() == idleBuffer);
        QObject::disconnect(observeMotion);
        require(motionAdmissions > 0);
        qInfo() << "Magazine flick motionAdmissions" << motionAdmissions;
        jump(0);
        require(QTest::qWaitFor([&]{return settled(0);},5000));
        // Exercise rapid crossings before admission has settled.
        for(const int index:{350,0,470,5,460,0}) jump(index);
        require(QTest::qWaitFor([&]{return settled(0);},5000));
        wall->setVisible(false);
        require(QTest::qWaitFor([&]{return layout->count()==0&&slotItems().isEmpty();},5000));
        wall->setVisible(true);
        jump(0);
        require(QTest::qWaitFor([&]{return settled(0);},5000));
        window->setWidth(590);
        require(QTest::qWaitFor([&]{return wall->property("columns").toInt()==1
            && !wall->property("resizeInProgress").toBool() && settled(0);},5000));
        require(source->reconcile(sourceRows(200,"replacement:")));
        jump(0);
        require(QTest::qWaitFor([&]{return settled(0)
            && layout->geometryAt(0).value("stableKey").toString()=="replacement:0";},5000));
        for(auto* slot:slotItems())
            require(slot->property("stableKey").toString().startsWith("replacement:"));
        wall->setVisible(false);
        require(QTest::qWaitFor([&]{return layout->count()==0&&slotItems().isEmpty();},5000));
        controller->insert("shuttingDown",true);
        object.reset();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        engine.collectGarbage();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        std::sort(latencies.begin(),latencies.end());
        qInfo()<<"Magazine 500 rows, settled jumps"<<latencies.size()
               <<"medianReadyMs"<<latencies.at(latencies.size()/2)
               <<"maxReadyMs"<<latencies.back()<<"maxSlots"<<maxSlots;
        require(maxSlots<70);
    }
    require(lifecycleWarnings.load(std::memory_order_relaxed)==0);
    qInstallMessageHandler(previousHandler);
    return 0;
}
