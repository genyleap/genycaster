#include "browser.hpp"
#include "browser_mobile_bridge.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QJniObject>
#include <QJsonDocument>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QStyleHints>
#include <QtTranslation>
#include <jni.h>
#include <utility>

namespace farcaster::platform::miniapp {
namespace {
constexpr auto javaClass = "com/genyleap/genycaster/MiniAppBrowser";
class AndroidBrowser;
QHash<jlong, QPointer<AndroidBrowser>> sessions;
jlong nextId = 0; // Accessed only on Qt GUI thread.

class AndroidBrowser final : public QObject, public Browser {
public:
    AndroidBrowser(const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
                   bool visible, BrowserRole role)
        : m_callbacks(std::move(callbacks)), m_id(++nextId) {
        sessions.insert(m_id, this);
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        const auto t = QJniObject::fromString(title.left(160));
        QUrl base = origin;
        if (role != BrowserRole::WalletApproval) {
            base.setPath(QString{}); base.setQuery(QString{}); base.setFragment(QString{}); base.setPort(-1);
        }
        const auto o = QJniObject::fromString(base.toString(QUrl::FullyEncoded));
        const auto script = QJniObject::fromString(QString::fromUtf8(GENY_MOBILE_BRIDGE_SCRIPT));
        const QString mode = QSettings{}.value(QStringLiteral("appearance/themeMode"),
                                               QStringLiteral("system")).toString();
        const bool dark = mode == QStringLiteral("dark")
            || (mode != QStringLiteral("light")
                && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
        const QJsonArray labels{qtTrId("genycaster.profile.back"), qtTrId("genycaster.feed_header.refresh"),
            qtTrId("genycaster.miniapp.minimize"), qtTrId("genycaster.shell.close"),
            qtTrId("genycaster.miniapp.share"), qtTrId("genycaster.miniapp.loading"), dark};
        const auto controls = QJniObject::fromString(QString::fromUtf8(QJsonDocument(labels).toJson(QJsonDocument::Compact)));
        QJniObject::callStaticMethod<void>(javaClass, "create",
            "(Landroid/content/Context;JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZI)V",
            activity.object(), m_id, t.object<jstring>(), o.object<jstring>(), script.object<jstring>(), controls.object<jstring>(),
            jboolean(visible), jint(role));
    }
    ~AndroidBrowser() override {
        sessions.remove(m_id);
        QJniObject::callStaticMethod<void>(javaClass, "destroy", "(J)V", m_id);
    }
    void load(const QUrl& url, const QUrl& referrer) override {
        auto a=QJniObject::fromString(url.toString(QUrl::FullyEncoded));
        auto b=QJniObject::fromString(referrer.toString(QUrl::FullyEncoded));
        QJniObject::callStaticMethod<void>(javaClass,"load","(JLjava/lang/String;Ljava/lang/String;)V",
            m_id,a.object<jstring>(),b.object<jstring>());
    }
    void status(const QString& message, bool failed) override {
        auto s=QJniObject::fromString(message);
        QJniObject::callStaticMethod<void>(javaClass,"status","(JLjava/lang/String;Z)V",m_id,s.object<jstring>(),jboolean(failed));
    }
    void ready(bool) override { flag("ready",true); }
    void setBackVisible(bool visible) override { flag("setBackVisible",visible); }
    void reply(const QJsonObject& value,const QString& document) override { emitEvent("__genyMiniAppReply",document,value); }
    void ethereumProviderEvent(const QString& event,const QJsonArray& params,const QString& document) override {
        emitEvent("__genyMiniAppEthProviderEvent",document,{{QStringLiteral("event"),event},{QStringLiteral("params"),params}});
    }
    void miniAppEvent(const QJsonObject& event,const QString& document) override { emitEvent("__genyMiniAppEvent",document,event); }
    void setVisible(bool visible) override { flag("setVisible",visible); }
    qlonglong processIdentifier() const noexcept override { return 0; }
    void dispatch(int event,const QString& payload,const QString& document) {
        switch(event) {
        case 0: if(m_callbacks.closed) m_callbacks.closed(); break;
        case 1: if(m_callbacks.retry) m_callbacks.retry(); break;
        case 2: if(m_callbacks.minimize) m_callbacks.minimize(); break;
        case 3: if(m_callbacks.share) m_callbacks.share(); break;
        case 4: if(m_callbacks.navigationStarted) m_callbacks.navigationStarted(); break;
        case 5: if(m_callbacks.failed) m_callbacks.failed(qtTrId(payload.toUtf8().constData())); break;
        case 6: {
            const auto json=QJsonDocument::fromJson(payload.toUtf8());
            if(json.isObject() && m_callbacks.message && !document.isEmpty() && document.size()<=64)
                m_callbacks.message(json.object(),document);
            break;
        }
        }
    }
private:
    void flag(const char* method,bool value) {
        QJniObject::callStaticMethod<void>(javaClass,method,"(JZ)V",m_id,jboolean(value));
    }
    void emitEvent(const char* name,const QString& document,const QJsonObject& data) {
        const auto args=QString::fromUtf8(QJsonDocument(QJsonArray{document,data}).toJson(QJsonDocument::Compact));
        const auto script=QJniObject::fromString(QStringLiteral("window.")+QString::fromLatin1(name)+QStringLiteral("?.(...")+args+QStringLiteral(")"));
        const auto token=QJniObject::fromString(document);
        QJniObject::callStaticMethod<void>(javaClass,"evaluate","(JLjava/lang/String;Ljava/lang/String;)V",
            m_id,script.object<jstring>(),token.object<jstring>());
    }
    BrowserCallbacks m_callbacks;
    const jlong m_id;
};
} // namespace

std::unique_ptr<Browser> createBrowser(const QString& title,const QUrl& origin,
    BrowserCallbacks callbacks,PresentationMode,bool,bool visible,BrowserRole role) {
    return std::make_unique<AndroidBrowser>(title,origin,std::move(callbacks),visible,role);
}
void dispatchAndroidBrowserEvent(jlong id,int event,QString payload,QString document) {
    if(auto* app=QCoreApplication::instance()) {
        QMetaObject::invokeMethod(app,[id,event,payload=std::move(payload),document=std::move(document)] {
            if(const auto browser=sessions.value(id)) browser->dispatch(event,payload,document);
        },Qt::QueuedConnection);
    }
}
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_genyleap_genycaster_MiniAppBrowser_nativeEvent(JNIEnv*,jclass,jlong id,jint event,jstring payload,jstring document) {
    farcaster::platform::miniapp::dispatchAndroidBrowserEvent(id,event,
        payload ? QJniObject(payload).toString() : QString{},
        document ? QJniObject(document).toString() : QString{});
}
