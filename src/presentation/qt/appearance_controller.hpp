/*!
 * @file        src/presentation/qt/appearance_controller.hpp
 * @brief       Qt appearance preferences controller.
 *
 * @details
 * Presents locally persisted theme and appearance choices through a
 * MOC-compatible QObject rather than coupling them to the domain.
 *
 * @author      Kambiz Asadzadeh
 * @since       29 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
  Q_PROPERTY(QString platformName READ platformName CONSTANT)
  Q_PROPERTY(QString platformAccentColor READ platformAccentColor CONSTANT)
  Q_PROPERTY(QString platformIcon READ platformIcon CONSTANT)
  Q_PROPERTY(QString customAccentColor READ customAccentColor NOTIFY appearanceChanged)
  Q_PROPERTY(bool effectiveDark READ effectiveDark NOTIFY appearanceChanged)
  Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY appearanceChanged)
  Q_PROPERTY(qreal fontScale READ fontScale NOTIFY appearanceChanged)
  Q_PROPERTY(qreal interfaceScale READ interfaceScale NOTIFY appearanceChanged)
  Q_PROPERTY(qreal socialNavigationWidth READ socialNavigationWidth NOTIFY layoutChanged)
  Q_PROPERTY(qreal socialContextWidth READ socialContextWidth NOTIFY layoutChanged)

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
  /** @brief Host OS identity for native apps or browser-reported host for WASM.
   * The browser may withhold or spoof this coarse hint; unknown hosts use Web. */
  [[nodiscard]] QString platformName() const;
  [[nodiscard]] QString platformAccentColor() const;
  [[nodiscard]] QString platformIcon() const;
  [[nodiscard]] QString customAccentColor() const;
  [[nodiscard]] bool effectiveDark() const noexcept;
  [[nodiscard]] bool reduceMotion() const noexcept;

  /** @brief Returns the persisted text scale in the supported 0.85-1.30 range. */
  [[nodiscard]] qreal fontScale() const noexcept;

  /** @brief Returns the persisted control/spacing scale in the supported 0.90-1.20 range. */
  [[nodiscard]] qreal interfaceScale() const noexcept;

  /** @brief Returns the persisted desktop navigation rail width in logical pixels. */
  [[nodiscard]] qreal socialNavigationWidth() const noexcept;

  /** @brief Returns the persisted desktop social-context rail width in logical pixels. */
  [[nodiscard]] qreal socialContextWidth() const noexcept;

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

  /** @brief Persists a text scale, clamped and quantized to the supported range. */
  Q_INVOKABLE bool setFontScale(qreal scale);

  /** @brief Persists a control/spacing scale, clamped and quantized to the supported range. */
  Q_INVOKABLE bool setInterfaceScale(qreal scale);

  /** @brief Persists the desktop navigation rail width, clamped to a usable range. */
  Q_INVOKABLE bool setSocialNavigationWidth(qreal width);

  /** @brief Persists the desktop context rail width, clamped to a usable range. */
  Q_INVOKABLE bool setSocialContextWidth(qreal width);

  /** @brief Restores the social column widths without changing other appearance preferences. */
  Q_INVOKABLE void resetSocialLayout();

  /** @brief Restores the Geny preset with System mode and normal motion. */
  Q_INVOKABLE void resetAppearance();

signals:
  void appearanceChanged();
  void layoutChanged();

private:
  void refreshEffectiveMode(bool notify = true);

  QSettings &m_settings;
  QString m_themeMode;
  QString m_colorThemeId;
  QString m_customAccentColor;
  bool m_effectiveDark{false};
  bool m_reduceMotion{false};
  qreal m_fontScale{1.0};
  qreal m_interfaceScale{1.0};
  qreal m_socialNavigationWidth{320.0};
  qreal m_socialContextWidth{420.0};
};
