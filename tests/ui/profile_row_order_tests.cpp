#include "../../src/presentation/qt/profile_row_order.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QJSEngine>
#include <QJSValue>
#include <array>

using genycaster::presentation::detail::ProfileRowSnapshot;
namespace {
void require(const bool condition, const char* message)
{
    if (!condition) qFatal("%s", message);
}
class Ranker final : public QObject {
    Q_OBJECT
public:
    ProfileRowSnapshot snapshot;
    Q_INVOKABLE QVariantList order(const QString& token, bool popular,
                                  const QString& mode, const QString& metric) const
    {
        return snapshot.order(token, popular, mode, metric);
    }
};
}
int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    Ranker ranker;
    QJSEngine engine;
    QJSEngine::setObjectOwnership(&ranker, QJSEngine::CppOwnership);
    engine.globalObject().setProperty("ranker", engine.newQObject(&ranker));
    auto evaluate = [&](const QString& code) {
        const auto result=engine.evaluate(code);
        require(!result.isError(), qPrintable(result.toString()));
        return result;
    };
    evaluate(QStringLiteral(R"JS(
        function reference(popular,mode,metric) {
            const score=r=>metric==="likes"?r.likeCount:metric==="recasts"?r.recastCount:
                 metric==="replies"?r.replyCount:r.likeCount+2*r.recastCount+1.5*r.replyCount;
            const keys=Array.from(source,(row,index)=>({index,time:Number(row.createdAtMs||row.time||0),
                                                        score:popular?score(row):0}));
            keys.sort((a,b)=>(popular?(b.score-a.score||b.time-a.time):
                (a.time-b.time)*(mode==="oldest"?1:-1))||a.index-b.index);
            return keys.map(k=>k.index);
        }
        function verify(popular,mode,metric) {
            const actual=ranker.order(token,popular,mode,metric);
            return JSON.stringify(Array.from(actual))===JSON.stringify(reference(popular,mode,metric));
        }
    )JS"));
    for (const int count : {20,60,200,1000,5000}) {
        QVariantList rows;
        for (int i=0;i<count;++i)
            rows.append(QVariantMap{{"id",i},{"createdAtMs",(i*73)%1234},{"time",(i*11)%54},
                {"likeCount",i%17},{"recastCount",i%7},{"replyCount",i%5},
                {"text",QString(180,QLatin1Char('x'))},{"embeds",QVariantList{
                    QVariantMap{{"url","https://example.invalid/a"},{"width",640},{"height",480}}}}});
        const auto snapshot=ranker.snapshot.capture(rows);
        const auto token=snapshot.value("token").toString();
        require(ranker.snapshot.capture(rows).value("token").toString()==token,
                "Repeated reader invalidated the same snapshot");
        engine.globalObject().setProperty("source",engine.toScriptValue(snapshot.value("rows").toList()));
        engine.globalObject().setProperty("token",token);
        for (const auto& mode : {"oldest","newest"}) {
            for (const auto& metric : {"likes","recasts","replies","all"}) {
                for (const bool popular : {false,true}) {
                    const QString code=QStringLiteral("verify(%1,'%2','%3')")
                        .arg(popular ? QStringLiteral("true") : QStringLiteral("false"),
                             QString::fromLatin1(mode),QString::fromLatin1(metric));
                    require(evaluate(code).toBool(),"Native ordering differs from stable reference");
                }
            }
        }
        const auto baseline=ranker.snapshot.order(token,true,"newest","all");
        rows[0]=QVariantMap{{"id",-1}};
        require(ranker.snapshot.order(token,true,"newest","all")==baseline,
                "Native snapshot was mutated through source");
        const auto next=ranker.snapshot.capture(rows);
        require(ranker.snapshot.order(token,true,"newest","all").isEmpty(),"Old token accepted");
        ranker.snapshot.invalidate();
        require(ranker.snapshot.order(next.value("token").toString(),true,"newest","all").isEmpty(),
                "Invalidated snapshot accepted");
    }
    const auto odd=ranker.snapshot.capture({QVariantMap{{"createdAtMs","0x10"}}});
    require(ranker.snapshot.order(odd.value("token").toString(),false,"newest","all").isEmpty(),
            "Noncanonical number must use QML fallback");
    const auto zero=ranker.snapshot.capture({QVariantMap{{"createdAtMs","0"},{"time",99}},
                                            QVariantMap{{"createdAtMs",1}}});
    require(ranker.snapshot.order(zero.value("token").toString(),false,"newest","all")
            == QVariantList{1LL,0LL},"String zero is truthy, numeric zero is not");
    qInfo()<<"Native profile ranking: 80 parity scenarios, snapshot isolation and fallback passed";
    return 0;
}
#include "profile_row_order_tests.moc"
