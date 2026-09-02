// Textual compatibility boundary: Qt MOC requires this QObject declaration in a
// .hpp file.
#pragma once

#include <QObject>
#include <QString>

class QSettings;

/**
 * @brief Owns device-local appearance policy for the Qt presentation layer.
 *
 * The controller persists only presentation preferences. It resolves
 * System/Dark/Light against Qt's platform color-scheme hint but never exposes
 * or enables platform-native control styling. QSettings must outlive this
 * controller.
 */
class AppearanceController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString themeMode READ themeMode NOTIFY appearanceChanged)
  Q_PROPERTY(QString colorThemeId READ colorThemeId NOTIFY appearanceChanged)
  Q_PROPERTY(QString customAccentColor READ customAccentColor NOTIFY appearanceChanged)
  Q_PROPERTY(bool effectiveDark READ effectiveDark NOTIFY appearanceChanged)
  Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY appearanceChanged)

public:
  /** @brief Optional startup values for deterministic development and visual
   * verification. */
  struct StartupOverrides final {
    QString themeMode;
    QString colorThemeId;
    QString reduceMotion;
  };

  /**
   * @brief Creates an appearance adapter backed by the borrowed device-local
   * settings store.
   * @param settings Store that must outlive this controller.
   * @param overrides Optional command-line/environment overrides; empty values
   * use persistence.
   * @param parent Optional Qt owner.
   */
  explicit AppearanceController(QSettings &settings,
                                StartupOverrides overrides = {},
                                QObject *parent = nullptr);

  [[nodiscard]] QString themeMode() const;
  [[nodiscard]] QString colorThemeId() const;
  [[nodiscard]] QString customAccentColor() const;
  [[nodiscard]] bool effectiveDark() const noexcept;
  [[nodiscard]] bool reduceMotion() const noexcept;

  /** @brief Selects system, dark, or light mode. Returns false for an unknown
   * value. */
  Q_INVOKABLE bool setThemeMode(const QString &mode);
  /**
   * @brief Selects a built-in color preset or the custom accent slot.
   * @return false when themeId is not a supported preset identifier.
   */
  Q_INVOKABLE bool setColorThemeId(const QString &themeId);
  /**
   * @brief Stores an opaque custom accent color and activates the custom preset.
   * @param color CSS/Qt color text such as #FF4FA3.
   * @return false when color cannot be parsed as a valid color.
   */
  Q_INVOKABLE bool setCustomAccentColor(const QString &color);
  /** @brief Enables or disables reduced presentation motion. */
  Q_INVOKABLE void setReduceMotion(bool enabled);
  /** @brief Restores the Geny preset with System mode and normal motion. */
  Q_INVOKABLE void resetAppearance();

signals:
  void appearanceChanged();

private:
  void refreshEffectiveMode(bool notify = true);

  QSettings &m_settings;
  QString m_themeMode;
  QString m_colorThemeId;
  QString m_customAccentColor;
  bool m_effectiveDark{false};
  bool m_reduceMotion{false};
};
