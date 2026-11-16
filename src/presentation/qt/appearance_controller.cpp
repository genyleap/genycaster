#include "appearance_controller.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QSettings>
#include <QSysInfo>
#include <QStyleHints>

#if defined(Q_OS_WASM)
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cmath>

namespace {

constexpr auto kThemeModeSetting = "appearance/themeMode";
constexpr auto kColorThemeSetting = "appearance/colorTheme";
constexpr auto kCustomAccentSetting = "appearance/customAccent";
constexpr auto kReduceMotionSetting = "appearance/reduceMotion";
constexpr auto kFontScaleSetting = "appearance/fontScale";
constexpr auto kInterfaceScaleSetting = "appearance/interfaceScale";
constexpr auto kSocialNavigationWidthSetting = "layout/socialNavigationWidth";
constexpr auto kSocialContextWidthSetting = "layout/socialContextWidth";
constexpr auto kDefaultCustomAccent = "#FF4FA3";

// A WebAssembly binary runs on the browser's virtual platform, not the host
// OS. Resolve only the coarse, non-identifying host platform exposed by the
// browser; unknown/obscured platforms remain Web, never fake Linux.
#if defined(Q_OS_WASM)
EM_JS(int, browserHostPlatform, (), {
  const nav = typeof navigator === 'object' ? navigator : null;
  if (!nav) return 0;
  const platform = String(nav.userAgentData?.platform || nav.platform || '');
  const agent = String(nav.userAgent || '');
  const identity = platform + ' ' + agent;
  const tabletMac = /Mac/i.test(platform) && Number(nav.maxTouchPoints || 0) > 1;
  if (/iPad|iPhone|iPod/i.test(identity) || tabletMac) return 2;
  if (/Android/i.test(identity)) return 5;
  if (/Mac/i.test(platform) || /Macintosh|Mac OS X/i.test(agent)) return 1;
  if (/Win/i.test(platform) || /Windows NT/i.test(agent)) return 3;
  if (/CrOS|Chrome OS/i.test(identity)) return 6;
  if (/Linux|X11/i.test(identity)) return 4;
  return 0;
});

[[nodiscard]] int currentBrowserHostPlatform()
{
  static const int host = browserHostPlatform();
  return host;
}
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && !defined(Q_OS_WASM)
// Match /etc/os-release ID via QSysInfo::productType(), never ID_LIKE.
// A derivative must not inherit a parent distribution's branding unless its
// own icon is explicitly mapped. Unknown IDs retain the generic Linux mark.
struct LinuxBrand final {
  const char* id;
  const char* name;
  const char* accent;
  const char* icon;
};

constexpr LinuxBrand kLinuxBrands[] = {
    {"ubuntu", "Ubuntu", "#E95420", "platforms/ubuntu"},
    {"debian", "Debian", "#D70A53", "platforms/debian"},
    {"fedora", "Fedora", "#51A2DA", "platforms/fedora"},
    {"arch", "Arch Linux", "#1793D1", "platforms/archlinux"},
    {"archlinux", "Arch Linux", "#1793D1", "platforms/archlinux"},
    {"manjaro", "Manjaro", "#35BF5C", "platforms/manjaro"},
    {"linuxmint", "Linux Mint", "#87CF3E", "platforms/linuxmint"},
    {"pop", "Pop!_OS", "#48B9C7", "platforms/popos"},
    {"popos", "Pop!_OS", "#48B9C7", "platforms/popos"},
    {"opensuse", "openSUSE", "#73BA25", "platforms/opensuse"},
    {"opensuse-tumbleweed", "openSUSE Tumbleweed", "#73BA25", "platforms/opensuse"},
    {"opensuse-leap", "openSUSE Leap", "#73BA25", "platforms/opensuse"},
    {"opensuse-microos", "openSUSE MicroOS", "#73BA25", "platforms/opensuse"},
    {"alpine", "Alpine Linux", "#0D597F", "platforms/alpinelinux"},
    {"nixos", "NixOS", "#5277C3", "platforms/nixos"},
    {"gentoo", "Gentoo", "#54487A", "platforms/gentoo"},
    {"kali", "Kali Linux", "#557C94", "platforms/kalilinux"},
    {"elementary", "elementary OS", "#64BAFF", "platforms/elementary"},
    {"elementaryos", "elementary OS", "#64BAFF", "platforms/elementary"},
    {"endeavouros", "EndeavourOS", "#7F3FBF", "platforms/endeavouros"},
    {"zorin", "Zorin OS", "#15A6F0", "platforms/zorin"},
    {"zorinos", "Zorin OS", "#15A6F0", "platforms/zorin"},
    {"rocky", "Rocky Linux", "#10B981", "platforms/rockylinux"},
    {"rockylinux", "Rocky Linux", "#10B981", "platforms/rockylinux"},
    {"almalinux", "AlmaLinux", "#0F4266", "platforms/almalinux"},
    {"centos", "CentOS", "#932279", "platforms/centos"},
    {"rhel", "Red Hat Enterprise Linux", "#EE0000", "platforms/redhat"},
    {"neon", "KDE neon", "#3DCEB5", "platforms/kdeneon"},
    {"raspbian", "Raspberry Pi OS", "#C51A4A", "platforms/raspberrypi"},
    {"raspberrypi", "Raspberry Pi OS", "#C51A4A", "platforms/raspberrypi"},
    {"slackware", "Slackware", "#2B2B2B", "platforms/slackware"},
    {"void", "Void Linux", "#478061", "platforms/voidlinux"},
    {"solus", "Solus", "#5294E2", "platforms/solus"},
    {"garuda", "Garuda Linux", "#9B4BCA", "platforms/garudalinux"},
    {"asahi", "Asahi Linux", "#BE4C7E", "platforms/asahilinux"},
    {"asahilinux", "Asahi Linux", "#BE4C7E", "platforms/asahilinux"},
};
constexpr LinuxBrand kGenericLinuxBrand{"linux", "Linux", "#3584E4", "platforms/linux"};

[[nodiscard]] const LinuxBrand& currentLinuxBrand() {
  const QString id = QSysInfo::productType().trimmed().toLower();
  for (const auto& entry : kLinuxBrands) {
    if (id == QString::fromLatin1(entry.id)) return entry;
  }
  return kGenericLinuxBrand;
}
#endif

[[nodiscard]] QString canonicalThemeMode(QString value) {
  value = value.trimmed().toLower();
  if (value.isEmpty()) {
    return QStringLiteral("system");
  }
  if (value == QStringLiteral("system") || value == QStringLiteral("dark") ||
      value == QStringLiteral("light")) {
    return value;
  }
  return {};
}

[[nodiscard]] QString canonicalColorTheme(QString value) {
  value = value.trimmed().toLower();
  if (value.isEmpty()) {
    return QStringLiteral("platform");
  }
  if (value == QStringLiteral("platform") || value == QStringLiteral("geny") || value == QStringLiteral("ocean") ||
      value == QStringLiteral("violet") || value == QStringLiteral("ember") ||
      value == QStringLiteral("rose") || value == QStringLiteral("mint") ||
      value == QStringLiteral("mono") || value == QStringLiteral("custom")) {
    return value;
  }
  return {};
}

[[nodiscard]] QString canonicalAccentColor(const QString &value) {
  QColor color{value.trimmed()};
  if (!color.isValid()) {
    return {};
  }
  color.setAlpha(255);
  return color.name(QColor::HexRgb).toUpper();
}

[[nodiscard]] bool parseBooleanOverride(const QString &value,
                                        const bool fallback) {
  if (value.isEmpty()) {
    return fallback;
  }
  const QString canonical = value.trimmed().toLower();
  if (canonical == QStringLiteral("1") || canonical == QStringLiteral("true") ||
      canonical == QStringLiteral("yes") || canonical == QStringLiteral("on")) {
    return true;
  }
  if (canonical == QStringLiteral("0") ||
      canonical == QStringLiteral("false") ||
      canonical == QStringLiteral("no") || canonical == QStringLiteral("off")) {
    return false;
  }
  return fallback;
}

[[nodiscard]] qreal canonicalPanelWidth(const qreal value, const qreal fallback,
                                        const qreal minimum, const qreal maximum) {
  if (!std::isfinite(value)) {
    return fallback;
  }
  return std::round(std::clamp(value, minimum, maximum));
}

[[nodiscard]] qreal canonicalScale(const qreal value, const qreal minimum,
                                   const qreal maximum) {
  if (!std::isfinite(value)) {
    return 1.0;
  }
  const qreal clamped = std::clamp(value, minimum, maximum);
  return std::round(clamped * 20.0) / 20.0;
}

} // namespace

AppearanceController::AppearanceController(QSettings &settings,
                                           StartupOverrides overrides,
                                           QObject *parent)
    : QObject{parent}, m_settings{settings} {
  m_themeMode = canonicalThemeMode(
      overrides.themeMode.isEmpty()
          ? m_settings
                .value(QString::fromLatin1(kThemeModeSetting),
                       QStringLiteral("system"))
                .toString()
          : overrides.themeMode);
  if (m_themeMode.isEmpty()) {
    m_themeMode = QStringLiteral("system");
  }

  m_colorThemeId = canonicalColorTheme(
      overrides.colorThemeId.isEmpty()
          ? m_settings
                .value(QString::fromLatin1(kColorThemeSetting),
                       QStringLiteral("platform"))
                .toString()
          : overrides.colorThemeId);
  if (m_colorThemeId.isEmpty()) {
    m_colorThemeId = QStringLiteral("platform");
  }

  m_customAccentColor = canonicalAccentColor(
      m_settings
          .value(QString::fromLatin1(kCustomAccentSetting),
                 QString::fromLatin1(kDefaultCustomAccent))
          .toString());
  if (m_customAccentColor.isEmpty()) {
    m_customAccentColor = QString::fromLatin1(kDefaultCustomAccent);
  }

  m_reduceMotion = parseBooleanOverride(
      overrides.reduceMotion,
      m_settings.value(QString::fromLatin1(kReduceMotionSetting), false)
          .toBool());
  m_fontScale = canonicalScale(
      m_settings.value(QString::fromLatin1(kFontScaleSetting), 1.0).toReal(),
      0.85, 1.30);
  m_interfaceScale = canonicalScale(
      m_settings.value(QString::fromLatin1(kInterfaceScaleSetting), 1.0).toReal(),
      0.90, 1.20);
  m_socialNavigationWidth = canonicalPanelWidth(
      m_settings.value(QString::fromLatin1(kSocialNavigationWidthSetting), 320.0).toReal(),
      320.0, 264.0, 460.0);
  m_socialContextWidth = canonicalPanelWidth(
      m_settings.value(QString::fromLatin1(kSocialContextWidthSetting), 420.0).toReal(),
      420.0, 304.0, 620.0);
  refreshEffectiveMode(false);

  if (auto *const hints = QGuiApplication::styleHints()) {
    connect(hints, &QStyleHints::colorSchemeChanged, this,
            [this] { refreshEffectiveMode(); });
  }
}

QString AppearanceController::themeMode() const { return m_themeMode; }
QString AppearanceController::colorThemeId() const { return m_colorThemeId; }
QString AppearanceController::platformName() const {
#if defined(Q_OS_WASM)
  switch (currentBrowserHostPlatform()) {
    case 1: return QStringLiteral("macOS");
    case 2: return QStringLiteral("iOS / iPadOS");
    case 3: return QStringLiteral("Windows");
    case 4: return QStringLiteral("Linux");
    case 5: return QStringLiteral("Android");
    case 6: return QStringLiteral("ChromeOS");
    default: return QStringLiteral("Web");
  }
#elif defined(Q_OS_ANDROID)
  return QStringLiteral("Android");
#elif defined(Q_OS_IOS)
  return QStringLiteral("iOS");
#elif defined(Q_OS_MACOS)
  return QStringLiteral("macOS");
#elif defined(Q_OS_WIN)
  return QStringLiteral("Windows");
#elif defined(Q_OS_LINUX)
  return QString::fromLatin1(currentLinuxBrand().name);
#else
  return QStringLiteral("GenyCaster");
#endif
}
QString AppearanceController::platformAccentColor() const {
#if defined(Q_OS_WASM)
  switch (currentBrowserHostPlatform()) {
    case 1: case 2: return QStringLiteral("#007AFF");
    case 3: return QStringLiteral("#0078D4");
    case 4: return QStringLiteral("#3584E4");
    case 5: return QStringLiteral("#3DDC84");
    default: return QStringLiteral("#315CF5");
  }
#elif defined(Q_OS_ANDROID)
  return QStringLiteral("#3DDC84");
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
  return QStringLiteral("#007AFF");
#elif defined(Q_OS_WIN)
  return QStringLiteral("#0078D4");
#elif defined(Q_OS_LINUX)
  return QString::fromLatin1(currentLinuxBrand().accent);
#else
  return QStringLiteral("#315CF5");
#endif
}
QString AppearanceController::platformIcon() const {
#if defined(Q_OS_WASM)
  switch (currentBrowserHostPlatform()) {
    case 1: return QStringLiteral("platforms/macos");
    case 2: return QStringLiteral("platforms/ios");
    case 3: return QStringLiteral("platforms/windows");
    case 4: return QStringLiteral("platforms/linux");
    case 5: return QStringLiteral("platforms/android");
    default: return QStringLiteral("globe");
  }
#elif defined(Q_OS_ANDROID)
  return QStringLiteral("platforms/android");
#elif defined(Q_OS_IOS)
  return QStringLiteral("platforms/ios");
#elif defined(Q_OS_MACOS)
  return QStringLiteral("platforms/macos");
#elif defined(Q_OS_WIN)
  return QStringLiteral("platforms/windows");
#elif defined(Q_OS_LINUX)
  return QString::fromLatin1(currentLinuxBrand().icon);
#else
  return QStringLiteral("globe");
#endif
}
QString AppearanceController::customAccentColor() const {
  return m_customAccentColor;
}
bool AppearanceController::effectiveDark() const noexcept {
  return m_effectiveDark;
}
bool AppearanceController::reduceMotion() const noexcept {
  return m_reduceMotion;
}
qreal AppearanceController::fontScale() const noexcept { return m_fontScale; }
qreal AppearanceController::interfaceScale() const noexcept {
  return m_interfaceScale;
}
qreal AppearanceController::socialNavigationWidth() const noexcept {
  return m_socialNavigationWidth;
}
qreal AppearanceController::socialContextWidth() const noexcept {
  return m_socialContextWidth;
}

bool AppearanceController::setThemeMode(const QString &mode) {
  const QString canonical = canonicalThemeMode(mode);
  if (canonical.isEmpty()) {
    return false;
  }
  if (canonical == m_themeMode) {
    return true;
  }
  m_themeMode = canonical;
  m_settings.setValue(QString::fromLatin1(kThemeModeSetting), m_themeMode);
  refreshEffectiveMode(false);
  emit themeModeChanged();
  emit appearanceChanged();
  return true;
}

bool AppearanceController::setColorThemeId(const QString &themeId) {
  const QString canonical = canonicalColorTheme(themeId);
  if (canonical.isEmpty()) {
    return false;
  }
  if (canonical == m_colorThemeId) {
    return true;
  }
  m_colorThemeId = canonical;
  m_settings.setValue(QString::fromLatin1(kColorThemeSetting), m_colorThemeId);
  emit colorThemeIdChanged();
  emit appearanceChanged();
  return true;
}

bool AppearanceController::setCustomAccentColor(const QString &color) {
  const QString canonical = canonicalAccentColor(color);
  if (canonical.isEmpty()) {
    return false;
  }
  const bool colorChanged = canonical != m_customAccentColor;
  const bool presetChanged = m_colorThemeId != QStringLiteral("custom");
  m_customAccentColor = canonical;
  m_colorThemeId = QStringLiteral("custom");
  m_settings.setValue(QString::fromLatin1(kCustomAccentSetting),
                      m_customAccentColor);
  m_settings.setValue(QString::fromLatin1(kColorThemeSetting), m_colorThemeId);
  if (colorChanged) {
    emit customAccentColorChanged();
  }
  if (presetChanged) {
    emit colorThemeIdChanged();
  }
  if (colorChanged || presetChanged) {
    emit appearanceChanged();
  }
  return true;
}

void AppearanceController::setReduceMotion(const bool enabled) {
  if (enabled == m_reduceMotion) {
    return;
  }
  m_reduceMotion = enabled;
  m_settings.setValue(QString::fromLatin1(kReduceMotionSetting),
                      m_reduceMotion);
  emit reduceMotionChanged();
  emit appearanceChanged();
}

bool AppearanceController::setFontScale(const qreal scale) {
  if (!std::isfinite(scale)) {
    return false;
  }
  const qreal canonical = canonicalScale(scale, 0.85, 1.30);
  if (qFuzzyCompare(canonical, m_fontScale)) {
    return true;
  }
  m_fontScale = canonical;
  m_settings.setValue(QString::fromLatin1(kFontScaleSetting), m_fontScale);
  emit fontScaleChanged();
  emit appearanceChanged();
  return true;
}

bool AppearanceController::setInterfaceScale(const qreal scale) {
  if (!std::isfinite(scale)) {
    return false;
  }
  const qreal canonical = canonicalScale(scale, 0.90, 1.20);
  if (qFuzzyCompare(canonical, m_interfaceScale)) {
    return true;
  }
  m_interfaceScale = canonical;
  m_settings.setValue(QString::fromLatin1(kInterfaceScaleSetting), m_interfaceScale);
  emit interfaceScaleChanged();
  emit appearanceChanged();
  return true;
}

bool AppearanceController::setSocialNavigationWidth(const qreal width) {
  const qreal canonical = canonicalPanelWidth(width, m_socialNavigationWidth, 264.0, 460.0);
  if (qFuzzyCompare(canonical, m_socialNavigationWidth)) {
    return true;
  }
  m_socialNavigationWidth = canonical;
  m_settings.setValue(QString::fromLatin1(kSocialNavigationWidthSetting), m_socialNavigationWidth);
  emit layoutChanged();
  return true;
}

bool AppearanceController::setSocialContextWidth(const qreal width) {
  const qreal canonical = canonicalPanelWidth(width, m_socialContextWidth, 304.0, 620.0);
  if (qFuzzyCompare(canonical, m_socialContextWidth)) {
    return true;
  }
  m_socialContextWidth = canonical;
  m_settings.setValue(QString::fromLatin1(kSocialContextWidthSetting), m_socialContextWidth);
  emit layoutChanged();
  return true;
}

void AppearanceController::resetSocialLayout() {
  const bool changed = !qFuzzyCompare(m_socialNavigationWidth, 320.0)
      || !qFuzzyCompare(m_socialContextWidth, 420.0);
  m_socialNavigationWidth = 320.0;
  m_socialContextWidth = 420.0;
  m_settings.remove(QString::fromLatin1(kSocialNavigationWidthSetting));
  m_settings.remove(QString::fromLatin1(kSocialContextWidthSetting));
  if (changed) {
    emit layoutChanged();
  }
}

void AppearanceController::resetAppearance() {
  const bool modeChanged = m_themeMode != QStringLiteral("system");
  const bool presetChanged = m_colorThemeId != QStringLiteral("platform");
  const bool colorChanged = m_customAccentColor != QString::fromLatin1(kDefaultCustomAccent);
  const bool motionChanged = m_reduceMotion;
  const bool textScaleChanged = !qFuzzyCompare(m_fontScale, 1.0);
  const bool uiScaleChanged = !qFuzzyCompare(m_interfaceScale, 1.0);

  m_themeMode = QStringLiteral("system");
  m_colorThemeId = QStringLiteral("platform");
  m_customAccentColor = QString::fromLatin1(kDefaultCustomAccent);
  m_reduceMotion = false;
  m_fontScale = 1.0;
  m_interfaceScale = 1.0;
  m_settings.remove(QString::fromLatin1(kThemeModeSetting));
  m_settings.remove(QString::fromLatin1(kColorThemeSetting));
  m_settings.remove(QString::fromLatin1(kCustomAccentSetting));
  m_settings.remove(QString::fromLatin1(kReduceMotionSetting));
  m_settings.remove(QString::fromLatin1(kFontScaleSetting));
  m_settings.remove(QString::fromLatin1(kInterfaceScaleSetting));

  // A system appearance change may also change effectiveDark; it emits its
  // own precise NOTIFY without invalidating every unrelated QML property.
  const bool previousEffectiveDark = m_effectiveDark;
  refreshEffectiveMode(false);
  if (modeChanged) {
    emit themeModeChanged();
  }
  if (presetChanged) {
    emit colorThemeIdChanged();
  }
  if (colorChanged) {
    emit customAccentColorChanged();
  }
  if (motionChanged) {
    emit reduceMotionChanged();
  }
  if (textScaleChanged) {
    emit fontScaleChanged();
  }
  if (uiScaleChanged) {
    emit interfaceScaleChanged();
  }
  if (modeChanged || presetChanged || colorChanged || motionChanged ||
      textScaleChanged || uiScaleChanged || m_effectiveDark != previousEffectiveDark) {
    emit appearanceChanged();
  }
}

void AppearanceController::refreshEffectiveMode(const bool notify) {
  const bool resolved =
      m_themeMode == QStringLiteral("dark") ||
      (m_themeMode == QStringLiteral("system") &&
       QGuiApplication::styleHints() &&
       QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
  if (resolved == m_effectiveDark) {
    return;
  }
  m_effectiveDark = resolved;
  emit effectiveDarkChanged();
  if (notify) {
    emit appearanceChanged();
  }
}
