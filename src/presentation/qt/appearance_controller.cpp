#include "appearance_controller.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QSettings>
#include <QSysInfo>
#include <QStyleHints>

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
#if defined(Q_OS_ANDROID)
  return QStringLiteral("Android");
#elif defined(Q_OS_IOS)
  return QStringLiteral("iOS");
#elif defined(Q_OS_MACOS)
  return QStringLiteral("macOS");
#elif defined(Q_OS_WIN)
  return QStringLiteral("Windows");
#elif defined(Q_OS_LINUX)
  const QString distro = QSysInfo::productType().toLower();
  if (distro == QStringLiteral("ubuntu")) return QStringLiteral("Ubuntu");
  if (distro == QStringLiteral("fedora")) return QStringLiteral("Fedora");
  if (distro == QStringLiteral("debian")) return QStringLiteral("Debian");
  return QStringLiteral("Linux");
#elif defined(Q_OS_WASM)
  return QStringLiteral("Web");
#else
  return QStringLiteral("GenyCaster");
#endif
}
QString AppearanceController::platformAccentColor() const {
#if defined(Q_OS_ANDROID)
  return QStringLiteral("#3DDC84");
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
  return QStringLiteral("#007AFF");
#elif defined(Q_OS_WIN)
  return QStringLiteral("#0078D4");
#elif defined(Q_OS_LINUX)
  const QString distro = QSysInfo::productType().toLower();
  if (distro == QStringLiteral("ubuntu")) return QStringLiteral("#E95420");
  if (distro == QStringLiteral("fedora")) return QStringLiteral("#3C6EB4");
  if (distro == QStringLiteral("debian")) return QStringLiteral("#D70A53");
  return QStringLiteral("#3584E4");
#else
  return QStringLiteral("#315CF5");
#endif
}
QString AppearanceController::platformIcon() const {
#if defined(Q_OS_ANDROID)
  return QStringLiteral("platforms/android");
#elif defined(Q_OS_IOS)
  return QStringLiteral("platforms/ios");
#elif defined(Q_OS_MACOS)
  return QStringLiteral("platforms/macos");
#elif defined(Q_OS_WIN)
  return QStringLiteral("platforms/windows");
#elif defined(Q_OS_LINUX)
  if (QSysInfo::productType().compare(QStringLiteral("ubuntu"), Qt::CaseInsensitive) == 0)
    return QStringLiteral("platforms/ubuntu");
  return QStringLiteral("platforms/linux");
#else
  return QStringLiteral("platforms/linux");
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
  emit appearanceChanged();
  return true;
}

bool AppearanceController::setCustomAccentColor(const QString &color) {
  const QString canonical = canonicalAccentColor(color);
  if (canonical.isEmpty()) {
    return false;
  }
  const bool changed = canonical != m_customAccentColor ||
                       m_colorThemeId != QStringLiteral("custom");
  m_customAccentColor = canonical;
  m_colorThemeId = QStringLiteral("custom");
  m_settings.setValue(QString::fromLatin1(kCustomAccentSetting),
                      m_customAccentColor);
  m_settings.setValue(QString::fromLatin1(kColorThemeSetting), m_colorThemeId);
  if (changed) {
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
  const bool changed = m_themeMode != QStringLiteral("system") ||
                       m_colorThemeId != QStringLiteral("platform") ||
                       m_customAccentColor != QString::fromLatin1(kDefaultCustomAccent) ||
                       m_reduceMotion || !qFuzzyCompare(m_fontScale, 1.0) ||
                       !qFuzzyCompare(m_interfaceScale, 1.0);
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
  refreshEffectiveMode(false);
  if (changed) {
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
  if (notify) {
    emit appearanceChanged();
  }
}
