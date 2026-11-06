// Qt font resource policy belongs to a private implementation unit; the
// public C++26 module does not export Qt's internal templates or QObject ABI.
module;
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
module genycaster.ui.bootstrap.fonts;

namespace genycaster::ui::bootstrap {

struct FontFamilies::Impl final {
    QString persian{};
    QString devanagari{};
    QString japanese{};
    QString korean{};
    QString chinese{};
    QString iconSolid{};
    QString iconRegular{};
    QString systemText{};
    QString systemRtl{};
    QString systemDevanagari{};
    QString systemJapanese{};
    QString systemKorean{};
    QString systemChinese{};
};

QString FontFamilies::forLanguage(QString languageTag) const
{
    languageTag = languageTag.trimmed().toLower();
    if (languageTag == QStringLiteral("fa") || languageTag == QStringLiteral("ar"))
        return !m_impl->persian.isEmpty() ? m_impl->persian : m_impl->systemRtl;
    if (languageTag == QStringLiteral("hi"))
        return !m_impl->devanagari.isEmpty() ? m_impl->devanagari : m_impl->systemDevanagari;
    if (languageTag == QStringLiteral("ja"))
        return !m_impl->japanese.isEmpty() ? m_impl->japanese : m_impl->systemJapanese;
    if (languageTag == QStringLiteral("ko"))
        return !m_impl->korean.isEmpty() ? m_impl->korean : m_impl->systemKorean;
    if (languageTag == QStringLiteral("zh"))
        return !m_impl->chinese.isEmpty() ? m_impl->chinese : m_impl->systemChinese;
    return m_impl->systemText;
}

FontFamilies::FontFamilies() : m_impl{std::make_unique<Impl>()} {}
FontFamilies::~FontFamilies() = default;

bool FontFamilies::load()
{
    QString& bundledPersianFontFamily = m_impl->persian;
    QString& bundledDevanagariFontFamily = m_impl->devanagari;
    QString& bundledJapaneseFontFamily = m_impl->japanese;
    QString& bundledKoreanFontFamily = m_impl->korean;
    QString& bundledChineseFontFamily = m_impl->chinese;
    QString& bundledIconFontFamily = m_impl->iconSolid;
    QString& bundledIconRegularFontFamily = m_impl->iconRegular;

    // The minimal QPA backend used by headless structure tests intentionally has
    // no application-font backend. Shipping platforms load every bundled family;
    // a missing resource is a packaging defect rather than a system-font fallback.
    if (QGuiApplication::platformName() != QStringLiteral("minimal")) {
        const auto loadApplicationFont = [](const QString& resource) -> QStringList {
            const int fontId = QFontDatabase::addApplicationFont(resource);
            if (fontId < 0) {
                qCritical().noquote() << "Unable to load embedded Genycaster font:" << resource;
                return {};
            }
            return QFontDatabase::applicationFontFamilies(fontId);
        };

        constexpr std::array bundledPersianFontResources{
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Regular.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Medium.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-SemiBold.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Bold.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Black.ttf",
        };
        for (const char* const resource : bundledPersianFontResources) {
            const QStringList families = loadApplicationFont(QString::fromLatin1(resource));
            if (families.isEmpty()) {
                return false;
            }
            if (bundledPersianFontFamily.isEmpty()) {
                bundledPersianFontFamily = families.front();
            }
        }

#if defined(__EMSCRIPTEN__)
        // Qt 6.12 supports CBDT/CBLC and emoji sequence segmentation.
        // System/browser fonts are not available in the WASM sandbox.
        const QStringList colorEmojiFamilies = loadApplicationFont(QStringLiteral(
            ":/qt/qml/Farcaster/Client/assets/fonts/GenycasterColorEmoji.ttf"));
        const QStringList monoEmojiFamilies = loadApplicationFont(QStringLiteral(
            ":/qt/qml/Farcaster/Client/assets/fonts/GenycasterEmojiFallback.ttf"));
        if (monoEmojiFamilies.isEmpty()) {
            return false;
        }
        if (!colorEmojiFamilies.isEmpty()) {
            QFontDatabase::setApplicationEmojiFontFamilies(
                {colorEmojiFamilies.front(), monoEmojiFamilies.front()});
        } else {
            qWarning() << "Bundled color emoji unavailable; using monochrome fallback";
            QFontDatabase::addApplicationEmojiFontFamily(monoEmojiFamilies.front());
        }
#endif

        // Devanagari/CJK resources are optional in source/developer builds.
        // Production packaging can require them through
        // GENYCASTER_REQUIRE_SCRIPT_FONTS; when absent, the platform fallback
        // families selected below provide shaping without making configure depend
        // on network access.
        const QString devanagariResource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/fonts/NotoSansDevanagari.ttf");
        if (QFile::exists(devanagariResource)) {
            const QStringList devanagariFamilies = loadApplicationFont(devanagariResource);
            if (!devanagariFamilies.isEmpty()) {
                bundledDevanagariFontFamily = devanagariFamilies.front();
            }
        }

        const QString cjkResource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/fonts/NotoSansCJK-Regular.ttc");
        if (QFile::exists(cjkResource)) {
            const QStringList cjkFamilies = loadApplicationFont(cjkResource);
            if (!cjkFamilies.isEmpty()) {
                const auto chooseCjkFamily = [&cjkFamilies](const QString& suffix) {
                    const auto exact = std::ranges::find_if(cjkFamilies, [&](const QString& family) {
                        return family.contains(suffix, Qt::CaseInsensitive);
                    });
                    return exact != cjkFamilies.end() ? *exact : cjkFamilies.front();
                };
                bundledJapaneseFontFamily = chooseCjkFamily(QStringLiteral("JP"));
                bundledKoreanFontFamily = chooseCjkFamily(QStringLiteral("KR"));
                bundledChineseFontFamily = chooseCjkFamily(QStringLiteral("SC"));
            }
        }

        const QStringList iconRegularFamilies = loadApplicationFont(
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/fa-regular-400.ttf"));
        const QStringList iconSolidFamilies = loadApplicationFont(
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/fa-solid-900.ttf"));
        if (iconRegularFamilies.isEmpty() || iconSolidFamilies.isEmpty()) {
            return false;
        }
        bundledIconRegularFontFamily = iconRegularFamilies.front();
        // Keep the legacy property populated for compatibility with older QML;
        // new Icon.qml selects the regular or solid family explicitly.
        bundledIconFontFamily = iconSolidFamilies.front();
    }

    const auto installedFamilies = QFontDatabase::families();
    const auto preferredFamily = [&installedFamilies](const QStringList& candidates, const QString& fallback) {
        for (const auto& candidate : candidates) {
            if (installedFamilies.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
        return fallback;
    };
    const QString systemFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#if defined(Q_OS_MACOS)
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("SF Pro Text"), QStringLiteral("Helvetica Neue")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Geeza Pro"), QStringLiteral("Arial Unicode MS"), QStringLiteral("Arial")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Kohinoor Devanagari"), QStringLiteral("Devanagari Sangam MN")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Hiragino Sans"), QStringLiteral("YuGothic")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Apple SD Gothic Neo"), QStringLiteral("Arial Unicode MS")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("PingFang SC"), QStringLiteral("Hiragino Sans GB")}, systemFamily);
#elif defined(Q_OS_WIN)
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Segoe UI"), QStringLiteral("Tahoma"), QStringLiteral("Arial")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Nirmala UI"), QStringLiteral("Mangal")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo UI")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Malgun Gothic")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft JhengHei UI")}, systemFamily);
#else
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans Arabic"), QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK JP"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK KR"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Noto Sans")}, systemFamily);
#endif

    QGuiApplication::setFont(QFont{platformTextFontFamily});
    m_impl->systemText = platformTextFontFamily;
    m_impl->systemRtl = platformRtlFontFamily;
    m_impl->systemDevanagari = platformDevanagariFontFamily;
    m_impl->systemJapanese = platformJapaneseFontFamily;
    m_impl->systemKorean = platformKoreanFontFamily;
    m_impl->systemChinese = platformChineseFontFamily;
    return true;
}

const QString& FontFamilies::persian() const noexcept { return m_impl->persian; }
const QString& FontFamilies::devanagari() const noexcept { return m_impl->devanagari; }
const QString& FontFamilies::japanese() const noexcept { return m_impl->japanese; }
const QString& FontFamilies::korean() const noexcept { return m_impl->korean; }
const QString& FontFamilies::chinese() const noexcept { return m_impl->chinese; }
const QString& FontFamilies::iconSolid() const noexcept { return m_impl->iconSolid; }
const QString& FontFamilies::iconRegular() const noexcept { return m_impl->iconRegular; }
const QString& FontFamilies::systemText() const noexcept { return m_impl->systemText; }
const QString& FontFamilies::systemRtl() const noexcept { return m_impl->systemRtl; }
const QString& FontFamilies::systemDevanagari() const noexcept { return m_impl->systemDevanagari; }
const QString& FontFamilies::systemJapanese() const noexcept { return m_impl->systemJapanese; }
const QString& FontFamilies::systemKorean() const noexcept { return m_impl->systemKorean; }
const QString& FontFamilies::systemChinese() const noexcept { return m_impl->systemChinese; }
} // namespace genycaster::ui::bootstrap
