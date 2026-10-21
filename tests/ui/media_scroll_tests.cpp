#include "appearance_controller.hpp"
#include "localization_controller.hpp"
#include "media_playback_coordinator.hpp"
#include "rendering_controller.hpp"
#include "rich_inline_text_item.hpp"
#include "scene_graph_superellipse_item.hpp"
#include "../../ui/mobile/feed_rows_model.hpp"

#include <QGuiApplication>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QJSValue>
#include <QMediaPlayer>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickAsyncImageProvider>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickStyle>
#include <QQuickTextureFactory>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QTest>

#include <algorithm>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <print>
#include <utility>
#include <vector>

namespace {
int failures{};

void expect(const bool condition, const char* message)
{
    if (!condition) {
        std::println("FAIL: {}", message);
        ++failures;
    }
}

// Qt owns every returned response and factory. The fixture controls completion
// independently of scrolling, without HTTP, disk-cache state, or decoder timing.
class ObservedTextureFactory final : public QQuickTextureFactory {
public:
    ObservedTextureFactory(const QImage& image, std::shared_ptr<std::atomic_int> uploads)
        : m_factory{QQuickTextureFactory::textureFactoryForImage(image)}, m_uploads{std::move(uploads)} {}

    QSGTexture* createTexture(QQuickWindow* window) const override
    {
        ++*m_uploads;
        return m_factory->createTexture(window);
    }
    QSize textureSize() const override { return m_factory->textureSize(); }
    int textureByteCount() const override { return m_factory->textureByteCount(); }
    QImage image() const override { return m_factory->image(); }

private:
    std::unique_ptr<QQuickTextureFactory> m_factory;
    std::shared_ptr<std::atomic_int> m_uploads;
};

class ControlledResponse final : public QQuickImageResponse {
public:
    ControlledResponse(const QImage& image, std::shared_ptr<std::atomic_int> uploads)
        : m_image{image}, m_uploads{std::move(uploads)} {}

    QQuickTextureFactory* textureFactory() const override
    {
        return new ObservedTextureFactory{m_image, m_uploads};
    }

    void complete() { emit finished(); }

private:
    QImage m_image{};
    std::shared_ptr<std::atomic_int> m_uploads;
};

class ControlledImages final : public QQuickAsyncImageProvider {
public:
    ControlledImages() : m_image{640, 360, QImage::Format_RGB32}
    {
        m_image.fill(QColor{QStringLiteral("#4287c7")});
    }

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize&) override
    {
        // The production provider uses a one-pixel sentinel for recoverable
        // failures, allowing policy/manual-load UI without a QML image warning.
        QImage pixels = id.startsWith(QStringLiteral("failure")) ? m_image.scaled(1, 1) : m_image;
        if (id == QStringLiteral("replacement-shadow")) {
            pixels.fill(QColor{QStringLiteral("#db442c")});
        }
        auto* response = new ControlledResponse{pixels, m_uploads};
        std::scoped_lock lock{m_mutex};
        m_pending.emplace_back(response);
        ++m_requestCount;
        return response;
    }

    int requestCount() const
    {
        std::scoped_lock lock{m_mutex};
        return m_requestCount;
    }

    int textureUploads() const { return m_uploads->load(); }

    void completePending()
    {
        std::vector<QPointer<ControlledResponse>> pending{};
        {
            std::scoped_lock lock{m_mutex};
            pending.swap(m_pending);
        }
        for (const auto& response : pending) {
            if (response) {
                response->complete();
            }
        }
    }

private:
    QImage m_image{};
    mutable std::mutex m_mutex{};
    std::vector<QPointer<ControlledResponse>> m_pending{};
    int m_requestCount{};
    std::shared_ptr<std::atomic_int> m_uploads{std::make_shared<std::atomic_int>(0)};
};

auto createInline(QQmlEngine& engine, const QByteArray& source) -> std::unique_ptr<QObject>
{
    QQmlComponent component{&engine};
    component.setData(source, QUrl{QStringLiteral("qrc:/media-scroll-fixture.qml")});
    expect(component.isReady(), qPrintable(component.errorString()));
    auto object = std::unique_ptr<QObject>{component.create()};
    expect(object != nullptr, qPrintable(component.errorString()));
    return object;
}

auto createSurface(QQmlEngine& engine, const QString& name, QQuickItem& parent,
                   const QVariantMap& properties = {}, QQmlContext* creationContext = nullptr)
    -> std::unique_ptr<QObject>
{
    QQmlComponent component{&engine, QUrl::fromLocalFile(
        QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/") + name + QStringLiteral(".qml"))};
    expect(component.isReady(), qPrintable(component.errorString()));
    auto initial = properties;
    initial.insert(QStringLiteral("parent"), QVariant::fromValue(&parent));
    initial.insert(QStringLiteral("width"), 480);
    initial.insert(QStringLiteral("height"), 280);
    auto object = std::unique_ptr<QObject>{component.createWithInitialProperties(initial, creationContext)};
    expect(object != nullptr, qPrintable(component.errorString()));
    return object;
}

bool flag(const QObject& object, const char* name) { return object.property(name).toBool(); }

void collectVisualItemsNamed(QQuickItem& root, const QString& name, std::vector<QQuickItem*>& result)
{
    for (auto* child : root.childItems()) {
        if (child->objectName() == name) {
            result.push_back(child);
        }
        collectVisualItemsNamed(*child, name, result);
    }
}

auto visualItemsNamed(QQuickItem& root, const QString& name) -> std::vector<QQuickItem*>
{
    std::vector<QQuickItem*> result{};
    collectVisualItemsNamed(root, name, result);
    return result;
}

int countVisualItemsWithClass(QQuickItem& root, const QString& classFragment)
{
    int count = 0;
    for (auto* child : root.childItems()) {
        if (QString::fromLatin1(child->metaObject()->className()).contains(classFragment)) {
            ++count;
        }
        count += countVisualItemsWithClass(*child, classFragment);
    }
    return count;
}

void polishTree(QQuickItem* item)
{
    item->ensurePolished();
    for (auto* child : item->childItems()) { polishTree(child); }
    item->ensurePolished();
}
}

int main(int argc, char** argv)
{
    QGuiApplication application{argc, argv};
    // The production Theme singleton uses QML Settings. Unlike the full
    // application, this standalone offscreen test must establish its own
    // organization/application identity before constructing any QML engine.
    application.setOrganizationName(QStringLiteral("GenycasterTests"));
    application.setApplicationName(QStringLiteral("media-scroll"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterType<RichInlineTextItem>("Farcaster.Client", 1, 0, "RichInlineTextItem");
    qmlRegisterType<FeedRowsModel>("Farcaster.Client", 1, 0, "FeedRowsModel");
    qmlRegisterType<SceneGraphSuperellipseItem>(
        "Farcaster.Client", 1, 0, "SceneGraphSuperellipse");
    // Source-loaded components normally get native types from their enclosing
    // application module. Supply that import in this standalone test engine.
    qmlRegisterModuleImport("QtQuick", QQmlModuleImportModuleAny, "Farcaster.Client", 1, 0);
    QTemporaryDir temporary;
    expect(temporary.isValid(), "isolated settings directory");
    QSettings settings{temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    AppearanceController appearance{settings};
    RenderingController rendering{settings};
    MediaPlaybackCoordinator playback{settings};
    QSignalSpy renderingPolicyChanges{&rendering, &RenderingController::renderingChanged};
    QSignalSpy adaptiveQualityChanges{&rendering, &RenderingController::adaptiveQualityChanged};
    expect(qFuzzyCompare(rendering.effectScale(), 0.88)
               && qFuzzyCompare(rendering.geometryQualityScale(), 1.0)
               && qFuzzyCompare(rendering.vectorRasterScale(), 1.0),
        "Adaptive keeps render-resource sizes stable independently of its frame-quality label");
    expect(rendering.setProfile(QStringLiteral("efficiency"))
               && renderingPolicyChanges.count() == 1 && adaptiveQualityChanges.count() == 1,
        "an explicit profile change publishes both resource and effective-quality policy");
    expect(rendering.setProfile(QStringLiteral("adaptive"))
               && renderingPolicyChanges.count() == 2 && adaptiveQualityChanges.count() == 2
               && qFuzzyCompare(rendering.effectScale(), 0.88),
        "returning to Adaptive restores its stable resource budget and notifications");
    // Context objects must outlive the engine's Theme/LocaleFormat singletons.
    std::unique_ptr<LocalizationController> localization;
    QQmlEngine engine;
    localization = std::make_unique<LocalizationController>(engine, settings);
    // Keep source preparation pending: exercising lazy player construction must
    // not contact an external video service or depend on media codecs.
    playback.setSourcePreparationBackend({.start = [](qulonglong, const QString&) {}});
    auto* context = engine.rootContext();
    context->setContextProperty(QStringLiteral("appearanceController"), &appearance);
    context->setContextProperty(QStringLiteral("localizationController"), localization.get());
    context->setContextProperty(QStringLiteral("renderingController"), &rendering);
    context->setContextProperty(QStringLiteral("mediaPlayback"), &playback);
    context->setContextProperty(QStringLiteral("platformTextFontFamily"), application.font().family());
    auto storage = createInline(engine, R"(
        import QtQml
        QtObject {
            property bool automaticAllowed: true
            signal policyChanged()
            function autoDownloadAllowed(category) { return automaticAllowed }
            function remoteImageSource(source, category, manual) { return source }
        }
    )");
    if (!storage) { return 1; }
    context->setContextProperty(QStringLiteral("dataStorage"), storage.get());
    auto previewServices = createInline(engine, R"(
        import QtQml
        QtObject {
            property int revision: 0
            property int accountFid: 0
            property bool isAuthenticated: false
            property var bookmarkedCastIds: []
            property string errorText: ""
            property string launchUrl: ""
            property var myApps: []
            property var explorerApps: []
            signal previewReady(string url, var preview)
            function isLaunchingUrl(url) { return false }
            function previewFor(url) { return ({}) }
            function requestPreview(url) {}
            function imageSourceForUrl(url) { return url }
            function contextualReactionIcon(text) { return "actions/heart" }
        }
    )");
    if (!previewServices) { return 1; }
    context->setContextProperty(QStringLiteral("linkPreviewResolver"), previewServices.get());
    context->setContextProperty(QStringLiteral("miniAppTaskManager"), previewServices.get());
    context->setContextProperty(QStringLiteral("miniAppHost"), previewServices.get());
    context->setContextProperty(QStringLiteral("miniAppCatalog"), previewServices.get());
    context->setContextProperty(QStringLiteral("presentationBridge"), previewServices.get());
    // The profile connection manager is another native context object in the
    // application. Its presence is required when source-loading ProfileSurface.
    auto openProofConnections = createInline(engine, R"(
        import QtQml
        QtObject {
            property var providers: []
            property var connections: []
            property bool authorized: false
            property bool authorizing: false
            property bool actionBusy: false
            property bool catalogFallback: false
            property string approvalUrl: ""
            property string consentUrl: ""
            property string error: ""
            function openConsent() {}
            function refresh() {}
            function authorizeAccount() {}
            function refreshConnections() {}
            function connectProvider(provider) {}
            function disconnectProvider(provider, subject) {}
            function stopAuthorization() {}
        }
    )");
    if (!openProofConnections) { return 1; }
    context->setContextProperty(QStringLiteral("openProofConnections"), openProofConnections.get());
    // A text-bearing CastItem now exercises the single shared caption Loader.
    // Supply an empty native-rich sidecar so the standalone media fixture
    // takes the regular plain-text rendering path.
    auto richComposer = createInline(engine, R"(
        import QtQml
        QtObject {
            property int revision: 0
            function richBlocksForCast(castId) { return [] }
        }
    )");
    if (!richComposer) { return 1; }
    context->setContextProperty(QStringLiteral("richComposer"), richComposer.get());
    const int themeType = qmlRegisterSingletonType(
        QUrl::fromLocalFile(QStringLiteral(MEDIA_THEME_FILE)), "Farcaster.Client", 1, 0, "Theme");
    const int localeType = qmlRegisterSingletonType(
        QUrl::fromLocalFile(QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/LocaleFormat.qml")),
        "Farcaster.Client", 1, 0, "LocaleFormat");
    const int tokenSemanticType = qmlRegisterSingletonType(
        QUrl::fromLocalFile(QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/TokenSemantic.qml")),
        "Farcaster.Client", 1, 0, "TokenSemantic");
    // Production registers these QML singletons through qt_add_qml_module.
    // This isolated Native test loads the same sources directly, so it must
    // register the same singleton contracts before constructing CastItem.
    const int castViewsType = qmlRegisterSingletonType(
        QUrl::fromLocalFile(QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/CastViews.qml")),
        "Farcaster.Client", 1, 0, "CastViews");
    const int castReactionsType = qmlRegisterSingletonType(
        QUrl::fromLocalFile(QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/CastReactions.qml")),
        "Farcaster.Client", 1, 0, "CastReactions");
    auto* theme = engine.singletonInstance<QObject*>(themeType);
    expect(theme != nullptr, "production theme singleton loads");
    context->setContextProperty(QStringLiteral("Theme"), theme);
    context->setContextProperty(QStringLiteral("LocaleFormat"), engine.singletonInstance<QObject*>(localeType));
    context->setContextProperty(QStringLiteral("TokenSemantic"), engine.singletonInstance<QObject*>(tokenSemanticType));
    auto* castViews = engine.singletonInstance<QObject*>(castViewsType);
    auto* castReactions = engine.singletonInstance<QObject*>(castReactionsType);
    expect(castViews != nullptr && castReactions != nullptr,
           "production view/reaction singletons are registered for Native UI tests");
    context->setContextProperty(QStringLiteral("CastViews"), castViews);
    context->setContextProperty(QStringLiteral("CastReactions"), castReactions);

    // Explicitly parse EmbedPreview with QQmlEngine rather than relying on
    // qmlcache compilation, which cannot catch missing attached imports.
    QQmlComponent embedDefinition{
        &engine, QUrl::fromLocalFile(
            QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/EmbedPreview.qml"))};
    expect(embedDefinition.isReady(), qPrintable(embedDefinition.errorString()));

    // Fast, genuine QQmlEngine load test for the entire same dependency
    // chain that has caused macOS's QQmlApplicationEngine white screen.
    // Keep it independent of the long rendering/scroll assertions below:
    // a media geometry regression must not mask an import/attached-property
    // error at application startup.
    if (qEnvironmentVariableIsSet("GENYCASTER_QML_IMPORT_SMOKE_ONLY")) {
        // Test that every provider vector is bundled and decodable by the
        // same Qt SVG image plugin used by the actual QML Image delegates.
        for (const auto& provider : {
                 "x", "github", "google", "linkedin", "telegram",
                 "farcaster", "ethereum-wallet"}) {
            const QString path =
                QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/")
                + QString::fromLatin1(provider) + QStringLiteral(".svg");
            QImageReader image{path};
            expect(image.canRead(), qPrintable(
                QStringLiteral("Provider SVG missing or undecodable: ") + path));
        }
        const QStringList components{
            QStringLiteral("EmbedPreview"),
            QStringLiteral("RichCastRenderer"),
            QStringLiteral("CastItem"),
            QStringLiteral("ConversationRow"),
            QStringLiteral("SemanticFeedItem"),
            QStringLiteral("FeedSurface"),
            QStringLiteral("AccountConnectionsPopup"),
            QStringLiteral("AppShell"),
        };
        for (const auto& name : components) {
            QQmlComponent definition{
                &engine, QUrl::fromLocalFile(
                    QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/") + name
                    + QStringLiteral(".qml"))};
            if (!definition.isReady()) {
                qWarning().noquote() << "Native QML import failure" << name
                                     << definition.errorString();
                return 1;
            }
        }
        return 0;
    }

    // Catch import/assignment failures in a transitive AppShell dependency
    // before shipping WASM. In particular GridLayout has rowSpacing and
    // columnSpacing but no general 'spacing' property: a bad assignment
    // prevents LocaleSettingsPopup and consequently AppShell from loading.
    QQmlComponent settingsDefinition{
        &engine, QUrl::fromLocalFile(
            QStringLiteral(MEDIA_COMPONENT_DIRECTORY "/LocaleSettingsPopup.qml"))};
    expect(settingsDefinition.isReady(),
           qPrintable(settingsDefinition.errorString()));

    // QQmlEngine owns this provider. Requests are held until explicitly released.
    auto* images = new ControlledImages;
    engine.addImageProvider(QStringLiteral("remote"), images);
    QQuickWindow window;
    const bool captureArtifacts = qEnvironmentVariableIsSet("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR");
    window.resize(captureArtifacts ? 1100 : 560, captureArtifacts ? 900 : 760);
    auto host = createInline(engine, R"(
        import QtQuick
        Item { width: 560; height: 760; property bool moving: false; property real contentY: 0 }
    )");
    if (!host) { return 1; }
    auto* scroll = qobject_cast<QQuickItem*>(host.get());
    scroll->setParentItem(window.contentItem());
    window.show();
    if (QGuiApplication::platformName() != QStringLiteral("minimal")
        && QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        window.raise();
        window.requestActivate();
    }
    expect(QTest::qWaitForWindowExposed(&window), "media test window is exposed before rendering assertions");
    std::atomic<quint64> observedRenderPasses{0};
    QObject::connect(
        &window,
        &QQuickWindow::afterRendering,
        &application,
        [&observedRenderPasses] { ++observedRenderPasses; },
        Qt::DirectConnection);
    rendering.attachWindow(&window);

    // A ListView/Loader can invalidate a delegate's creation context before its
    // QObject is deleted. Queued initialization must not invoke QML functions
    // in that dead context, even when the visual object still exists.
    QSignalSpy qmlWarnings{&engine, &QQmlEngine::warnings};

    // Exercise the real notification delegate with reply body, quote context,
    // and media under Qt 6.12. Structural string tests cannot detect a missing
    // QML property, a broken ListView model binding or an inert category tab.
    const QVariantMap replyNotice{
        {QStringLiteral("id"), QStringLiteral("notification-fixture-1")},
        {QStringLiteral("kind"), QStringLiteral("reply")},
        {QStringLiteral("actor"), QStringLiteral("alice")},
        {QStringLiteral("actorDisplayName"), QStringLiteral("Alice")},
        {QStringLiteral("actorFid"), 1},
        {QStringLiteral("observedAtMs"), qint64{1'782'000'000'000LL}},
        {QStringLiteral("castId"), QStringLiteral("0x") + QString(40, QLatin1Char('a'))},
        {QStringLiteral("castText"), QStringLiteral("A complete reply message from the fixture.")},
        {QStringLiteral("parentCastId"), QStringLiteral("0x") + QString(40, QLatin1Char('b'))},
        {QStringLiteral("parentText"), QStringLiteral("Quoted original cast")},
        // The geometry test must not fetch external images from an invalid host.
        {QStringLiteral("parentEmbedUrl"), QStringLiteral("https://example.invalid/quote")}
    };
    auto notificationFixture = createSurface(engine, QStringLiteral("NotificationSurface"), *scroll,
        {{QStringLiteral("authenticated"), true},
         {QStringLiteral("compact"), true},
         {QStringLiteral("rows"), QVariantList{replyNotice}}});
    if (!notificationFixture) { return 1; }
    auto* notificationRoot = qobject_cast<QQuickItem*>(notificationFixture.get());
    notificationRoot->setSize(QSizeF{390, 620});
    auto* notificationTitle = notificationRoot->findChild<QQuickItem*>(
        QStringLiteral("notificationTitleRow"));
    auto* notificationFilters = notificationRoot->findChild<QQuickItem*>(
        QStringLiteral("notificationFilterRow"));
    auto* notificationBody = notificationRoot->findChild<QQuickItem*>(
        QStringLiteral("notificationBody"));
    expect(notificationTitle && notificationFilters && notificationBody,
           "notification header, filters and single body are instantiated");
    if (!notificationTitle || !notificationFilters || !notificationBody) { return 1; }
    QTest::qWait(80);
    polishTree(notificationRoot);
    const auto relativeY = [notificationRoot](QQuickItem& item) {
        return item.mapToItem(notificationRoot, QPointF{0, 0}).y();
    };
    const auto titleY = relativeY(*notificationTitle);
    const auto filterY = relativeY(*notificationFilters);
    const auto bodyY = relativeY(*notificationBody);
    const auto assertStableNotificationLayout = [&] {
        polishTree(notificationRoot);
        QTest::qWait(35);
        expect(qAbs(relativeY(*notificationTitle) - titleY) < 2,
               "notification title never jumps vertically while loading or filtering");
        expect(qAbs(relativeY(*notificationFilters) - filterY) < 2,
               "Activity, Channels and Mentions tabs retain their y position");
        expect(qAbs(relativeY(*notificationBody) - bodyY) < 2,
               "notification empty, loading and populated states share one anchored body");
        expect(filterY >= titleY + notificationTitle->height() - 2,
               "notification category tabs stay immediately below the header");
        expect(bodyY >= filterY + notificationFilters->height() - 2,
               "notification state area always stays below the category tabs");
    };
    assertStableNotificationLayout();
    notificationFixture->setProperty("loadingNotifications", true);
    notificationFixture->setProperty("rows", QVariantList{});
    for (const auto& filter : {
             QStringLiteral("all"), QStringLiteral("channels"),
             QStringLiteral("mention")}) {
        notificationFixture->setProperty("filterKind", filter);
        assertStableNotificationLayout();
    }
    notificationFixture->setProperty("loadingNotifications", false);
    assertStableNotificationLayout();
    notificationFixture->setProperty("rows", QVariantList{replyNotice});
    notificationFixture->setProperty("filterKind", QStringLiteral("all"));
    assertStableNotificationLayout();
    expect(!flag(*notificationFixture, "showState"),
           "an actual reply row displays on the Activity notifications tab");
    notificationFixture->setProperty("filterKind", QStringLiteral("mention"));
    expect(flag(*notificationFixture, "showState"),
           "Mentions excludes replies without a literal @viewer reference");
    notificationFixture->setProperty("accountProfile", QVariantMap{
        {QStringLiteral("username"), QStringLiteral("viewer")}});
    QVariantMap mentionedReply = replyNotice;
    mentionedReply.insert(QStringLiteral("castText"), QStringLiteral("Hello @viewer!"));
    notificationFixture->setProperty("rows", QVariantList{mentionedReply});
    expect(!flag(*notificationFixture, "showState"),
           "Mentions displays replies that explicitly @mention the signed-in account");
    notificationFixture->setProperty("rows", QVariantList{replyNotice});
    notificationFixture->setProperty("filterKind", QStringLiteral("channels"));
    expect(flag(*notificationFixture, "showState"),
           "Channels does not invent a channel for unrelated replies");
    QVariantMap channelReply = replyNotice;
    channelReply.insert(QStringLiteral("castChannel"), QStringLiteral("design"));
    notificationFixture->setProperty("rows", QVariantList{channelReply});
    expect(!flag(*notificationFixture, "showState"),
           "Channels displays a provider-tagged notification");
    notificationFixture->setProperty("rows", QVariantList{replyNotice});
    notificationFixture->setProperty("filterKind", QStringLiteral("all"));
    expect(!flag(*notificationFixture, "showState"),
           "returning to Activity restores the previously loaded reply");
    expect(notificationFixture->findChild<QObject*>(QStringLiteral("pullToRefresh")) != nullptr,
           "notifications have a real shared pull-to-refresh gesture overlay");
    expect(notificationFixture->findChild<QObject*>(QStringLiteral("notificationList")) != nullptr,
           "empty notifications retain an interactive refresh viewport");

    // The iPhone notification regression was in the reaction summary:
    // stacked actor avatars escaped their allocated column, a colored
    // fill-height quote rail crossed the body, and a large link card
    // reflowed the feed. Exercise that QML delegate, not only replies.
    const QVariantMap reactedNotice{
        {QStringLiteral("id"), QStringLiteral("notification-reaction-fixture")},
        {QStringLiteral("kind"), QStringLiteral("reaction")},
        {QStringLiteral("actor"), QStringLiteral("alice")},
        {QStringLiteral("actorDisplayName"), QStringLiteral("Alice")},
        {QStringLiteral("actorFid"), 1},
        {QStringLiteral("aggregateCount"), 7},
        {QStringLiteral("observedAtMs"), qint64{1'782'000'000'000LL}},
        {QStringLiteral("castId"), QStringLiteral("0x") + QString(40, QLatin1Char('c'))},
        {QStringLiteral("castText"), QStringLiteral(
            "The page related to the token has been updated, as has its documentation.")},
        {QStringLiteral("castEmbedUrl"), QStringLiteral("https://example.com/token")},
        {QStringLiteral("actors"), QVariantList{
            QVariantMap{{QStringLiteral("fid"), 1},
                        {QStringLiteral("displayName"), QStringLiteral("Alice")}},
            QVariantMap{{QStringLiteral("fid"), 2},
                        {QStringLiteral("displayName"), QStringLiteral("Bob")}}}}
    };
    notificationFixture->setProperty("rows", QVariantList{reactedNotice});
    QTest::qWait(90);
    expect(!flag(*notificationFixture, "showState"),
           "aggregated reaction with a link displays without losing its row");
    expect(qmlWarnings.empty(),
           "reaction summary, bounded quote and compact external link instantiate without QML warnings");
    qmlWarnings.clear();
    notificationFixture.reset();
    images->completePending();
    expect(qmlWarnings.empty(), "notification row instantiates without Qt 6.12 QML warnings");
    qmlWarnings.clear();

    // Regression: a profile's transient empty metadata/rows during polling must
    // not reset an already-scrolled ListView or replace the loaded casts. The
    // reported WASM jump to the banner happened without a user route change.
    {
        QVariantList profileRows;
        for (int index = 0; index < 24; ++index) {
            profileRows.push_back(QVariantMap{
                {QStringLiteral("castId"), QStringLiteral("0xscroll-%1").arg(index)},
                {QStringLiteral("author"), QStringLiteral("reader")},
                {QStringLiteral("text"), QStringLiteral("Scroll fixture cast %1: ")
                    .arg(index) + QString(160, QLatin1Char('x'))},
                {QStringLiteral("createdAtMs"), 300000 - index * 1000},
            });
        }
        auto profile = createSurface(engine, QStringLiteral("ProfileSurface"), *scroll,
            {{QStringLiteral("detail"), QVariantMap{
                  {QStringLiteral("fid"), 44},
                  {QStringLiteral("username"), QStringLiteral("scroll-test")}}},
             {QStringLiteral("rows"), profileRows},
             {QStringLiteral("compact"), true}});
        if (!profile) return 1;
        auto* connections = profile->findChild<QObject*>(QStringLiteral("profileConnectionsButton"));
        expect(connections != nullptr, "owner has a discoverable Connections button near the avatar");
        if (connections) {
            profile->setProperty("accountFid", 44);
            profile->setProperty("accountIssuer", QStringLiteral("genycaster:device-local"));
            QCoreApplication::processEvents();
            expect(connections->property("visible").toBool(),
                "own profile connections remains visible after local Farcaster recovery");
            profile->setProperty("accountIssuer", QString{});
            QCoreApplication::processEvents();
            expect(connections->property("visible").toBool(),
                "own profile connections is not tied to a fragile issuer comparison");
            profile->setProperty("accountFid", 55);
            QCoreApplication::processEvents();
            expect(!connections->property("visible").toBool(),
                "another user's profile cannot show owner connection controls");
            profile->setProperty("accountFid", 0);
            QCoreApplication::processEvents();
            expect(!connections->property("visible").toBool(),
                "guest sessions cannot show owner connection controls");
        }
        auto* view = profile->findChild<QObject*>(QStringLiteral("profileList"));
        auto* model = profile->findChild<FeedRowsModel*>(QStringLiteral("profileRowsModel"));
        auto* topButton = profile->findChild<QObject*>(QStringLiteral("scrollToTopButton"));
        expect(view && model && topButton, "profile exposes its scroll viewport and return-to-top control");
        if (!view || !model || !topButton) return 1;
        expect(QTest::qWaitFor([&] { return model->rowCount() == profileRows.size(); }),
            "profile populates stable feed before scroll");
        expect(QMetaObject::invokeMethod(view, "positionViewAtIndex",
            Q_ARG(int, 12), Q_ARG(int, 0)), "scroll fixture moves into later profile casts");
        expect(QTest::qWaitFor([&] {
            return view->property("contentY").toDouble() > 550;
        }), "profile can scroll well beyond its header");
        // Exercise the native QML anchor logic with a real indexed delegate.
        expect(QMetaObject::invokeMethod(profile.get(), "captureViewportAnchor"),
            "capture a stable visible profile row at a deep scroll offset");
        const QString anchoredCast = profile->property("viewportAnchorKey").toString();
        expect(!anchoredCast.isEmpty(),
            "deep profile viewport anchor resolves a visible cast, not header coordinates");
        expect(topButton->property("visible").toBool(),
            "return-to-top affordance appears only after substantial scrolling");

        profile->setProperty("busy", true);
        profile->setProperty("rows", QVariantList{});
        QTest::qWait(100);
        expect(model->rowCount() == profileRows.size(),
            "temporary empty busy profile does not clear loaded casts");
        // As asynchronous CastItem heights settle, raw pixel offsets can
        // legitimately change by thousands while the same cast stays in view.
        // Assert the stable visible row, not a stale pixel coordinate.
        expect(QMetaObject::invokeMethod(profile.get(), "captureViewportAnchor"),
            "resample visible profile row after temporary empty response");
        // A still-incubating ListView can correct its estimates by several
        // delegate heights between two probes. The originally sampled row
        // need not remain the first visible row even with unchanged data.
        // The important invariants are no destructive model clear and no
        // teleport back to the header during a transient empty response.
        expect(!profile->property("viewportAnchorKey").toString().isEmpty(),
            "temporary empty profile response retains a measurable visible cast");
        expect(view->property("contentY").toDouble() - view->property("originY").toDouble() > 300,
            "temporary empty profile response does not teleport to the header");
        profile->setProperty("detail", QVariantMap{});
        QTest::qWait(50);
        expect(model->rowCount() == profileRows.size(),
            "temporary missing profile fid does not clear stable rows");
        profile->setProperty("detail", QVariantMap{
            {QStringLiteral("fid"), 44},
            {QStringLiteral("username"), QStringLiteral("scroll-test")}});
        profile->setProperty("rows", profileRows);
        profile->setProperty("busy", false);
        QTest::qWait(110);
        expect(view->property("contentY").toDouble() > 400,
            "metadata refresh does not teleport a deep profile scroll to the banner");
        // A stale anchor may refer to a delegate recycled hundreds of pixels
        // away. An asynchronous geometry event must never resurrect that old
        // row and teleport a profile reader back to the header.
        const double scrollBeforeStaleAnchor = view->property("contentY").toDouble();
        profile->setProperty("viewportAnchorKey", QStringLiteral("cast:0xscroll-0"));
        profile->setProperty("viewportAnchorOffset", 0.0);
        expect(QMetaObject::invokeMethod(profile.get(), "stabilizeViewportAfterGeometryChange"),
            "simulate a delayed profile geometry update with a stale row anchor");
        QTest::qWait(100);
        expect(view->property("contentY").toDouble() > 400
                   && qAbs(view->property("contentY").toDouble() - scrollBeforeStaleAnchor) < 200,
            "a recycled/stale profile anchor never scrolls to the beginning");
        QMetaObject::invokeMethod(profile.get(), "captureViewportAnchor");
        expect(QMetaObject::invokeMethod(view, "positionViewAtIndex",
            Q_ARG(int, 18), Q_ARG(int, 0)), "move deeper before testing the floating return control");
        expect(QTest::qWaitFor([&] { return topButton->property("visible").toBool(); }),
            "floating control becomes visible after a long profile scroll");
        const int rowCountBeforeReturn = model->rowCount();
        expect(QMetaObject::invokeMethod(topButton, "click"),
            "return-to-top button is actionable, not merely a decorative overlay");
        // Return-to-top must not animate contentY across dozens of recycled
        // delegates. That produced a several-second empty viewport in WASM.
        QCoreApplication::processEvents();
        expect(view->property("atYBeginning").toBool(),
            "deep return-to-top positions the real header in one event turn");
        expect(!topButton->property("visible").toBool(),
            "return-to-top control hides promptly at the actual beginning");
        expect(model->rowCount() == rowCountBeforeReturn,
            "deep return-to-top preserves loaded profile casts");
        auto* returnedHeader = view->property("headerItem").value<QQuickItem*>();
        expect(returnedHeader != nullptr && returnedHeader->height() > 0,
            "return-to-top restores a populated profile header, not an empty viewport");
        QTest::qWait(100);
        expect(view->property("atYBeginning").toBool(),
            "asynchronous reflow does not move the profile away from the returned header");
        // Switching to an actually different identity must still replace rows.
        profile->setProperty("detail", QVariantMap{
            {QStringLiteral("fid"), 45},
            {QStringLiteral("username"), QStringLiteral("other-person")}});
        expect(model->rowCount() == 0, "real profile identity change invalidates stale casts");
        profile.reset();
    }

    // Regression: a notification/profile route can be destroyed in the turn
    // where stable-list and geometry updates schedule viewport restoration.
    // The former unowned Qt.callLater path invoked a method on a null QML
    // root after the enclosing Loader was released.
    for (int iteration = 0; iteration < 6; ++iteration) {
        auto notices = createSurface(engine, QStringLiteral("NotificationSurface"), *scroll,
            {{QStringLiteral("authenticated"), true},
             {QStringLiteral("compact"), true},
             {QStringLiteral("rows"), QVariantList{replyNotice}}});
        if (!notices) return 1;
        notices->setProperty("rows", QVariantList{replyNotice, reactedNotice});
        notices->setProperty("filterKind", QStringLiteral("channels"));
        notices->setProperty("rows", QVariantList{reactedNotice});
        notices.reset();
        QCoreApplication::processEvents();

        auto profile = createSurface(engine, QStringLiteral("ProfileSurface"), *scroll,
            {{QStringLiteral("detail"), QVariantMap{
                  {QStringLiteral("fid"), 44},
                  {QStringLiteral("username"), QStringLiteral("profile-test")}}},
             {QStringLiteral("rows"), QVariantList{QVariantMap{
                  {QStringLiteral("castId"), QStringLiteral("0xviewport")},
                  {QStringLiteral("text"), QStringLiteral("Cached local cast")},
                  {QStringLiteral("createdAtMs"), 1000},
             }}}});
        if (!profile) return 1;
        auto* profileModel = profile->findChild<FeedRowsModel*>(
            QStringLiteral("profileRowsModel"));
        expect(profileModel != nullptr,
            "profile feed uses the same native stable-ID Qt model as Explorer");
        if (profileModel) {
            expect(QTest::qWaitFor([&] { return profileModel->rowCount() == 1; }),
                "initial cached profile row is delivered to native model");
            QSignalSpy changes{profileModel, &QAbstractItemModel::dataChanged};
            profile->setProperty("rows", QVariantList{QVariantMap{
                {QStringLiteral("castId"), QStringLiteral("0xviewport")},
                {QStringLiteral("text"), QStringLiteral("Cached local cast")},
                {QStringLiteral("createdAtMs"), 1000},
                {QStringLiteral("likedByMe"), true},
            }});
            expect(QTest::qWaitFor([&] { return changes.count() > 0; }),
                "hydrated viewer evidence updates one native profile row role");
            expect(profileModel->rowCount() == 1,
                "profile reaction update retains one row and stable geometry");
        }
        profile->setProperty("profileSortMode", QStringLiteral("oldest"));
        profile->setProperty("rows", QVariantList{});
        profile->setProperty("rows", QVariantList{QVariantMap{
             {QStringLiteral("castId"), QStringLiteral("0xviewport-2")},
             {QStringLiteral("createdAtMs"), 2000},
        }});
        if (profileModel) {
            expect(QTest::qWaitFor([&] {
                return profileModel->rowCount() == 1
                    && profileModel->get(0).value(QStringLiteral("stableKey"))
                        == QStringLiteral("cast:0xviewport-2");
            }), "replacing profile rows updates the same native model");
            const QVariant cachedPayloads = profile->property("stablePayloads");
            const QVariantMap payloads = cachedPayloads.metaType() == QMetaType::fromType<QJSValue>()
                ? cachedPayloads.value<QJSValue>().toVariant().toMap()
                : cachedPayloads.toMap();
            expect(payloads.size() == 1
                && payloads.contains(QStringLiteral("cast:0xviewport-2"))
                && !payloads.contains(QStringLiteral("cast:0xviewport")),
                "profile switch releases obsolete cast payloads but retains current cast");
        }
        profile.reset();
        QCoreApplication::processEvents();

        // The feed formerly scheduled multiple global Qt.callLater closures
        // while its route, semantic projection and cached rows were changing.
        // Unloading the surface in the same event turn must cancel them.
        auto feedRoute = createSurface(engine, QStringLiteral("FeedSurface"), *scroll,
            {{QStringLiteral("rows"), QVariantList{QVariantMap{
                 {QStringLiteral("castId"), QStringLiteral("0xrapid-a")},
                 {QStringLiteral("text"), QStringLiteral("Cached cast")},
                 {QStringLiteral("createdAtMs"), 1000},
             }}},
             {QStringLiteral("presentationMode"), QStringLiteral("for-you")},
             {QStringLiteral("runtimeReady"), true},
             {QStringLiteral("feedStarted"), true},
             {QStringLiteral("compact"), true}});
        if (!feedRoute) return 1;
        feedRoute->setProperty("rows", QVariantList{
            QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xrapid-a")},
                        {QStringLiteral("text"), QStringLiteral("Changed cache")},
                        {QStringLiteral("createdAtMs"), 1000}},
            QVariantMap{{QStringLiteral("castId"), QStringLiteral("0xrapid-b")},
                        {QStringLiteral("text"), QStringLiteral("Recent cast")},
                        {QStringLiteral("createdAtMs"), 2000}},
        });
        feedRoute->setProperty("presentationMode", QStringLiteral("visuals"));
        feedRoute->setProperty("presentationMode", QStringLiteral("for-you"));
        feedRoute->setProperty("presentationActive", false);
        feedRoute->setProperty("presentationActive", true);
        feedRoute.reset();
        QCoreApplication::processEvents();
    }
    expect(qmlWarnings.empty(),
        "destroyed notification/profile/feed surfaces never evaluate stale deferred callbacks");
    qmlWarnings.clear();

    for (const QString& surface : {QStringLiteral("SuperellipseImage"),
                                   QStringLiteral("AmbientMediaBackdrop")}) {
        auto creationContext = std::make_unique<QQmlContext>(context);
        auto discarded = createSurface(engine, surface, *scroll, {}, creationContext.get());
        if (!discarded) { return 1; }
        creationContext.reset();
        QTest::qWait(80);
        expect(qmlWarnings.empty(), "discarded media initialization cannot evaluate an invalid context");
        qmlWarnings.clear();
    }

    auto image = createSurface(engine, QStringLiteral("SuperellipseImage"), *scroll,
        {{QStringLiteral("source"), QUrl{QStringLiteral("image://remote/initial")}}});
    if (!image) { return 1; }
    auto* maskLoader = image->findChild<QObject*>(QStringLiteral("mediaMaskLoader"));
    auto* loadingIndicator = image->findChild<QObject*>(QStringLiteral("mediaLoadingIndicator"));
    auto* loadingSpinner = image->findChild<QObject*>(QStringLiteral("mediaLoadingSpinner"));
    auto* loadingFallback = image->findChild<QObject*>(QStringLiteral("mediaLoadingFallbackIcon"));
    expect(maskLoader && !flag(*maskLoader, "active") && !maskLoader->property("item").value<QObject*>(),
        "pending media does not construct its hidden mask geometry");
    expect(loadingIndicator && loadingSpinner && loadingFallback,
        "media loading placeholder exposes one stable visual state");
    expect(flag(*loadingIndicator, "visible") && flag(*loadingSpinner, "visible")
               && !flag(*loadingFallback, "visible"),
        "large pending media starts with a spinner instead of an image glyph");
    expect(QTest::qWaitFor([&] { return images->requestCount() == 1; }), "initial async request starts");
    const auto originalSize = qobject_cast<QQuickItem*>(image.get())->size();
    host->setProperty("moving", true);
    expect(flag(*image, "loading") && flag(*loadingIndicator, "visible")
               && flag(*loadingSpinner, "visible") && !flag(*loadingFallback, "visible"),
        "scroll motion keeps the same media spinner visible");
    expect(!flag(*loadingSpinner, "animationRequested"),
        "pending media freezes indeterminate animation during scroll motion");
    const int uploadsBeforeCompletion = images->textureUploads();
    images->completePending();
    expect(QTest::qWaitFor([&] { return image->property("status").toInt() == 0; }),
        "motion detaches the QML image response before texture preparation");
    expect(!flag(*image, "ready"), "detached completion cannot admit a new texture while scrolling");
    expect(!flag(*image, "maskActive"), "detached completion cannot allocate a mask layer while scrolling");
    expect(flag(*image, "loading"), "placeholder persists until presentation admission");
    QTest::qWait(80);
    expect(images->textureUploads() == uploadsBeforeCompletion,
        "detached media does not upload a GPU texture during motion");
    expect(images->requestCount() == 1,
        "scroll detachment creates no replacement QML request during motion");
    expect(qobject_cast<QQuickItem*>(image.get())->size() == originalSize,
        "scroll detachment preserves reserved image geometry");
    host->setProperty("moving", false);
    expect(flag(*image, "loading") && flag(*loadingIndicator, "visible")
               && flag(*loadingSpinner, "visible") && !flag(*loadingFallback, "visible"),
        "post-fling admission delay cannot blank or replace the media spinner");
    expect(flag(*loadingSpinner, "running"),
        "spinner animation resumes after scrolling settles");
    expect(QTest::qWaitFor([&] { return images->requestCount() == 2; }),
        "settling reattaches the QML image consumer");
    images->completePending();
    expect(QTest::qWaitFor([&] { return flag(*image, "ready"); }), "settling admits the reattached image");
    expect(maskLoader && flag(*maskLoader, "active") && maskLoader->property("item").value<QObject*>(),
        "ready media constructs its mask only after scrolling settles");
    expect(countVisualItemsWithClass(*qobject_cast<QQuickItem*>(image.get()), QStringLiteral("MultiEffect")) == 1,
        "a clipped single image constructs only its admitted mask effect");
    if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        expect(QTest::qWaitFor([&] { return images->textureUploads() > uploadsBeforeCompletion; }),
            "settling uploads the real image through the GPU renderer");
    }
    host->setProperty("moving", true);
    expect(flag(*image, "ready") && flag(*image, "maskActive"),
        "an already presented image and mask persist across the next fling");
    image->setProperty("source", QUrl{QStringLiteral("image://remote/recycled")});
    expect(!flag(*image, "ready"), "recycled delegates clear previous artwork immediately");
    QTest::qWait(100);
    expect(images->requestCount() == 2, "recycled delegate defers its next image request during scrolling");
    host->setProperty("moving", false);
    expect(QTest::qWaitFor([&] { return images->requestCount() == 3; }), "recycled image starts after settling");
    images->completePending();
    expect(QTest::qWaitFor([&] { return flag(*image, "ready"); }), "replacement image is eventually visible");

    // Provider failures already include transport retry and URL backoff. A
    // batch of failed thumbnails must become quiescent, not create a new wave
    // of QML requests and placeholder transitions every few hundred ms.
    std::vector<std::unique_ptr<QObject>> failedImages;
    const int requestsBeforeFailures = images->requestCount();
    for (int index = 0; index < 12; ++index) {
        failedImages.push_back(createSurface(engine, QStringLiteral("SuperellipseImage"), *scroll,
            {{QStringLiteral("source"), QUrl{QStringLiteral("image://remote/failure-batch-%1").arg(index)}}}));
        if (!failedImages.back()) { return 1; }
    }
    expect(QTest::qWaitFor([&] { return images->requestCount() == requestsBeforeFailures + 12; }),
        "failed-thumbnail batch starts one request per source");
    images->completePending();
    expect(QTest::qWaitFor([&] {
        return std::ranges::all_of(failedImages, [](const auto& item) { return flag(*item, "loadFailed"); });
    }), "all failed thumbnails reach a terminal placeholder");
    std::vector<std::unique_ptr<QSignalSpy>> failureLoadingChanges;
    for (const auto& item : failedImages) {
        failureLoadingChanges.push_back(std::make_unique<QSignalSpy>(item.get(), SIGNAL(loadingChanged())));
    }
    const int terminalRequestCount = images->requestCount();
    for (int cycle = 0; cycle < 4; ++cycle) {
        host->setProperty("moving", true);
        QTest::mouseMove(&window, QPoint{520, 300});
        QTest::qWait(40);
        host->setProperty("moving", false);
        QTest::mouseMove(&window, QPoint{240, 140});
        QTest::qWait(320);
        images->completePending();
    }
    expect(images->requestCount() == terminalRequestCount,
        "failed thumbnails do not retry from QML during repeated scroll and idle cycles");
    expect(std::ranges::all_of(failureLoadingChanges, [](const auto& changes) { return changes->empty(); }),
        "terminal image failures never flash back to loading during provider backoff");
    failedImages.clear();

    storage->setProperty("automaticAllowed", false);
    const int requestsBeforeManual = images->requestCount();
    auto manualImage = createSurface(engine, QStringLiteral("SuperellipseImage"), *scroll,
        {{QStringLiteral("source"), QUrl{QStringLiteral("image://remote/failure-manual")}}});
    if (!manualImage) { return 1; }
    auto* manualLoader = manualImage->findChild<QObject*>(QStringLiteral("mediaManualLoadLoader"));
    expect(manualLoader && !flag(*manualLoader, "active"), "manual download controls are absent before a policy cache miss");
    expect(QTest::qWaitFor([&] { return images->requestCount() > requestsBeforeManual; }), "manual-policy image checks the cache");
    images->completePending();
    expect(QTest::qWaitFor([&] { return flag(*manualImage, "manualLoadRequired"); }), "cache miss exposes manual download");
    expect(manualLoader && flag(*manualLoader, "active") && manualLoader->property("item").value<QObject*>(),
        "manual download creates its controls on demand");
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
        qobject_cast<QQuickItem*>(manualImage.get())->mapToScene(QPointF{240, 140}).toPoint());
    expect(flag(*manualImage, "manualLoadRequested"), "lazy manual download controls accept a tap");
    storage->setProperty("automaticAllowed", true);
    manualImage.reset();

    auto backdrop = createSurface(engine, QStringLiteral("AmbientMediaBackdrop"), *scroll,
        {{QStringLiteral("sourceItem"), image->property("renderItem")}, {QStringLiteral("active"), false}});
    if (!backdrop) { return 1; }
    auto* ambientLoader = backdrop->findChild<QObject*>(QStringLiteral("ambientEffectLoader"));
    expect(ambientLoader != nullptr, "ambient media exposes its lazy effect boundary");
    expect(QTest::qWaitFor([&] { return flag(*backdrop, "componentReady"); }),
        "ambient media resolves its owning presentation context");
    backdrop->setProperty("active", true);
    QTest::qWait(80);
    expect(flag(*backdrop, "scrollableContext") && !flag(*backdrop, "effectAllowed")
               && !flag(*backdrop, "effectActive"),
        "scrollable media permanently omits the decorative ambient effect");
    expect(ambientLoader && !flag(*ambientLoader, "active")
               && !ambientLoader->property("item").value<QObject*>()
               && !backdrop->findChild<QObject*>(QStringLiteral("ambientMediaCapture")),
        "scrollable media constructs no blur layer or capture texture");

    // Explicit non-deferred use keeps the component available to static media
    // surfaces and exercises its one-shot frozen snapshot contract.
    backdrop->setProperty("deferWhileScrolling", false);
    expect(QTest::qWaitFor([&] { return flag(*backdrop, "effectActive"); }),
        "an explicitly static ambient surface admits its effect");
    auto* ambientCapture = backdrop->findChild<QObject*>(QStringLiteral("ambientMediaCapture"));
    expect(ambientCapture != nullptr, "static ambient media creates its texture capture lazily");
    if (!ambientCapture) { return 1; }
    QSignalSpy captureLiveChanges{ambientCapture, SIGNAL(liveChanged())};
    QSignalSpy captureCompletions{ambientCapture, SIGNAL(scheduledUpdateCompleted())};
    // A desktop test can be occluded while its QML event loop keeps running.
    // Drive GPU frames explicitly so snapshot assertions do not depend on the
    // window manager continuing to present this test window in the foreground.
    const auto waitForCapture = [&] {
        return QTest::qWaitFor([&] {
            window.grabWindow();
            return !captureCompletions.empty();
        });
    };
    QCoreApplication::processEvents();
    host->setProperty("moving", true);
    expect(flag(*backdrop, "effectActive") && !flag(*backdrop, "captureLive"),
        "explicit static ambient effect keeps a frozen capture during movement");
    host->setProperty("moving", false);
    QTest::qWait(100);
    expect(!flag(*ambientCapture, "live"), "static image shadows never enable live offscreen recapture at rest");
    if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        expect(waitForCapture(),
            "admitted image shadow completes its initial GPU snapshot");
    }
    captureLiveChanges.clear();
    captureCompletions.clear();
    const int requestsBeforeHover = images->requestCount();
    for (int cycle = 0; cycle < 4; ++cycle) {
        host->setProperty("moving", true);
        QTest::mouseMove(&window, QPoint{520, 300});
        QTest::qWait(30);
        host->setProperty("moving", false);
        QTest::mouseMove(&window, QPoint{240, 140});
        QTest::qWait(80);
        if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
            window.grabWindow();
            QCoreApplication::processEvents();
        }
    }
    expect(captureLiveChanges.empty() && captureCompletions.empty(),
        "hover and scroll settling reuse the static shadow without another capture");
    expect(images->requestCount() == requestsBeforeHover,
        "hovering a presented image never restarts provider work");
    if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        auto* captureItem = qobject_cast<QQuickItem*>(ambientCapture);
        auto blueSnapshot = captureItem->grabToImage(QSize{80, 80});
        expect(blueSnapshot && QTest::qWaitFor([&] {
            window.grabWindow();
            return !blueSnapshot->image().isNull();
        }),
            "static shadow capture is readable on the GPU");
        if (blueSnapshot && !blueSnapshot->image().isNull()) {
            const auto center = blueSnapshot->image().pixelColor(40, 40);
            expect(center.alpha() > 0 && center.blue() > center.red(),
                "initial shadow snapshot contains the image pixels rather than a blank texture");
        }
        backdrop->setProperty("active", false);
        image->setProperty("source", QUrl{QStringLiteral("image://remote/replacement-shadow")});
        expect(QTest::qWaitFor([&] { return images->requestCount() > requestsBeforeHover; }),
            "replacement shadow source starts");
        images->completePending();
        expect(QTest::qWaitFor([&] {
            window.update();
            window.grabWindow();
            QCoreApplication::processEvents();
            return flag(*image, "ready");
        }), "replacement shadow pixels become ready");
        backdrop->setProperty("active", true);
        expect(QTest::qWaitFor([&] {
            return backdrop->findChild<QObject*>(QStringLiteral("ambientMediaCapture")) != nullptr;
        }), "replacement shadow recreates its lazy capture");
        auto* replacementCapture = backdrop->findChild<QObject*>(QStringLiteral("ambientMediaCapture"));
        if (!replacementCapture) { return 1; }
        QSignalSpy replacementCompletions{replacementCapture, SIGNAL(scheduledUpdateCompleted())};
        const auto waitForReplacementCapture = [&] {
            return QTest::qWaitFor([&] {
                window.grabWindow();
                return !replacementCompletions.empty();
            });
        };
        expect(waitForReplacementCapture(),
            "source replacement refreshes the frozen shadow once");
        auto* replacementCaptureItem = qobject_cast<QQuickItem*>(replacementCapture);
        auto redSnapshot = replacementCaptureItem->grabToImage(QSize{80, 80});
        expect(redSnapshot && QTest::qWaitFor([&] {
            window.grabWindow();
            return !redSnapshot->image().isNull();
        }),
            "replacement shadow snapshot is readable on the GPU");
        if (redSnapshot && !redSnapshot->image().isNull()) {
            const auto center = redSnapshot->image().pixelColor(40, 40);
            expect(center.alpha() > 0 && center.red() > center.blue(),
                "recycled media cannot retain the previous image's shadow pixels");
        }
        replacementCompletions.clear();
        qobject_cast<QQuickItem*>(image.get())->setWidth(460);
        expect(waitForReplacementCapture(),
            "source geometry changes refresh the frozen shadow");
        qobject_cast<QQuickItem*>(image.get())->setWidth(480);
    }

    const int requestsBeforeGallery = images->requestCount();
    auto gallery = createSurface(engine, QStringLiteral("GalleryLayout"), *scroll,
        {{QStringLiteral("embeds"), QVariantList{QVariantMap{
            {QStringLiteral("imageSource"), QStringLiteral("image://remote/gallery")}}}}});
    if (!gallery) { return 1; }
    const auto galleryHeight = gallery->property("implicitHeight").toDouble();
    expect(QTest::qWaitFor([&] { return images->requestCount() > requestsBeforeGallery; }), "gallery requests actual shared image surface");
    images->completePending();
    QTest::qWait(100);
    expect(gallery->property("implicitHeight").toDouble() == galleryHeight,
        "decoded aspect ratio cannot reflow the feed gallery");
    auto* galleryItem = qobject_cast<QQuickItem*>(gallery.get());
    const auto singleGalleryPreviews = visualItemsNamed(*galleryItem, QStringLiteral("galleryPreview"));
    expect(singleGalleryPreviews.size() == 1 && !flag(*singleGalleryPreviews.front(), "clipToSuperellipse"),
        "single-image feed artwork stays on the direct texture path without an offscreen mask");
    const auto singleMaskLoaders = visualItemsNamed(*galleryItem, QStringLiteral("mediaMaskLoader"));
    expect(singleMaskLoaders.size() == 1 && !flag(*singleMaskLoaders.front(), "active")
               && !singleMaskLoaders.front()->property("item").value<QObject*>(),
        "single-image gallery does not allocate a mask item or MultiEffect");
    expect(countVisualItemsWithClass(*galleryItem, QStringLiteral("MultiEffect")) == 0,
        "single-image gallery constructs no hidden MultiEffect object");

    // The production failure case is a cast with adjacent images. This controlled
    // provider deliberately releases both responses in one turn to prove that
    // the QML direct path constructs no per-tile offscreen effects and that the
    // render-generation handshake separates their visibility across renders.
    const int requestsBeforeMultiGallery = images->requestCount();
    host->setProperty("moving", true);
    auto multiGallery = createSurface(engine, QStringLiteral("GalleryLayout"), *scroll,
        {{QStringLiteral("embeds"), QVariantList{
            QVariantMap{{QStringLiteral("imageSource"), QStringLiteral("image://remote/gallery-left")}},
            QVariantMap{{QStringLiteral("imageSource"), QStringLiteral("image://remote/gallery-right")}}
        }}});
    if (!multiGallery) { return 1; }
    auto* multiGalleryItem = qobject_cast<QQuickItem*>(multiGallery.get());
    expect(QTest::qWaitFor([&] {
        return visualItemsNamed(*multiGalleryItem, QStringLiteral("galleryTileLoader")).size() == 2;
    }), "multi-image gallery reserves both tile slots while moving");
    QTest::qWait(140);
    expect(visualItemsNamed(*multiGalleryItem, QStringLiteral("galleryPreview")).empty()
               && images->requestCount() == requestsBeforeMultiGallery,
        "new gallery rows construct no heavy tile subtree or image request during scrolling");
    host->setProperty("moving", false);
    expect(QTest::qWaitFor([&] {
        return visualItemsNamed(*multiGalleryItem, QStringLiteral("galleryPreview")).size() == 2;
    }), "multi-image gallery admits both heavy tile surfaces after settling");

    const auto tileLoaders = visualItemsNamed(*multiGalleryItem, QStringLiteral("galleryTileLoader"));
    expect(std::ranges::all_of(tileLoaders, [](const QObject* loader) {
        return loader->property("asynchronous").toBool();
    }), "multi-image gallery constructs each heavy tile asynchronously");

    const auto galleryPreviews = visualItemsNamed(*multiGalleryItem, QStringLiteral("galleryPreview"));
    std::vector<quint64> galleryAdmissionSequences{};
    std::vector<quint64> galleryAdmissionRenderPasses{};
    const auto galleryAdmissionConnection = QObject::connect(
        &rendering,
        &RenderingController::mediaPresentationGranted,
        &application,
        [&galleryPreviews, &galleryAdmissionSequences, &galleryAdmissionRenderPasses,
         &observedRenderPasses](QObject* requester, quint64, quint64 presentationSequence) {
            if (std::ranges::any_of(galleryPreviews, [requester](const QObject* preview) {
                    return preview == requester;
                })) {
                galleryAdmissionSequences.push_back(presentationSequence);
                galleryAdmissionRenderPasses.push_back(observedRenderPasses.load());
            }
        });
    int minimumLoadDelay = std::numeric_limits<int>::max();
    int maximumLoadDelay = 0;
    for (const auto* preview : galleryPreviews) {
        minimumLoadDelay = std::min(minimumLoadDelay, preview->property("loadDelayMs").toInt());
        maximumLoadDelay = std::max(maximumLoadDelay, preview->property("loadDelayMs").toInt());
        expect(!flag(*preview, "clipToSuperellipse"),
            "multi-image tile stays on the direct image texture path");
    }
    expect(minimumLoadDelay == 0 && maximumLoadDelay >= 48,
        "adjacent image requests are admitted on separate frame budgets");

    expect(QTest::qWaitFor([&] {
        return images->requestCount() >= requestsBeforeMultiGallery + 2;
    }), "multi-image gallery eventually starts every staggered image request");
    images->completePending();
    expect(QTest::qWaitFor([&] {
        window.update();
        window.grabWindow();
        QCoreApplication::processEvents();
        return std::ranges::all_of(galleryPreviews, [](const QObject* preview) {
            return flag(*preview, "ready");
        });
    }), "controlled adjacent-image completions remain renderable on the direct path");
    expect(galleryAdmissionSequences.size() == 2
               && galleryAdmissionSequences[0] != galleryAdmissionSequences[1]
               && galleryAdmissionRenderPasses.size() == 2
               && galleryAdmissionRenderPasses[1] > galleryAdmissionRenderPasses[0],
        "adjacent large images require distinct render-synchronized presentation grants");
    QObject::disconnect(galleryAdmissionConnection);

    const auto multiMaskLoaders = visualItemsNamed(*multiGalleryItem, QStringLiteral("mediaMaskLoader"));
    expect(multiMaskLoaders.size() == 2, "each gallery image retains the shared mask API");
    expect(std::ranges::all_of(multiMaskLoaders, [](const QObject* mask) {
        return !flag(*mask, "active") && !mask->property("item").value<QObject*>();
    }), "adjacent images allocate no per-tile mask layers or MultiEffects");
    expect(countVisualItemsWithClass(*multiGalleryItem, QStringLiteral("MultiEffect")) == 0,
        "multi-image gallery constructs no hidden MultiEffect objects");

    if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")
        && window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        const QDir artifacts{qEnvironmentVariable("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")};
        expect(artifacts.mkpath(QStringLiteral(".")), "visual evidence directory exists");
        std::vector<std::pair<QQuickItem*, bool>> siblingVisibility{};
        for (auto* sibling : scroll->childItems()) {
            siblingVisibility.emplace_back(sibling, sibling->isVisible());
            sibling->setVisible(sibling == multiGalleryItem);
        }
        multiGalleryItem->setPosition(QPointF{20, 24});
        multiGalleryItem->setWidth(window.width() - 40);
        multiGalleryItem->setHeight(multiGalleryItem->implicitHeight());
        expect(QTest::qWaitFor([&] {
            images->completePending();
            return std::ranges::all_of(galleryPreviews, [](const QObject* preview) {
                return flag(*preview, "ready");
            });
        }), "resized multi-image gallery finishes before visual capture");
        QTest::qWait(80);
        expect(window.grabWindow().save(artifacts.filePath(QStringLiteral("multi-gallery-direct-path.png"))),
            "multi-image direct rendering screenshot saved");
        multiGalleryItem->setPosition(QPointF{0, 0});
        multiGalleryItem->setSize(QSizeF{480, 280});
        expect(QTest::qWaitFor([&] {
            images->completePending();
            return std::ranges::all_of(galleryPreviews, [](const QObject* preview) {
                return flag(*preview, "ready");
            });
        }), "multi-image gallery returns to interaction-test geometry");
        for (const auto& [sibling, wasVisible] : siblingVisibility) {
            sibling->setVisible(wasVisible);
        }
    }

    // A cast that combines visual media with a supporting external link is the
    // most expensive common feed shape. Entering it during kinetic motion must
    // reserve geometry without constructing the LinkPreview subtree; admission
    // happens asynchronously only after the scroll settles.
    host->setProperty("moving", true);
    auto mixedCast = createSurface(
        engine,
        QStringLiteral("CastItem"),
        *scroll,
        {{QStringLiteral("modelData"), QVariantMap{
            {QStringLiteral("castId"), QStringLiteral("mixed-image-link")},
            {QStringLiteral("displayName"), QStringLiteral("Mixed Cast")},
            {QStringLiteral("author"), QStringLiteral("mixed")},
            {QStringLiteral("text"), QStringLiteral("A cast caption must appear before its media.")},
            {QStringLiteral("embeds"), QVariantList{
                QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("image")},
                    {QStringLiteral("imageSource"), QStringLiteral("image://remote/mixed-image")},
                    {QStringLiteral("aspectRatio"), 1.5},
                },
                QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("url")},
                    {QStringLiteral("url"), QStringLiteral("https://example.invalid/mixed-link")},
                },
            }},
        }}});
    if (!mixedCast) { return 1; }
    auto* mixedCastItem = qobject_cast<QQuickItem*>(mixedCast.get());
    QTest::qWait(120);
    auto* captionLoader = mixedCast->findChild<QQuickItem*>(QStringLiteral("castTextBody"));
    auto* mediaLoader = mixedCast->findChild<QQuickItem*>(QStringLiteral("castVisualMedia"));
    expect(captionLoader && mediaLoader && captionLoader->y() < mediaLoader->y(),
        "text precedes image media in every cast, including mixed media casts");
    expect(countVisualItemsWithClass(*mixedCastItem, QStringLiteral("LinkPreview")) == 0,
        "mixed image+link cast defers its supporting LinkPreview while scrolling");
    host->setProperty("moving", false);
    expect(QTest::qWaitFor([&] {
        return countVisualItemsWithClass(*mixedCastItem, QStringLiteral("LinkPreview")) == 1;
    }), "mixed image+link cast asynchronously admits one LinkPreview after settling");
    mixedCast.reset();
    QCoreApplication::processEvents();

    // Static-role ListModels keep complex payloads in an external side store.
    // A viewer-reaction update must therefore reach the existing delegate
    // without dynamicRoles or a [List -> VariantMap] role conversion.
    QVariantMap reactionRow{
        {QStringLiteral("castId"), QStringLiteral("reaction-row")},
        {QStringLiteral("authorFid"), 42},
        {QStringLiteral("author"), QStringLiteral("reader")},
        {QStringLiteral("displayName"), QStringLiteral("Reader")},
        {QStringLiteral("text"), QString{}},
        {QStringLiteral("embeds"), QVariantList{}},
        {QStringLiteral("replyCount"), 0},
        {QStringLiteral("recastCount"), 2},
        {QStringLiteral("likeCount"), 3},
        {QStringLiteral("likedByMe"), false},
        {QStringLiteral("recastedByMe"), false},
    };
    auto reactionFeed = createSurface(engine, QStringLiteral("FeedSurface"), *scroll,
        {{QStringLiteral("rows"), QVariantList{reactionRow}},
         {QStringLiteral("runtimeReady"), true},
         {QStringLiteral("feedStarted"), true},
         {QStringLiteral("interactionsAvailable"), true},
         {QStringLiteral("presentationMode"), QStringLiteral("all")}});
    if (!reactionFeed) { return 1; }
    auto* reactionFeedItem = qobject_cast<QQuickItem*>(reactionFeed.get());
    QQuickItem* reactionFeedRow = nullptr;
    expect(QTest::qWaitFor([&] {
        const auto rows = visualItemsNamed(*reactionFeedItem, QStringLiteral("feedRow"));
        if (rows.empty())
            return false;
        reactionFeedRow = rows.front();
        return true;
    }), "feed constructs one stable delegate for reaction test row");
    // Verify a click on the actual, lazy-instantiated feed delegate reaches
    // the application-facing reply signal even for unsigned/read-only feeds.
    // The shell then owns authentication/signer and canonical hash validation.
    QSignalSpy requestedReply{reactionFeed.get(), SIGNAL(replyRequested(QVariant))};
    QObject* replyButton = nullptr;
    expect(QTest::qWaitFor([&] {
        replyButton = reactionFeedRow
            ? reactionFeedRow->findChild<QObject*>(QStringLiteral("castReplyButton"))
            : nullptr;
        return replyButton != nullptr;
    }), "feed delegate exposes its actual reply action");
    if (replyButton) {
        expect(QMetaObject::invokeMethod(replyButton, "clicked"),
            "pressing the feed reply button dispatches a click");
        expect(requestedReply.count() == 1
                   && requestedReply.at(0).at(0).toMap().value(QStringLiteral("castId")).toString()
                          == QStringLiteral("reaction-row"),
            "reply event carries the exact cast payload to the shell");
    }
    auto reactionPayload = [&]() {
        return reactionFeedRow ? reactionFeedRow->property("payload").toMap() : QVariantMap{};
    };
    expect(!reactionPayload().value(QStringLiteral("likedByMe")).toBool()
               && !reactionPayload().value(QStringLiteral("recastedByMe")).toBool(),
        "reaction row starts inactive");
    reactionRow.insert(QStringLiteral("likedByMe"), true);
    reactionRow.insert(QStringLiteral("recastedByMe"), true);
    reactionRow.insert(QStringLiteral("likeCount"), 4);
    reactionRow.insert(QStringLiteral("recastCount"), 3);
    reactionFeed->setProperty("rows", QVariantList{reactionRow});
    expect(QTest::qWaitFor([&] {
        const QVariantMap payload = reactionPayload();
        return payload.value(QStringLiteral("likedByMe")).toBool()
            && payload.value(QStringLiteral("recastedByMe")).toBool()
            && payload.value(QStringLiteral("likeCount")).toInt() == 4
            && payload.value(QStringLiteral("recastCount")).toInt() == 3;
    }), "viewer like/recast state patches the existing stable feed delegate");
    reactionFeed.reset();
    QCoreApplication::processEvents();

    const int requestsBeforeVideo = images->requestCount();
    // Offscreen Qt 6.12 CI has no PipeWire/PulseAudio service. Keep the real
    // MediaPlayer/VideoOutput lifecycle under test without initializing a
    // nonexistent physical audio sink. Production defaults to audio enabled.
    auto video = createSurface(engine, QStringLiteral("VideoPreview"), *scroll,
        {{QStringLiteral("audioOutputEnabled"), false},
         {QStringLiteral("embed"), QVariantMap{
            {QStringLiteral("url"), QStringLiteral("https://video.invalid/example.mp4")},
            {QStringLiteral("imageSource"), QStringLiteral("image://remote/video-poster")}}}});
    if (!video) { return 1; }
    const auto videoHeight = video->property("implicitHeight").toDouble();
    expect(QTest::qWaitFor([&] { return images->requestCount() > requestsBeforeVideo; }), "idle video requests its poster");
    images->completePending();
    QTest::qWait(100);
    expect(images->requestCount() == requestsBeforeVideo + 1, "video poster and ambient effect share one image request");
    video->setProperty("embed", QVariantMap{
        {QStringLiteral("url"), QStringLiteral("https://video.invalid/example.mp4")},
        {QStringLiteral("imageSource"), QStringLiteral("image://remote/video-poster")},
        {QStringLiteral("width"), 400}, {QStringLiteral("height"), 900}});
    QTest::qWait(80);
    expect(video->property("implicitHeight").toDouble() == videoHeight,
        "late video dimensions cannot resize its reserved poster frame");
    expect(video->findChildren<QMediaPlayer*>().empty(), "idle feed video allocates no MediaPlayer");
    auto* loader = video->findChild<QObject*>(QStringLiteral("feedVideoPlayerLoader"));
    expect(loader != nullptr && !flag(*loader, "active") && flag(*loader, "asynchronous"),
        "video player construction is lazy and asynchronous");
    auto* playControl = video->findChild<QQuickItem*>(QStringLiteral("feedVideoPreviewControl"));
    expect(playControl != nullptr, "video exposes a keyboard-accessible play control");
    if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR") && playControl
        && window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        const QDir artifacts{qEnvironmentVariable("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")};
        expect(artifacts.mkpath(QStringLiteral(".")), "visual evidence directory exists");
        qobject_cast<QQuickItem*>(image.get())->setVisible(false);
        qobject_cast<QQuickItem*>(backdrop.get())->setVisible(false);
        qobject_cast<QQuickItem*>(gallery.get())->setVisible(false);
        multiGalleryItem->setVisible(false);
        auto* videoItem = qobject_cast<QQuickItem*>(video.get());
        auto* poster = playControl->property("background").value<QQuickItem*>();
        for (const QString& mode : {QStringLiteral("light"), QStringLiteral("dark")}) {
            expect(appearance.setThemeMode(mode), "visual appearance mode applies");
            window.setColor(theme->property("background").value<QColor>());
            for (const int viewportWidth : {360, 640, 1024}) {
                videoItem->setX(16);
                videoItem->setY(32);
                videoItem->setWidth(viewportWidth - 32);
                videoItem->setHeight(video->property("implicitHeight").toDouble());
                QTest::qWait(80);
                expect(QTest::qWaitFor([&] {
                    images->completePending();
                    return poster && flag(*poster, "ready");
                }), "resized poster finishes before visual capture");
                QTest::qWait(80);
                expect(window.grabWindow().save(artifacts.filePath(
                    QStringLiteral("video-%1-%2.png").arg(mode).arg(viewportWidth))),
                    "video preview screenshot saved");
            }
        }
        videoItem->setPosition(QPointF{0, 0});
        videoItem->setSize(QSizeF{480, 280});
        QTest::qWait(80);
        expect(QTest::qWaitFor([&] {
            images->completePending();
            return poster && flag(*poster, "ready");
        }), "poster returns to interaction-test geometry");
    }
    if (playControl) {
        playControl->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
        expect(QTest::qWaitFor([&] { return flag(*video, "playerReady"); }), "Space activates the canonical player");
        expect(video->findChildren<QMediaPlayer*>().size() == 1, "explicit playback constructs exactly one MediaPlayer");
        expect(video->property("implicitHeight").toDouble() == videoHeight,
            "lazy playback activation preserves video row geometry");
        video->setProperty("embed", QVariantMap{
            {QStringLiteral("url"), QStringLiteral("https://video.invalid/recycled.mp4")}});
        expect(!flag(*video, "playbackRequested"), "delegate reuse cancels previous playback intent");
        expect(QTest::qWaitFor([&] { return video->findChildren<QMediaPlayer*>().empty(); }),
            "delegate reuse releases the previous multimedia stack");
    }
    images->completePending();
    // Model delegate construction accurately: nested children complete before
    // their ancestor is attached to the moving Flickable's content item.
    auto flickable = createInline(engine, R"(
        import QtQuick
        Flickable {
            width: 560; height: 760; contentWidth: width; contentHeight: 8000
            flickDeceleration: 100
            function startMotion() { flick(0, -1800) }
        }
    )");
    if (!flickable) { return 1; }
    auto* flickItem = qobject_cast<QQuickItem*>(flickable.get());
    flickItem->setParentItem(window.contentItem());
    expect(QMetaObject::invokeMethod(flickable.get(), "startMotion"), "real flick starts");
    expect(QTest::qWaitFor([&] { return flag(*flickable, "moving"); }), "real Flickable is moving");
    const int requestsBeforeAttachment = images->requestCount();
    const QByteArray nestedSource = QByteArray{"import QtQuick\nimport \""}
        + QUrl::fromLocalFile(QStringLiteral(MEDIA_COMPONENT_DIRECTORY)).toEncoded()
        + QByteArray{R"(" as Components
        Item {
            width: 480; height: 280
            Components.SuperellipseImage {
                objectName: "nestedImage"
                anchors.fill: parent
                source: "image://remote/nested-moving"
                fillMode: Image.PreserveAspectFit
            }
        }
    )"};
    auto nested = createInline(engine, nestedSource);
    if (!nested) { return 1; }
    qobject_cast<QQuickItem*>(nested.get())->setParentItem(
        flickable->property("contentItem").value<QQuickItem*>());
    auto* nestedImage = nested->findChild<QObject*>(QStringLiteral("nestedImage"));
    expect(nestedImage != nullptr, "nested image fixture exists");
    const double initialContentY = flickable->property("contentY").toDouble();
    expect(QTest::qWaitFor([&] { return flickable->property("contentY").toDouble() > initialContentY; }),
        "real scroll advances while media remains pending");
    expect(nestedImage && flag(*nestedImage, "scrollMoving"), "late-attached delegate discovers its moving ancestor");
    expect(images->requestCount() == requestsBeforeAttachment,
        "late-attached delegate does not start its image during a fling");
    expect(QMetaObject::invokeMethod(flickable.get(), "cancelFlick"), "real flick stops");
    expect(QTest::qWaitFor([&] { return images->requestCount() > requestsBeforeAttachment; }),
        "late-attached image starts after actual Flickable settles");
    images->completePending();
    expect(QTest::qWaitFor([&] {
        window.update();
        window.grabWindow();
        QCoreApplication::processEvents();
        return nestedImage && flag(*nestedImage, "ready");
    }),
        "late-attached image eventually presents");

    // Cover source-change callbacks as well as initial construction. Keep each
    // object alive after invalidating its context, with an update still queued.
    for (const QString& surface : {QStringLiteral("SuperellipseImage"),
                                   QStringLiteral("AmbientMediaBackdrop")}) {
        auto creationContext = std::make_unique<QQmlContext>(context);
        auto discarded = createSurface(engine, surface, *scroll, {}, creationContext.get());
        if (!discarded) { return 1; }
        expect(QTest::qWaitFor([&] { return flag(*discarded, "componentReady"); }),
            "media initializes before source-update teardown");
        if (surface == QStringLiteral("SuperellipseImage")) {
            discarded->setProperty("activeSource", QUrl{QStringLiteral("image://remote/discarded")});
        } else {
            discarded->setProperty("sourceItem", image->property("renderItem"));
        }
        creationContext.reset();
        QTest::qWait(80);
        expect(qmlWarnings.empty(), "queued media update cannot evaluate an invalid context");
        qmlWarnings.clear();
    }
    images->completePending();
    // Exercise the actual feed composition, not only GalleryLayout's default:
    // CastEmbedsPanel used to opt back into post-decode aspect changes.
    auto feedPanel = createSurface(engine, QStringLiteral("CastEmbedsPanel"), *scroll,
        {{QStringLiteral("visualEmbeds"), QVariantList{QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("image")},
            {QStringLiteral("imageSource"), QStringLiteral("image://remote/feed-reservation")}}}}});
    if (!feedPanel) { return 1; }
    auto* feedPanelItem = qobject_cast<QQuickItem*>(feedPanel.get());
    // Layout geometry is polished independently of image completion. Do not
    // assume the window receives a render frame within an arbitrary 100 ms.
    expect(QTest::qWaitFor([&] {
        feedPanelItem->ensurePolished();
        return feedPanelItem->implicitHeight() > 280;
    }), "feed reserves a complete image frame before decoding");
    const double reservedFeedHeight = feedPanel->property("implicitHeight").toDouble();
    images->completePending();
    QTest::qWait(260);
    expect(feedPanel->property("implicitHeight").toDouble() == reservedFeedHeight,
        "real feed panel cannot resize after single-image decoding and settling");
    feedPanel->setProperty("visualEmbeds", QVariantList{QVariantMap{
        {QStringLiteral("kind"), QStringLiteral("image")},
        {QStringLiteral("imageSource"), QStringLiteral("image://remote/feed-reservation")},
        {QStringLiteral("width"), 1920}, {QStringLiteral("height"), 1080}}});
    QTest::qWait(100);
    expect(feedPanel->property("implicitHeight").toDouble() == reservedFeedHeight,
        "late dimensions for unchanged media do not replace the reserved aspect");

    auto embedCard = createSurface(engine, QStringLiteral("EmbedPreview"), *scroll,
        {{QStringLiteral("embed"), QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("image")},
            {QStringLiteral("url"), QStringLiteral("image://remote/declared-dimensions")},
            {QStringLiteral("width"), 1920}, {QStringLiteral("height"), 1080}}}});
    if (!embedCard) { return 1; }
    QTest::qWait(100);
    auto* embedItem = qobject_cast<QQuickItem*>(embedCard.get());
    const double embedHeight = embedItem->implicitHeight();
    expect(embedHeight >= 270 && embedHeight < 320, "known dimensions reserve landscape geometry before decoding");
    double minimumEmbedHeight = embedHeight;
    const auto heightConnection = QObject::connect(embedItem, &QQuickItem::implicitHeightChanged,
        [&] { minimumEmbedHeight = std::min(minimumEmbedHeight, embedItem->implicitHeight()); });
    embedCard->setProperty("embed", QVariantMap{
        {QStringLiteral("kind"), QStringLiteral("image")},
        {QStringLiteral("url"), QStringLiteral("image://remote/declared-dimensions")},
        {QStringLiteral("title"), QStringLiteral("Updated metadata")},
        {QStringLiteral("width"), 1920}, {QStringLiteral("height"), 1080}});
    QTest::qWait(100);
    expect(minimumEmbedHeight == embedHeight, "same-URL refresh never unloads the card to zero height");
    QObject::disconnect(heightConnection);
    images->completePending();

    for (const QString& kind : {QStringLiteral("image"), QStringLiteral("video")}) {
        const QString url = QStringLiteral("https://example.invalid/extensionless-") + kind;
        auto promoted = createSurface(engine, QStringLiteral("EmbedPreview"), *scroll,
            {{QStringLiteral("embed"), QVariantMap{{QStringLiteral("kind"), QStringLiteral("link")},
                                                   {QStringLiteral("url"), url}}}});
        if (!promoted) { return 1; }
        QTest::qWait(100);
        auto* promotedItem = qobject_cast<QQuickItem*>(promoted.get());
        const double linkHeight = promotedItem->implicitHeight();
        promoted->setProperty("embed", QVariantMap{
            {QStringLiteral("kind"), kind}, {QStringLiteral("url"), url},
            {QStringLiteral("imageSource"), QStringLiteral("image://remote/promoted-") + kind}});
        QTest::qWait(100);
        images->completePending();
        QTest::qWait(100);
        expect(promotedItem->implicitHeight() == linkHeight,
            "late direct-media classification retains the visible link's reserved height");
        expect(promoted->property("resolvedKind").toString() == kind,
            "stable geometry does not suppress direct-media classification");
    }

    for (auto* child : scroll->childItems()) {
        child->setVisible(false);
    }
    flickItem->setVisible(false);

    // A link entering the viewport during a fling is waiting for metadata even
    // before the debounced resolver call starts. It must keep the same spinner
    // across that boundary instead of flashing the fallback image glyph.
    host->setProperty("moving", true);
    auto deferredLink = createSurface(engine, QStringLiteral("LinkPreview"), *scroll,
        {{QStringLiteral("embed"), QVariantMap{
            {QStringLiteral("url"), QStringLiteral("https://example.invalid/deferred-link")}}},
         {QStringLiteral("compact"), true}});
    if (!deferredLink) { return 1; }
    QTest::qWait(40);
    auto* deferredLinkSpinner = deferredLink->findChild<QObject*>(
        QStringLiteral("genericLinkLoadingSpinner"));
    auto* deferredLinkFallback = deferredLink->findChild<QObject*>(
        QStringLiteral("genericLinkFallbackIcon"));
    expect(flag(*deferredLink, "metadataLoading"),
        "deferred link metadata remains in the loading state while scrolling");
    expect(deferredLinkSpinner && deferredLinkFallback
               && flag(*deferredLinkSpinner, "visible") && !flag(*deferredLinkFallback, "visible"),
        "deferred link keeps its spinner instead of flashing an image glyph");
    expect(deferredLinkSpinner && !flag(*deferredLinkSpinner, "running"),
        "deferred link freezes indeterminate animation during the fling");
    host->setProperty("moving", false);
    expect(deferredLinkSpinner && flag(*deferredLinkSpinner, "running"),
        "deferred link resumes progress animation after the fling");
    expect(flag(*deferredLink, "metadataLoading") && deferredLinkSpinner && deferredLinkFallback
               && flag(*deferredLinkSpinner, "visible") && !flag(*deferredLinkFallback, "visible"),
        "link request debounce preserves the same loading placeholder after the fling");
    deferredLink.reset();

    // The HTTP provider can still be waiting after five seconds. A UI-only
    // deadline must not pretend those pending images failed, hide progress or
    // affect a real Flickable's movement. Exercise every link artwork layout.
    auto slowFlickable = createInline(engine, R"(
        import QtQuick
        Flickable {
            width: 560; height: 760; contentWidth: width; contentHeight: 12000
            flickDeceleration: 100
            function startMotion() { flick(0, -1800) }
        }
    )");
    if (!slowFlickable) { return 1; }
    auto* slowFlickItem = qobject_cast<QQuickItem*>(slowFlickable.get());
    slowFlickItem->setParentItem(window.contentItem());
    auto* slowContent = slowFlickable->property("contentItem").value<QQuickItem*>();
    std::vector<std::unique_ptr<QObject>> slowLinks;
    const int requestsBeforeSlowLinks = images->requestCount();
    for (const QString& kind : {QStringLiteral("web"), QStringLiteral("youtube"), QStringLiteral("miniapp")}) {
        auto slowLink = createSurface(engine, QStringLiteral("LinkPreview"), *slowContent,
            {{QStringLiteral("embed"), QVariantMap{{QStringLiteral("url"),
                QStringLiteral("https://example.invalid/slow-") + kind}}},
             {QStringLiteral("metadataManagedExternally"), true},
             {QStringLiteral("externalPreview"), QVariantMap{
                {QStringLiteral("providerKind"), kind}, {QStringLiteral("resolved"), true},
                {QStringLiteral("previewImage"), QStringLiteral("image://remote/slow-") + kind}}}});
        if (!slowLink) { return 1; }
        qobject_cast<QQuickItem*>(slowLink.get())->setY(static_cast<double>(slowLinks.size()) * 300);
        slowLinks.push_back(std::move(slowLink));
    }
    expect(QTest::qWaitFor([&] { return images->requestCount() >= requestsBeforeSlowLinks + 3; }),
        "all slow link posters start their controlled image request");
    QTest::qWait(5200);
    const QStringList slowPrefixes{QStringLiteral("generic"), QStringLiteral("rich"), QStringLiteral("miniApp")};
    for (std::size_t index = 0; index < slowLinks.size(); ++index) {
        const auto prefix = slowPrefixes.at(static_cast<qsizetype>(index));
        auto* spinner = slowLinks[index]->findChild<QObject*>(prefix + QStringLiteral("LinkLoadingSpinner"));
        auto* fallback = slowLinks[index]->findChild<QObject*>(prefix + QStringLiteral("LinkFallbackIcon"));
        expect(spinner && flag(*spinner, "visible") && flag(*spinner, "running")
                   && fallback && !flag(*fallback, "visible"),
            "an image still awaiting its provider after five seconds retains animated progress");
    }
    expect(QMetaObject::invokeMethod(slowFlickable.get(), "startMotion"), "slow-image fixture starts a real flick");
    expect(QTest::qWaitFor([&] { return flag(*slowFlickable, "moving"); }),
        "slow remote images do not prevent a real flick from starting");
    const double slowInitialY = slowFlickable->property("contentY").toDouble();
    expect(QTest::qWaitFor([&] { return slowFlickable->property("contentY").toDouble() > slowInitialY + 100; }),
        "real scroll advances by 100 pixels with all image responses still held pending");
    expect(QMetaObject::invokeMethod(slowFlickable.get(), "cancelFlick"), "slow-image fixture stops its flick");
    slowLinks.clear();
    slowFlickable.reset();
    images->completePending();

    for (const int cardWidth : {360, 640}) {
        for (const QString& cardKind : {QStringLiteral("web"), QStringLiteral("youtube"), QStringLiteral("miniapp")}) {
            const QString url = cardKind == QStringLiteral("youtube")
                ? QStringLiteral("https://www.youtube.com/watch?v=fixture")
                : QStringLiteral("https://example.invalid/") + cardKind;
            auto card = createSurface(engine, QStringLiteral("LinkPreview"), *scroll,
                {{QStringLiteral("embed"), QVariantMap{{QStringLiteral("url"), url}}},
                 {QStringLiteral("compact"), cardWidth < 520},
                 {QStringLiteral("metadataManagedExternally"), true},
                 {QStringLiteral("externalPreview"), QVariantMap{{QStringLiteral("providerKind"), cardKind}}}});
            if (!card) { return 1; }
            auto* cardItem = qobject_cast<QQuickItem*>(card.get());
            cardItem->setWidth(cardWidth);
            QTest::qWait(80);
            polishTree(cardItem);
            cardItem->setHeight(cardItem->implicitHeight());
            QTest::qWait(80);
            const double reservedHeight = cardItem->implicitHeight();
            if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_DIAGNOSTICS"))
                std::println("CARD kind={} width={} initial={} layout={} aspect={}",
                    cardKind.toStdString(), cardWidth, reservedHeight,
                    card->property("reservedLayout").toMap().value("kind").toString().toStdString(),
                    card->property("previewImageAspect").toDouble());
            const double reservedAspect = card->property("previewImageAspect").toDouble();
            expect(reservedHeight > 100, "link reserves card geometry before metadata arrives");
            QVariantMap metadata{
                {QStringLiteral("providerKind"), cardKind}, {QStringLiteral("resolved"), true},
                {QStringLiteral("previewImage"), QStringLiteral("image://remote/link-") + cardKind},
                {QStringLiteral("imageWidth"), 400}, {QStringLiteral("imageHeight"), 900},
                {QStringLiteral("title"), QStringLiteral("A long late title that wraps onto a second line in the preview")},
                {QStringLiteral("summary"), QStringLiteral("Late metadata summary with enough text to occupy several lines on narrow cards. ").repeated(3)},
                {QStringLiteral("siteName"), QStringLiteral("Example")},
                {QStringLiteral("domain"), QStringLiteral("example.invalid")},
                {QStringLiteral("author"), QStringLiteral("Author")}};
            card->setProperty("externalPreview", metadata);
            QTest::qWait(80);
            polishTree(cardItem);
            if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_DIAGNOSTICS"))
                std::println("CARD kind={} width={} resolved={}",
                    cardKind.toStdString(), cardWidth, cardItem->implicitHeight());
            expect(cardItem->implicitHeight() == reservedHeight,
                "late link title, summary and artwork metadata preserve card height");
            expect(card->property("previewImageAspect").toDouble() == reservedAspect,
                "late image dimensions preserve reserved poster aspect");
            images->completePending();
            QTest::qWait(100);
            if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_DIAGNOSTICS"))
                std::println("CARD kind={} width={} image={}",
                    cardKind.toStdString(), cardWidth, cardItem->implicitHeight());
            expect(cardItem->implicitHeight() == reservedHeight, "link image completion preserves card height");
            if (cardWidth == 360 && cardKind == QStringLiteral("web")) {
                QTest::mouseMove(&window, QPoint{520, 700});
                QTest::qWait(200);
                auto* linkEffect = card->findChild<QObject*>(QStringLiteral("ambientEffectLoader"));
                auto* linkCapture = card->findChild<QObject*>(QStringLiteral("ambientMediaCapture"));
                expect(linkEffect && !flag(*linkEffect, "active")
                           && !linkEffect->property("item").value<QObject*>() && !linkCapture,
                    "loaded feed links allocate no ambient blur or capture resources");
                const bool usesGpu = window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software;
                if (usesGpu) {
                    window.grabWindow();
                    QCoreApplication::processEvents();
                }
                const int requestsBeforeLinkHover = images->requestCount();
                QTest::mouseMove(&window, QPoint{80, 60});
                QTest::qWait(200);
                expect(card->property("fillColor").value<QColor>() == theme->property("surfaceHover").value<QColor>(),
                    "real link hover reaches its visual feedback state");
                if (usesGpu) {
                    window.grabWindow();
                    QCoreApplication::processEvents();
                }
                expect(!card->findChild<QObject*>(QStringLiteral("ambientMediaCapture"))
                           && images->requestCount() == requestsBeforeLinkHover,
                    "link hover creates no ambient texture and restarts no image request");
                QTest::mouseMove(&window, QPoint{520, 700});
            }
            if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")
                && window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
                const QDir artifacts{qEnvironmentVariable("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")};
                cardItem->setPosition(QPointF{16, 16});
                for (const QString& mode : {QStringLiteral("light"), QStringLiteral("dark")}) {
                    expect(appearance.setThemeMode(mode), "link visual theme applies");
                    window.setColor(theme->property("background").value<QColor>());
                    QTest::qWait(80);
                    expect(window.grabWindow().save(artifacts.filePath(
                        QStringLiteral("link-%1-%2-%3.png").arg(cardKind).arg(mode).arg(cardWidth))),
                        "reserved link layout screenshot saved");
                }
            }
            card->setProperty("metadataTimedOut", true);
            QTest::qWait(80);
            expect(cardItem->implicitHeight() == reservedHeight, "preview timeout cannot collapse a reserved frame");
            metadata.remove(QStringLiteral("previewImage"));
            card->setProperty("externalPreview", metadata);
            QTest::qWait(80);
            expect(cardItem->implicitHeight() == reservedHeight, "missing artwork keeps the reserved fallback frame");
            metadata.insert(QStringLiteral("previewImage"), QStringLiteral("image://remote/failure-") + cardKind);
            card->setProperty("externalPreview", metadata);
            QTest::qWait(80);
            images->completePending();
            QTest::qWait(80);
            expect(cardItem->implicitHeight() == reservedHeight, "failed artwork keeps the reserved frame");
            card->setProperty("embed", QVariantMap{{QStringLiteral("url"), url + QStringLiteral("&new-identity")}});
            QTest::qWait(80);
            expect(card->property("previewImageAspect").toDouble() == 400.0 / 900.0,
                "URL reuse reserves from the next identity's available dimensions");
        }

        auto collectible = createSurface(engine, QStringLiteral("CollectiblePreview"), *scroll,
            {{QStringLiteral("embed"), QVariantMap{{QStringLiteral("url"), QStringLiteral("https://opensea.io/assets/fixture")}}},
             {QStringLiteral("compact"), cardWidth < 520}});
        if (!collectible) { return 1; }
        auto* collectibleItem = qobject_cast<QQuickItem*>(collectible.get());
        collectibleItem->setWidth(cardWidth);
        QTest::qWait(80);
        polishTree(collectibleItem);
        collectibleItem->setHeight(collectibleItem->implicitHeight());
        QTest::qWait(80);
        const double collectibleHeight = collectibleItem->implicitHeight();
        collectible->setProperty("embed", QVariantMap{
            {QStringLiteral("url"), QStringLiteral("https://opensea.io/assets/fixture")},
            {QStringLiteral("imageSource"), QStringLiteral("image://remote/collectible")},
            {QStringLiteral("title"), QStringLiteral("A long collectible title arriving with resolved metadata")},
            {QStringLiteral("creator"), QStringLiteral("Creator")},
            {QStringLiteral("collection"), QStringLiteral("Collection")},
            {QStringLiteral("chain"), QStringLiteral("Ethereum")},
            {QStringLiteral("edition"), QStringLiteral("123/456")},
            {QStringLiteral("price"), QStringLiteral("12.34 ETH")}});
        QTest::qWait(80);
        images->completePending();
        QTest::qWait(100);
        expect(collectibleItem->implicitHeight() == collectibleHeight,
            "collectible metadata and image arrival preserve card height at each breakpoint");
        if (qEnvironmentVariableIsSet("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")
            && window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
            const QDir artifacts{qEnvironmentVariable("GENYCASTER_MEDIA_SCROLL_ARTIFACT_DIR")};
            collectibleItem->setPosition(QPointF{16, 16});
            for (const QString& mode : {QStringLiteral("light"), QStringLiteral("dark")}) {
                expect(appearance.setThemeMode(mode), "collectible visual theme applies");
                window.setColor(theme->property("background").value<QColor>());
                QTest::qWait(80);
                expect(window.grabWindow().save(artifacts.filePath(
                    QStringLiteral("collectible-%1-%2.png").arg(mode).arg(cardWidth))),
                    "reserved collectible layout screenshot saved");
            }
        }
        collectibleItem->setVisible(false);
    }
    // Regression: the same destination may appear once as a protocol embed
    // and again as an inferred/referral link with a different sharedBy key.
    // One visual link card must survive without swallowing meaningful IDs.
    auto duplicateLinks = createSurface(engine, QStringLiteral("CastEmbedsPanel"), *scroll,
        {{QStringLiteral("supportingEmbeds"), QVariantList{
            QVariantMap{{QStringLiteral("kind"), QStringLiteral("link")},
                        {QStringLiteral("url"), QStringLiteral("https://gm.lens.xyz?sharedBy=one")}},
            QVariantMap{{QStringLiteral("kind"), QStringLiteral("link")},
                        {QStringLiteral("url"), QStringLiteral("https://gm.lens.xyz/?sharedBy=two&utm_source=feed")}},
            QVariantMap{{QStringLiteral("kind"), QStringLiteral("link")},
                        {QStringLiteral("url"), QStringLiteral("https://example.test/post?id=7")}},
            QVariantMap{{QStringLiteral("kind"), QStringLiteral("link")},
                        {QStringLiteral("url"), QStringLiteral("https://example.test/post?id=8")}},
        }}});
    if (!duplicateLinks) { return 1; }
    expect(duplicateLinks->property("supportingCount").toInt() == 3,
           "matching referral links render one card, different content IDs remain distinct");
    qobject_cast<QQuickItem*>(duplicateLinks.get())->setVisible(false);

    auto supportingPanel = createSurface(engine, QStringLiteral("CastEmbedsPanel"), *scroll,
        {{QStringLiteral("supportingEmbeds"), QVariantList{QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("link")},
            {QStringLiteral("url"), QStringLiteral("https://example.test/first")}}}}});
    if (!supportingPanel) { return 1; }
    QPointer<QObject> retainedLink;
    std::function<void(QQuickItem*)> findLink = [&](QQuickItem* item) {
        if (item->metaObject()->indexOfProperty("layoutReservation") >= 0) {
            retainedLink = item;
            return;
        }
        for (auto* child : item->childItems()) { findLink(child); }
    };
    expect(QTest::qWaitFor([&] {
        retainedLink.clear();
        findLink(qobject_cast<QQuickItem*>(supportingPanel.get()));
        return retainedLink != nullptr;
    }), "supporting panel asynchronously creates a real link card after admission");
    const QPointer<QObject> firstLink = retainedLink;
    supportingPanel->setProperty("supportingEmbeds", QVariantList{QVariantMap{
        {QStringLiteral("kind"), QStringLiteral("link")},
        {QStringLiteral("url"), QStringLiteral("https://example.test/second")}}});
    expect(QTest::qWaitFor([&] {
        return firstLink
            && firstLink->property("targetUrl").toString()
                   == QStringLiteral("https://example.test/second");
    }), "recycling a supporting link updates its admitted card without rebuilding it");
    expect(retainedLink == firstLink,
        "supporting link reuse preserves the admitted preview object");
    qobject_cast<QQuickItem*>(supportingPanel.get())->setVisible(false);

    const QVariantMap menuRow{{QStringLiteral("castId"), QStringLiteral("menu-first")},
        {QStringLiteral("author"), QStringLiteral("reader")}, {QStringLiteral("text"), QString{}},
        {QStringLiteral("embeds"), QVariantList{}}};
    auto menuCast = createSurface(engine, QStringLiteral("CastItem"), *scroll,
        {{QStringLiteral("modelData"), menuRow}, {QStringLiteral("compact"), true}});
    if (!menuCast) { return 1; }
    expect(!menuCast->findChild<QObject*>(QStringLiteral("castMenu")), "idle cast does not construct its action menu");
    expect(!menuCast->findChild<QObject*>(QStringLiteral("actionHint")), "idle cast does not construct button tooltips");
    QSignalSpy conversationRequested{menuCast.get(), SIGNAL(openRequested(QString))};
    expect(QMetaObject::invokeMethod(menuCast.get(), "openMoreMenu"), "cast menu opens on explicit interaction");
    QPointer<QObject> menu = menuCast->findChild<QObject*>(QStringLiteral("castMenu"));
    expect(menu && menu->property("visible").toBool(), "lazily created cast menu is visible");
    if (menu) {
        QObject* conversationAction = nullptr;
        for (auto* child : menu->findChildren<QObject*>()) {
            if (child->property("iconGlyph") == theme->property("iconComment")) {
                conversationAction = child;
                break;
            }
        }
        expect(conversationAction && QMetaObject::invokeMethod(conversationAction, "clicked"),
            "lazy menu retains the conversation action");
        expect(conversationRequested.count() == 1 && conversationRequested.at(0).at(0).toString() == QStringLiteral("menu-first"),
            "lazy menu action targets the correct cast");
        auto nextRow = menuRow;
        nextRow.insert(QStringLiteral("castId"), QStringLiteral("menu-second"));
        nextRow.insert(QStringLiteral("text"),
                       QStringLiteral("Tap the cast body to view the conversation"));
        menuCast->setProperty("modelData", nextRow);
        expect(QTest::qWaitFor([&] { return menu.isNull(); }),
            "recycling a cast releases its old menu");
        auto* textBody = menuCast->findChild<QQuickItem*>(QStringLiteral("castTextBody"));
        expect(textBody && QTest::qWaitFor([&] {
            return textBody->property("active").toBool()
                && textBody->width() > 100 && textBody->height() > 0;
        }), "the ordinary cast text is available as a tappable content region");
        if (textBody) {
            const QPoint point = textBody->mapToScene(
                QPointF{std::min(125.0, textBody->width() / 2), textBody->height() / 2}).toPoint();
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
            expect(QTest::qWaitFor([&] { return conversationRequested.count() == 2; })
                && conversationRequested.at(1).at(0).toString() == QStringLiteral("menu-second"),
                "tapping ordinary cast body opens the same conversation as the menu action");
        }
    }
    qobject_cast<QQuickItem*>(menuCast.get())->setVisible(false);

    // The tooltip intentionally does not appear on narrow touch layouts.
    // Use a desktop-width test window for pointer-only tooltip acceptance,
    // then return to the original compact viewport for the feed stress test.
    const QSize priorWindowSize = window.size();
    window.resize(1100, 760);
    auto hintButton = createSurface(engine, QStringLiteral("ActionButton"), *scroll,
        {{QStringLiteral("iconOnly"), true}, {QStringLiteral("text"), QStringLiteral("Media action")}});
    if (!hintButton) { return 1; }
    auto* hintItem = qobject_cast<QQuickItem*>(hintButton.get());
    hintItem->setWidth(48);
    hintItem->setHeight(48);
    expect(!hintButton->findChild<QObject*>(QStringLiteral("actionHint")), "tooltip is absent before hover");
    QTest::mouseMove(&window, QPoint{530, 700});
    QTest::mouseMove(&window, hintItem->mapToScene(QPointF{24, 24}).toPoint());
    expect(QTest::qWaitFor([&] {
        auto* hint = hintButton->findChild<QObject*>(QStringLiteral("actionHint"));
        return hint && hint->property("visible").toBool();
    }), "hover still opens the lazy tooltip");
    QTest::mouseMove(&window, QPoint{530, 700});
    expect(QTest::qWaitFor([&] {
        auto* hint = hintButton->findChild<QObject*>(QStringLiteral("actionHint"));
        return !hint || !hint->property("visible").toBool();
    }), "tooltip closes when hover ends");
    hintItem->setVisible(false);
    window.resize(priorWindowSize);

    QVariantList feedRows;
    for (int index = 0; index < 40; ++index) {
        const QString kind = index % 2 == 0 ? QStringLiteral("image") : QStringLiteral("link");
        const QString source = QStringLiteral("image://remote/pooled-") + QString::number(index);
        feedRows.append(QVariantMap{
            {QStringLiteral("castId"), QString::number(index)},
            {QStringLiteral("author"), QStringLiteral("reader")},
            {QStringLiteral("text"), QString{}},
            {QStringLiteral("embeds"), QVariantList{QVariantMap{
                {QStringLiteral("kind"), kind}, {QStringLiteral("url"), source},
                {QStringLiteral("imageSource"), source}}}}});
    }
    auto feed = createSurface(engine, QStringLiteral("FeedSurface"), *scroll,
        {{QStringLiteral("rows"), feedRows}, {QStringLiteral("presentationMode"), QStringLiteral("all")},
         {QStringLiteral("runtimeReady"), true}, {QStringLiteral("feedStarted"), true},
         {QStringLiteral("compact"), true}});
    if (!feed) { return 1; }
    auto* list = feed->findChild<QObject*>(QStringLiteral("readList"));
    expect(list != nullptr, "production feed creates its ListView");
    struct ObservedRow {
        QPointer<QQuickItem> item;
        QString layoutKind;
        QString stableKey;
    };
    std::vector<ObservedRow> observedRows;
    int reusedRows = 0;
    bool allObservedRowsAreAsynchronous = true;
    bool allObservedRowsReserveHeight = true;
    std::function<void(QQuickItem*)> inspectRows = [&](QQuickItem* item) {
        if (item->objectName() == QStringLiteral("feedRow")) {
            const QString layoutKind = item->property("mediaLayoutKind").toString();
            const QString stableKey = item->property("stableKey").toString();
            allObservedRowsAreAsynchronous = allObservedRowsAreAsynchronous
                && item->property("asynchronous").toBool();
            allObservedRowsReserveHeight = allObservedRowsReserveHeight
                && item->property("reservedHeight").toDouble() > 0.0 && item->height() > 0.0;
            auto found = std::find_if(observedRows.begin(), observedRows.end(),
                [&](const ObservedRow& row) { return row.item == item; });
            if (found == observedRows.end()) {
                observedRows.push_back({item, layoutKind, stableKey});
            } else if (found->stableKey != stableKey) {
                ++reusedRows;
                expect(found->layoutKind == layoutKind,
                    "recycling a real feed row never swaps image and link layout trees");
                found->stableKey = stableKey;
            }
        }
        for (auto* child : item->childItems()) {
            inspectRows(child);
        }
    };
    if (list) {
        expect(QTest::qWaitFor([&] { return list->property("count").toInt() == 40; }), "production feed receives fixture rows");
        expect(QTest::qWaitFor([&] {
            if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
                window.grabWindow();
            }
            return list->property("contentHeight").toDouble() > list->property("height").toDouble();
        }),
            "asynchronous feed incubation reserves a usable scroll range before every row is ready");
        for (const int index : {0, 6, 12, 18, 24, 30, 24, 18, 12, 6, 0}) {
            expect(QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, index), Q_ARG(int, 0)),
                "production ListView moves through media rows");
            inspectRows(qobject_cast<QQuickItem*>(feed.get()));
            QTest::qWait(80);
            images->completePending();
            inspectRows(qobject_cast<QQuickItem*>(feed.get()));
        }
        expect(!observedRows.empty() && allObservedRowsAreAsynchronous,
            "production cast delegates incubate asynchronously instead of constructing in the scroll frame");
        expect(allObservedRowsReserveHeight,
            "incubating and pooled feed delegates never collapse to zero height");
        expect(reusedRows > 0, "real feed regression exercises pooled delegates");

        // Regress the reported WASM hole: an already-incubated Cast can be
        // shorter than its initial media/text reservation. Even during a real
        // kinetic flick it must release that extra height immediately instead
        // of leaving an empty band until onMovementEnded.
        expect(QMetaObject::invokeMethod(list, "positionViewAtIndex",
            Q_ARG(int, 18), Q_ARG(int, 0)), "position feed for kinetic gap regression");
        QTest::qWait(100);
        auto* realFeedItem = qobject_cast<QQuickItem*>(feed.get());
        QQuickItem* readyRow = nullptr;
        for (auto* candidate : visualItemsNamed(*realFeedItem, QStringLiteral("feedRow"))) {
            if (!candidate->property("incubating").toBool()
                && candidate->property("resolvedHeight").toDouble() > 1.0) {
                readyRow = candidate;
                break;
            }
        }
        expect(readyRow != nullptr, "gap regression finds a measured asynchronous cast");
        if (readyRow) {
            expect(QMetaObject::invokeMethod(list, "flick",
                Q_ARG(qreal, 0.0), Q_ARG(qreal, -1800.0)), "start a real feed flick");
            expect(QTest::qWaitFor([&] { return list->property("moving").toBool(); }),
                "feed kinetic gap regression runs during motion");
            const double measured = readyRow->property("resolvedHeight").toDouble();
            readyRow->setProperty("committedHeight", measured + 240.0);
            expect(QMetaObject::invokeMethod(readyRow, "synchronizeHeight"),
                "reconcile inflated placeholder during kinetic scrolling");
            expect(qAbs(readyRow->height() - measured) < 1.0,
                "ready feed cast never retains a gap-sized reservation during motion");
            QMetaObject::invokeMethod(list, "cancelFlick");
        }
        expect(QMetaObject::invokeMethod(list, "positionViewAtIndex",
            Q_ARG(int, 18), Q_ARG(int, 0)), "feed scrolls before background refresh");
        feed->setProperty("busy", true);
        feed->setProperty("rows", QVariantList{});
        QTest::qWait(120);
        expect(list->property("count").toInt() == 40,
            "temporary empty background feed refresh retains the existing scrollable timeline");
        expect(list->property("contentY").toDouble() > list->property("height").toDouble(),
            "temporary empty background feed refresh does not return to the feed header");
        feed->setProperty("rows", feedRows);
        feed->setProperty("busy", false);
        expect(QTest::qWaitFor([&] { return list->property("count").toInt() == 40; }),
            "completed background refresh reconciles the original feed");
    }
    images->completePending();
    expect(qmlWarnings.empty(), "real media/feed scrolling emits zero QQmlEngine warnings");

    // The first Following story must show the signed-in viewer's actual PFP
    // with a small Add badge, rather than replacing the PFP with a large plus.
    auto selfStory = createSurface(engine, QStringLiteral("StoryItem"), *scroll,
        {{QStringLiteral("addStory"), true},
         {QStringLiteral("displayName"), QStringLiteral("Current user")},
         {QStringLiteral("avatarUrl"), QStringLiteral("image://remote/own-story-avatar")}});
    if (!selfStory) { return 1; }
    auto* ownPortrait = selfStory->findChild<QQuickItem*>(QStringLiteral("storyPortrait"));
    auto* ownAddBadge = selfStory->findChild<QQuickItem*>(QStringLiteral("ownStoryAddBadge"));
    expect(ownPortrait && ownPortrait->isVisible(),
           "first story presents the authenticated user's photo");
    expect(ownAddBadge && ownAddBadge->isVisible(),
           "first story overlays an Add badge without hiding the photo");
    expect(ownPortrait && ownPortrait->property("sourceUrl").toString()
            == QStringLiteral("image://remote/own-story-avatar"),
           "story portrait loads the actual account avatar");
    images->completePending();
    QTest::qWait(30);
    expect(qmlWarnings.empty(), "own-story avatar creates no Qt 6.12 QML warnings");
    std::println("Media scroll regression: {} failure(s)", failures);
    return failures == 0 ? 0 : 1;
}
