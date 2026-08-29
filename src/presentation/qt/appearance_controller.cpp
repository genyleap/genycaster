#include "appearance_controller.hpp"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace {

constexpr auto kThemeModeSetting = "appearance/themeMode";
constexpr auto kColorThemeSetting = "appearance/colorTheme";
constexpr auto kReduceMotionSetting = "appearance/reduceMotion";

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
    return QStringLiteral("geny");
  }
  if (value == QStringLiteral("geny") || value == QStringLiteral("ocean") ||
      value == QStringLiteral("violet") || value == QStringLiteral("ember")) {
    return value;
  }
  return {};
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
                       QStringLiteral("geny"))
                .toString()
          : overrides.colorThemeId);
  if (m_colorThemeId.isEmpty()) {
    m_colorThemeId = QStringLiteral("geny");
  }

  m_reduceMotion = parseBooleanOverride(
      overrides.reduceMotion,
      m_settings.value(QString::fromLatin1(kReduceMotionSetting), false)
          .toBool());
  refreshEffectiveMode(false);

  if (auto *const hints = QGuiApplication::styleHints()) {
    connect(hints, &QStyleHints::colorSchemeChanged, this,
            [this] { refreshEffectiveMode(); });
  }
}

QString AppearanceController::themeMode() const { return m_themeMode; }
QString AppearanceController::colorThemeId() const { return m_colorThemeId; }
bool AppearanceController::effectiveDark() const noexcept {
  return m_effectiveDark;
}
bool AppearanceController::reduceMotion() const noexcept {
  return m_reduceMotion;
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

void AppearanceController::setReduceMotion(const bool enabled) {
  if (enabled == m_reduceMotion) {
    return;
  }
  m_reduceMotion = enabled;
  m_settings.setValue(QString::fromLatin1(kReduceMotionSetting),
                      m_reduceMotion);
  emit appearanceChanged();
}

void AppearanceController::resetAppearance() {
  const bool changed = m_themeMode != QStringLiteral("system") ||
                       m_colorThemeId != QStringLiteral("geny") ||
                       m_reduceMotion;
  m_themeMode = QStringLiteral("system");
  m_colorThemeId = QStringLiteral("geny");
  m_reduceMotion = false;
  m_settings.remove(QString::fromLatin1(kThemeModeSetting));
  m_settings.remove(QString::fromLatin1(kColorThemeSetting));
  m_settings.remove(QString::fromLatin1(kReduceMotionSetting));
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
